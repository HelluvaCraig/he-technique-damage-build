#include "pch.h"
#include "PrismaUI_API.h"
#include "StoneMap.h"

using namespace std::chrono_literals;

namespace
{
    constexpr auto kStonePlugin = "HE Elden Rim - Ash Rings.esp";
    constexpr auto kCooldownPlugin = "HE Elden Rim - Ash Cooldown.esp";
    constexpr auto kNewArmouryPlugin = "NewArmoury.esp";
    constexpr auto kSkyrimPlugin = "Skyrim.esm";
    constexpr auto kViewPath = "HE_TechniqueHUD/index.html";
    constexpr auto kDefaultIniRelativePath = "Data\\SKSE\\Plugins\\HE_TechniqueHUD.ini";
    constexpr auto kUserIniRelativePath = "Data\\SKSE\\Plugins\\HE_TechniqueHUD.user.ini";

    inline constexpr std::array<RE::FormID, 3> kCooldownMGEFLocalIDs = { 0x804, 0x805, 0x806 };
    inline constexpr std::array<RE::FormID, 3> kAlterationProbeLocalIDs = { 0x5AD5C, 0x5AD5D, 0x51B16 };

    PRISMA_UI_API::IVPrismaUI1* g_prisma = nullptr;
    PrismaView g_view = 0;
    std::atomic_bool g_domReady{ false };
    std::jthread g_pollThread;

    std::array<RE::EffectSetting*, 3> g_cooldownMGEFs{};
    std::array<RE::SpellItem*, 3> g_alterationProbes{};
    std::unordered_map<RE::FormID, const StoneDefinition*> g_definitionByFullFormID;
    std::array<RE::BGSKeyword*, 3> g_colossusKeywords{};

    std::filesystem::path g_defaultIniPath;
    std::filesystem::path g_userIniPath;
    std::string g_defaultIniPathString;
    std::string g_userIniPathString;

    struct Config
    {
        int left = 22;
        int bottom = 112;
        int size = 72;
        int pollMs = 50;
        int menuKey = 68;
    } g_config;

    struct HUDState
    {
        bool equipped = false;
        std::uint8_t tier = 0;
        bool cooling = false;
        bool conditionsValid = false;
        float progress = 0.0f;
    };

    HUDState g_lastState{};
    bool g_hasLastState = false;
    const StoneDefinition* g_cachedDefinition = nullptr;
    std::chrono::steady_clock::time_point g_nextStoneScan{};
    bool g_settingsOpen = false;
    bool g_inputRegistered = false;

    void ResolveConfigPaths()
    {
        if (!g_defaultIniPathString.empty() && !g_userIniPathString.empty()) {
            return;
        }

        std::array<char, 32768> modulePath{};
        const auto length = GetModuleFileNameA(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));

        std::filesystem::path gameRoot;
        if (length > 0 && length < modulePath.size()) {
            gameRoot = std::filesystem::path(std::string(modulePath.data(), length)).parent_path();
        } else {
            gameRoot = std::filesystem::current_path();
        }

