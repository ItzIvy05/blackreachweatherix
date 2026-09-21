namespace
{
	bool RegionRejects(RE::TESRegion* a_region, RE::TESWeather* a_weather)
	{
		static REL::Relocation<bool (*)(RE::TESRegion*, RE::TESWeather*)> hasWeather{ RELOCATION_ID(16204, 16450) };
		if (!a_region->dataList || hasWeather(a_region, a_weather)) {
			return false;
		}
		for (const auto& data : a_region->dataList->regionDataList) {
			if (data && data->GetType() == RE::TESRegionData::Type::kWeather) {
				return data->IsLoaded();
			}
		}
		return false;
	}

	bool NeedsResync(RE::Sky* a_sky)
	{
		if (!a_sky->region || !a_sky->currentWeather || a_sky->overrideWeather) {
			return false;
		}
		return RegionRejects(a_sky->region, a_sky->currentWeather);
	}

	bool ScreenIsCovered()
	{
		const auto ui = RE::UI::GetSingleton();
		return ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
	}

	struct SkyUpdateWeather
	{
		static void thunk(RE::Sky* a_sky)
		{
			const auto resync = a_sky && NeedsResync(a_sky);
			const auto hidden = resync && ScreenIsCovered();
			const auto borrowFastTravel = hidden && a_sky->flags.none(RE::Sky::Flags::kFastTravel);
			if (resync && (hidden || !a_sky->lastWeather)) {
				a_sky->region = nullptr;
			}
			if (borrowFastTravel) {
				a_sky->flags.set(RE::Sky::Flags::kFastTravel);
			}
			func(a_sky);
			if (borrowFastTravel) {
				a_sky->flags.reset(RE::Sky::Flags::kFastTravel);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse);
	const REL::Relocation<std::uintptr_t> update{ RELOCATION_ID(25682, 26229) };
	const auto target = REL::Relocation<std::uintptr_t>{ RELOCATION_ID(25684, 26231) }.address();
	std::uintptr_t site = 0;
	for (auto at = update.address(); !site && at < update.address() + 0x1000; ++at) {
		if (*reinterpret_cast<const std::uint8_t*>(at) != 0xE8) {
			continue;
		}
		if (static_cast<std::uintptr_t>(static_cast<std::intptr_t>(at + 5) + *reinterpret_cast<const std::int32_t*>(at + 1)) == target) {
			site = at;
		}
	}
	logger::info("Sky::Update {:X}, UpdateWeather call site {:X}", update.address(), site);
	if (!site) {
		return true;
	}
	SKSE::AllocTrampoline(64);
	SkyUpdateWeather::func = SKSE::GetTrampoline().write_call<5>(site, SkyUpdateWeather::thunk);
	return true;
}
