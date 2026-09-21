namespace
{
	std::atomic<int> framesAfterLoad{ 0 };

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

	struct LoadingScreenWatcher : RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event->menuName == RE::LoadingMenu::MENU_NAME) {
				framesAfterLoad = 10;
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	LoadingScreenWatcher loadingScreenWatcher;

	struct SkyUpdateWeather
	{
		static void thunk(RE::Sky* a_sky)
		{
			if (framesAfterLoad > 0) {
				--framesAfterLoad;
				if (NeedsResync(a_sky)) {
					a_sky->region = nullptr;
					a_sky->flags.set(RE::Sky::Flags::kFastTravel);
				}
			}
			func(a_sky);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
			RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&loadingScreenWatcher);
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse);
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
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