        g_defaultIniPath = gameRoot / kDefaultIniRelativePath;
        g_userIniPath = gameRoot / kUserIniRelativePath;
        g_defaultIniPathString = g_defaultIniPath.string();
        g_userIniPathString = g_userIniPath.string();
    }

    void LoadConfig()
    {
        ResolveConfigPaths();

        g_config.left = static_cast<int>(GetPrivateProfileIntA("HUD", "Left", g_config.left, g_defaultIniPathString.c_str()));
        g_config.bottom = static_cast<int>(GetPrivateProfileIntA("HUD", "Bottom", g_config.bottom, g_defaultIniPathString.c_str()));
        g_config.size = static_cast<int>(GetPrivateProfileIntA("HUD", "Size", g_config.size, g_defaultIniPathString.c_str()));
        g_config.pollMs = static_cast<int>(GetPrivateProfileIntA("HUD", "PollMs", g_config.pollMs, g_defaultIniPathString.c_str()));
        g_config.menuKey = static_cast<int>(GetPrivateProfileIntA("HUD", "MenuKey", g_config.menuKey, g_defaultIniPathString.c_str()));

        g_config.left = static_cast<int>(GetPrivateProfileIntA("HUD", "Left", g_config.left, g_userIniPathString.c_str()));
        g_config.bottom = static_cast<int>(GetPrivateProfileIntA("HUD", "Bottom", g_config.bottom, g_userIniPathString.c_str()));
        g_config.size = static_cast<int>(GetPrivateProfileIntA("HUD", "Size", g_config.size, g_userIniPathString.c_str()));
        g_config.pollMs = static_cast<int>(GetPrivateProfileIntA("HUD", "PollMs", g_config.pollMs, g_userIniPathString.c_str()));
        g_config.menuKey = static_cast<int>(GetPrivateProfileIntA("HUD", "MenuKey", g_config.menuKey, g_userIniPathString.c_str()));

        g_config.left = std::clamp(g_config.left, 0, 4000);
        g_config.bottom = std::clamp(g_config.bottom, 0, 4000);
        g_config.size = std::clamp(g_config.size, 40, 200);
        g_config.pollMs = std::clamp(g_config.pollMs, 33, 250);
        g_config.menuKey = std::clamp(g_config.menuKey, 0, 255);

        SKSE::log::info("Loaded HUD layout Left={} Bottom={} Size={}", g_config.left, g_config.bottom, g_config.size);
    }

    void SaveLayoutConfig()
    {
        ResolveConfigPaths();

        std::error_code ec;
        std::filesystem::create_directories(g_userIniPath.parent_path(), ec);
        if (ec) {
            SKSE::log::error("Could not create HUD config directory: {}", ec.message());
            return;
        }

        const auto left = std::to_string(g_config.left);
        const auto bottom = std::to_string(g_config.bottom);
        const auto size = std::to_string(g_config.size);

        WritePrivateProfileStringA("HUD", "Left", left.c_str(), g_userIniPathString.c_str());
        WritePrivateProfileStringA("HUD", "Bottom", bottom.c_str(), g_userIniPathString.c_str());
        WritePrivateProfileStringA("HUD", "Size", size.c_str(), g_userIniPathString.c_str());
        WritePrivateProfileStringA(nullptr, nullptr, nullptr, g_userIniPathString.c_str());
    }

    std::uint8_t GetTechniqueTier(const StoneDefinition& a_def)
    {
        if (a_def.category == Category::None || a_def.resourceCost <= 0.0f) {
            return 0;
        }
        if (a_def.resourceCost < 55.0f) {
            return 1;
        }
        if (a_def.resourceCost < 90.0f) {
            return 2;
        }
        return 3;
    }

    void ResolveForms()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            SKSE::log::error("TESDataHandler unavailable");
            return;
        }

        g_definitionByFullFormID.clear();
        std::size_t stonesResolved = 0;
        for (const auto& def : kStoneDefinitions) {
            if (def.category == Category::None) {
                continue;
            }
            if (auto* stone = dataHandler->LookupForm<RE::TESObjectARMO>(def.localFormID, kStonePlugin)) {
                g_definitionByFullFormID.emplace(stone->GetFormID(), std::addressof(def));
                ++stonesResolved;
            }
        }

        std::size_t cooldownResolved = 0;
        for (std::size_t i = 0; i < kCooldownMGEFLocalIDs.size(); ++i) {
            g_cooldownMGEFs[i] = dataHandler->LookupForm<RE::EffectSetting>(kCooldownMGEFLocalIDs[i], kCooldownPlugin);
            if (g_cooldownMGEFs[i]) {
                g_cooldownMGEFs[i]->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kHideInUI);
                ++cooldownResolved;
            }
        }

        std::size_t probeResolved = 0;
        for (std::size_t i = 0; i < kAlterationProbeLocalIDs.size(); ++i) {
            g_alterationProbes[i] = dataHandler->LookupForm<RE::SpellItem>(kAlterationProbeLocalIDs[i], kSkyrimPlugin);
            if (g_alterationProbes[i]) {
                ++probeResolved;
            }
        }

        g_colossusKeywords[0] = dataHandler->LookupForm<RE::BGSKeyword>(0xE457E, kNewArmouryPlugin);
        g_colossusKeywords[1] = dataHandler->LookupForm<RE::BGSKeyword>(0xE457F, kNewArmouryPlugin);
        g_colossusKeywords[2] = dataHandler->LookupForm<RE::BGSKeyword>(0xE4580, kNewArmouryPlugin);

        SKSE::log::info("Resolved {}/77 active Stones, {}/3 cooldown blockers, {}/3 Alteration probes",
            stonesResolved, cooldownResolved, probeResolved);
    }

    const StoneDefinition* GetEquippedDefinition(RE::PlayerCharacter* a_player)
    {
        if (!a_player || g_definitionByFullFormID.empty()) {
            return nullptr;
        }

        const auto inventory = a_player->GetInventory([](RE::TESBoundObject& a_object) {
            return a_object.IsArmor();
        });

        for (const auto& [item, data] : inventory) {
            const auto& [count, entry] = data;
            if (count <= 0 || !entry || !entry->IsWorn()) {
                continue;
            }
            if (const auto it = g_definitionByFullFormID.find(item->GetFormID()); it != g_definitionByFullFormID.end()) {
                return it->second;
            }
        }
        return nullptr;
    }

    int GetWeaponTypeCode(RE::TESForm* a_form)
    {
        auto* weapon = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
        return weapon ? static_cast<int>(weapon->GetWeaponType()) : 0;
    }

    bool IsOneHanded(RE::TESForm* a_form)
    {
        const int type = GetWeaponTypeCode(a_form);
        return type >= 1 && type <= 4;
    }

    bool IsTwoHanded(RE::TESForm* a_form)
    {
        const int type = GetWeaponTypeCode(a_form);
        return type == 5 || type == 6;
    }

    bool IsBow(RE::TESForm* a_form)
    {
        auto* weapon = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
        return weapon && weapon->IsBow() && !weapon->IsCrossbow();
    }

    bool IsCrossbow(RE::TESForm* a_form)
    {
        auto* weapon = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
        return weapon && weapon->IsCrossbow();
    }

    bool IsShield(RE::TESForm* a_form)
    {
        auto* armor = a_form ? a_form->As<RE::TESObjectARMO>() : nullptr;
        return armor && armor->IsShield();
    }

    bool IsColossusWeapon(RE::TESForm* a_form)
    {
        auto* weapon = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
        if (!weapon) {
            return false;
        }
        if (static_cast<int>(weapon->GetWeaponType()) == 6) {
            return true;
        }
        for (auto* keyword : g_colossusKeywords) {
            if (keyword && weapon->HasKeyword(keyword)) {
                return true;
            }
        }
        return false;
    }

    bool EquipmentConditionMet(RE::PlayerCharacter* a_player, EquipRule a_rule)
    {
        if (!a_player) {
            return false;
        }

        auto* right = a_player->GetEquippedObject(false);
        auto* left = a_player->GetEquippedObject(true);

        switch (a_rule) {
        case EquipRule::Any:
            return true;
        case EquipRule::DualWield1H:
            return IsOneHanded(right) && IsOneHanded(left);
        case EquipRule::TwoHanded:
            return IsTwoHanded(right);
        case EquipRule::NotTwoHanded:
            return !IsTwoHanded(right);
        case EquipRule::ShieldLeft:
            return IsShield(left);
        case EquipRule::Colossus:
            return IsColossusWeapon(right);
        case EquipRule::Bow:
            return IsBow(right);
        case EquipRule::Crossbow:
            return IsCrossbow(right);
        case EquipRule::OneHanded:
            return IsOneHanded(right);
        case EquipRule::OneHandedOrCrossbow:
            return IsOneHanded(right) || IsCrossbow(right);
        default:
            return false;
        }
    }

    float CalculateAdjustedMagicCost(RE::PlayerCharacter* a_player, const StoneDefinition& a_def, std::uint8_t a_tier)
    {
        if (!a_player || a_tier < 1 || a_tier > 3) {
            return a_def.resourceCost;
        }

        auto* probe = g_alterationProbes[a_tier - 1];
        if (!probe) {
            return a_def.resourceCost;
        }

        const auto oldCostOverride = probe->data.costOverride;
        const auto oldFlags = probe->data.flags;

        probe->data.costOverride = static_cast<std::int32_t>(std::lround(a_def.resourceCost));
        probe->data.flags.set(RE::SpellItem::SpellFlag::kCostOverride);
        const float adjusted = probe->CalculateMagickaCost(a_player);

        probe->data.costOverride = oldCostOverride;
        probe->data.flags = oldFlags;

        if (!std::isfinite(adjusted) || adjusted < 0.0f) {
            return a_def.resourceCost;
        }
        return adjusted;
    }

    bool ResourceConditionMet(RE::PlayerCharacter* a_player, const StoneDefinition& a_def, std::uint8_t a_tier)
    {
        if (!a_player) {
            return false;
        }

        auto* avOwner = a_player->AsActorValueOwner();
        if (!avOwner) {
            return false;
        }

        if (a_def.resource == Resource::Magicka) {
            const float needed = CalculateAdjustedMagicCost(a_player, a_def, a_tier);
            return avOwner->GetActorValue(RE::ActorValue::kMagicka) + 0.001f >= needed;
        }

        return avOwner->GetActorValue(RE::ActorValue::kStamina) + 0.001f >= a_def.resourceCost;
    }

    bool TechniqueConditionsMet(RE::PlayerCharacter* a_player, const StoneDefinition& a_def, std::uint8_t a_tier)
    {
        return EquipmentConditionMet(a_player, a_def.equipRule) && ResourceConditionMet(a_player, a_def, a_tier);
    }

    std::pair<bool, float> GetCooldownState(RE::PlayerCharacter* a_player, std::uint8_t a_tier)
    {
        if (!a_player || a_tier < 1 || a_tier > 3) {
            return { false, 1.0f };
        }

        auto* cooldownMGEF = g_cooldownMGEFs[a_tier - 1];
        if (!cooldownMGEF) {
            return { false, 1.0f };
        }

        auto* magicTarget = a_player->AsMagicTarget();
        auto* effects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!effects) {
            return { false, 1.0f };
        }

        for (auto* effect : *effects) {
            if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
                continue;
            }
            if (effect->GetBaseObject() != cooldownMGEF || effect->duration <= 0.0f) {
                continue;
            }

            const float progress = std::clamp(effect->elapsedSeconds / effect->duration, 0.0f, 1.0f);
            return { true, progress };
        }
        return { false, 1.0f };
    }

    bool MeaningfullyChanged(const HUDState& a_state)
    {
        if (!g_hasLastState) {
            return true;
        }
        if (a_state.equipped != g_lastState.equipped ||
            a_state.tier != g_lastState.tier ||
            a_state.cooling != g_lastState.cooling ||
            a_state.conditionsValid != g_lastState.conditionsValid) {
            return true;
        }
        return std::fabs(a_state.progress - g_lastState.progress) >= 0.0025f;
    }

    void PushState(const HUDState& a_state)
    {
        if (!g_prisma || !g_domReady.load() || !g_view || !g_prisma->IsValid(g_view) || !MeaningfullyChanged(a_state)) {
            return;
        }

        const auto script = std::format(
            "window.TechniqueHUD&&window.TechniqueHUD.setState({{equipped:{},tier:{},cooling:{},conditionsValid:{},progress:{:.5f}}});",
            a_state.equipped ? "true" : "false",
            a_state.tier,
            a_state.cooling ? "true" : "false",
            a_state.conditionsValid ? "true" : "false",
            a_state.progress);

        g_prisma->Invoke(g_view, script.c_str());
        g_lastState = a_state;
        g_hasLastState = true;
    }

    void UpdateHUDOnGameThread()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= g_nextStoneScan) {
            g_cachedDefinition = GetEquippedDefinition(player);
            g_nextStoneScan = now + 250ms;
        }

        HUDState state{};
        state.equipped = g_cachedDefinition != nullptr;

        if (g_cachedDefinition) {
            state.tier = GetTechniqueTier(*g_cachedDefinition);
            state.conditionsValid = TechniqueConditionsMet(player, *g_cachedDefinition, state.tier);
            const auto [cooling, progress] = GetCooldownState(player, state.tier);
            state.cooling = cooling;
            state.progress = cooling ? progress : 1.0f;
        }

        PushState(state);
    }

    void StartPolling()
    {
        if (g_pollThread.joinable()) {
            return;
        }

        g_pollThread = std::jthread([](std::stop_token stopToken) {
            while (!stopToken.stop_requested()) {
                if (auto* taskInterface = SKSE::GetTaskInterface()) {
                    taskInterface->AddTask([]() { UpdateHUDOnGameThread(); });
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(g_config.pollMs));
            }
        });
    }

    void CloseSettingsMenu();

    void OnSettingsApply(const char* a_argument)
    {
        if (!a_argument) {
            return;
        }

        int left = g_config.left;
        int bottom = g_config.bottom;
        int size = g_config.size;
        if (std::sscanf(a_argument, "%d|%d|%d", &left, &bottom, &size) != 3) {
            return;
        }

        g_config.left = std::clamp(left, 0, 4000);
        g_config.bottom = std::clamp(bottom, 0, 4000);
        g_config.size = std::clamp(size, 40, 200);
        SaveLayoutConfig();
    }

    void OnSettingsClose(const char*)
    {
        CloseSettingsMenu();
    }

    void OpenSettingsMenu()
    {
        if (!g_prisma || !g_domReady.load() || !g_view || !g_prisma->IsValid(g_view) || g_settingsOpen) {
            return;
        }
        if (g_prisma->HasAnyActiveFocus()) {
            return;
        }

        const auto script = std::format(
            "window.TechniqueHUD&&window.TechniqueHUD.openSettings({},{},{});",
            g_config.left, g_config.bottom, g_config.size);
        g_prisma->Invoke(g_view, script.c_str());

        if (!g_prisma->Focus(g_view, true)) {
            return;
        }
        g_settingsOpen = true;
    }

    void CloseSettingsMenu()
    {
        if (!g_prisma || !g_view || !g_prisma->IsValid(g_view)) {
            g_settingsOpen = false;
            return;
        }

        if (g_domReady.load()) {
            g_prisma->Invoke(g_view, "window.TechniqueHUD&&window.TechniqueHUD.closeSettings();");
        }
        if (g_prisma->HasFocus(g_view)) {
            g_prisma->Unfocus(g_view);
        }
        g_settingsOpen = false;
    }

    void ToggleSettingsMenu()
    {
        if (g_settingsOpen) {
            CloseSettingsMenu();
        } else {
            OpenSettingsMenu();
        }
    }

    class InputEventSink final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputEventSink* GetSingleton()
        {
            static InputEventSink singleton;
            return std::addressof(singleton);
        }

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_events,
            RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!a_events) {
                return RE::BSEventNotifyControl::kContinue;
            }

            for (auto* event = *a_events; event; event = event->next) {
                if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
                    continue;
                }

                auto* button = event->AsButtonEvent();
                if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown()) {
                    continue;
                }

                if (static_cast<int>(button->GetIDCode()) == g_config.menuKey) {
                    ToggleSettingsMenu();
                    break;
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void RegisterInputSink()
    {
        if (g_inputRegistered) {
            return;
        }
        if (auto* inputManager = RE::BSInputDeviceManager::GetSingleton()) {
            inputManager->AddEventSink(InputEventSink::GetSingleton());
            g_inputRegistered = true;
        }
    }

    void CreateHUDView()
    {
        g_prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
        if (!g_prisma) {
            SKSE::log::error("PrismaUI API unavailable");
            return;
        }

        g_view = g_prisma->CreateView(kViewPath, [](PrismaView a_view) {
            g_view = a_view;
            g_domReady.store(true);

            const auto layout = std::format(
                "window.TechniqueHUD&&window.TechniqueHUD.setLayout({},{},{});",
                g_config.left, g_config.bottom, g_config.size);
            g_prisma->Invoke(g_view, layout.c_str());
            g_prisma->SetOrder(g_view, 500);
            g_prisma->Show(g_view);
            SKSE::log::info("Technique HUD DOM ready");
        });

        if (!g_view || !g_prisma->IsValid(g_view)) {
            SKSE::log::error("PrismaUI CreateView failed for {}", kViewPath);
            g_view = 0;
            return;
        }

        g_prisma->RegisterJSListener(g_view, "HEHUDApply", OnSettingsApply);
        g_prisma->RegisterJSListener(g_view, "HEHUDClose", OnSettingsClose);
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        switch (a_message->type) {
        case SKSE::MessagingInterface::kInputLoaded:
            LoadConfig();
            RegisterInputSink();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            LoadConfig();
            ResolveForms();
            CreateHUDView();
            StartPolling();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            if (g_settingsOpen) {
                CloseSettingsMenu();
            }
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    if (auto path = SKSE::log::log_directory()) {
        *path /= "HE_TechniqueHUD.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("HE_TechniqueHUD", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }

    SKSE::Init(a_skse);

    if (auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener("SKSE", MessageHandler);
    } else {
        return false;
    }

    SKSE::log::info("HE Technique HUD v5.6.11 layered sigil loaded");
    return true;
}
