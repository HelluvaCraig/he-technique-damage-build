#include "pch.h"
#include "PrismaUI_API.h"
#include "SKSEMenuFramework.h"

using namespace std::chrono_literals;

namespace
{
    constexpr auto kStonePlugin = "HE Elden Rim - Ash Rings.esp";
    constexpr auto kViewPath = "HelluvaHUD/index.html";

    constexpr auto kDefaultIniRelativePath = "Data\\SKSE\\Plugins\\HelluvaHUD.ini";
    constexpr auto kUserIniRelativePath = "Data\\SKSE\\Plugins\\HelluvaHUD.user.ini";
    constexpr auto kLegacyUserIniRelativePath = "Data\\SKSE\\Plugins\\HE_TechniqueHUD.user.ini";

    constexpr RE::FormID kTechniqueChargesGlobalLocalID = 0xE23;
    constexpr RE::FormID kTechniqueMaxChargesGlobalLocalID = 0xE24;
    constexpr RE::FormID kTechniqueRechargeProgressGlobalLocalID = 0xE25;
    constexpr RE::FormID kTechniqueRecoveryGlobalLocalID = 0xE26;

    PRISMA_UI_API::IVPrismaUI1* g_prisma = nullptr;
    PrismaView g_view = 0;
    std::atomic_bool g_domReady{ false };
    std::jthread g_pollThread;

    RE::TESGlobal* g_currentCharges = nullptr;
    RE::TESGlobal* g_maxCharges = nullptr;
    RE::TESGlobal* g_rechargeProgress = nullptr;
    RE::TESGlobal* g_recoveryPercent = nullptr;

    std::filesystem::path g_defaultIniPath{ kDefaultIniRelativePath };
    std::filesystem::path g_userIniPath{ kUserIniRelativePath };
    std::filesystem::path g_legacyUserIniPath{ kLegacyUserIniRelativePath };

    struct Config
    {
        int left = 574;
        int bottom = 32;
        int scale = 100;
        int pollMs = 50;
        bool combatOnly = false;
        int layoutVersion = 2;
    } g_config;

    struct HUDState
    {
        float chargesCurrent = 0.0f;
        float chargesMax = 0.0f;
        float chargeProgress = 0.0f;
        float recovery = 0.0f;

        float healthCurrent = 0.0f;
        float healthMax = 1.0f;
        float magickaCurrent = 0.0f;
        float magickaMax = 1.0f;
        float staminaCurrent = 0.0f;
        float staminaMax = 1.0f;

        float level = 1.0f;
        float xpProgress = 0.0f;
    };

    HUDState g_lastState{};
    bool g_hasLastState = false;
    bool g_menuFrameworkRegistered = false;
    bool g_gameLoaded = false;
    bool g_visibilityKnown = false;
    bool g_lastVisibility = false;

    std::string Trim(std::string a_value)
    {
        const auto first = a_value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }
        const auto last = a_value.find_last_not_of(" \t\r\n");
        return a_value.substr(first, last - first + 1);
    }

    bool ParseBool(const std::string& a_value, bool a_fallback)
    {
        std::string value = Trim(a_value);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (value == "1" || value == "true" || value == "yes" || value == "on") {
            return true;
        }
        if (value == "0" || value == "false" || value == "no" || value == "off") {
            return false;
        }
        return a_fallback;
    }

    void ClampConfig()
    {
        g_config.left = std::clamp(g_config.left, 0, 4000);
        g_config.bottom = std::clamp(g_config.bottom, 0, 4000);
        g_config.scale = std::clamp(g_config.scale, 50, 250);
        g_config.pollMs = std::clamp(g_config.pollMs, 33, 250);
    }

    void ApplyIniFile(const std::filesystem::path& a_path)
    {
        std::ifstream input(a_path);
        if (!input.is_open()) {
            return;
        }

        bool inHudSection = false;
        std::string line;
        while (std::getline(input, line)) {
            line = Trim(line);
            if (line.empty() || line.starts_with(';') || line.starts_with('#')) {
                continue;
            }

            if (line.front() == '[' && line.back() == ']') {
                inHudSection = (Trim(line.substr(1, line.size() - 2)) == "HUD");
                continue;
            }
            if (!inHudSection) {
                continue;
            }

            const auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }

            const auto key = Trim(line.substr(0, eq));
            const auto value = Trim(line.substr(eq + 1));

            try {
                if (key == "Left") {
                    g_config.left = std::stoi(value);
                } else if (key == "Bottom") {
                    g_config.bottom = std::stoi(value);
                } else if (key == "Scale") {
                    g_config.scale = std::stoi(value);
                } else if (key == "PollMs") {
                    g_config.pollMs = std::stoi(value);
                } else if (key == "CombatOnly") {
                    g_config.combatOnly = ParseBool(value, g_config.combatOnly);
                } else if (key == "LayoutVersion") {
                    g_config.layoutVersion = std::stoi(value);
                }
            } catch (...) {
                SKSE::log::warn("Ignoring invalid HelluvaHUD setting {}={} from {}", key, value, a_path.string());
            }
        }
    }

    bool SaveConfig()
    {
        ClampConfig();

        std::error_code ec;
        std::filesystem::create_directories(g_userIniPath.parent_path(), ec);
        if (ec) {
            SKSE::log::error(
                "Could not create HelluvaHUD user config directory {}: {}",
                g_userIniPath.parent_path().string(),
                ec.message());
            return false;
        }

        std::ofstream output(g_userIniPath, std::ios::out | std::ios::trunc);
        if (!output.is_open()) {
            SKSE::log::error(
                "Could not open HelluvaHUD user config for writing: {}",
                std::filesystem::absolute(g_userIniPath).string());
            return false;
        }

        output
            << "[HUD]\n"
            << "; Auto-generated by HelluvaHUD.\n"
            << "; Under MO2 this file should appear in Overwrite\\SKSE\\Plugins.\n"
            << "Left=" << g_config.left << "\n"
            << "Bottom=" << g_config.bottom << "\n"
            << "Scale=" << g_config.scale << "\n"
            << "CombatOnly=" << (g_config.combatOnly ? 1 : 0) << "\n"
            << "PollMs=" << g_config.pollMs << "\n"
            << "LayoutVersion=" << g_config.layoutVersion << "\n";
        output.flush();

        if (!output.good()) {
            SKSE::log::error(
                "Failed while writing HelluvaHUD user config: {}",
                std::filesystem::absolute(g_userIniPath).string());
            return false;
        }

        SKSE::log::info(
            "Saved HelluvaHUD settings Left={} Bottom={} Scale={}% CombatOnly={} to {}",
            g_config.left,
            g_config.bottom,
            g_config.scale,
            g_config.combatOnly,
            std::filesystem::absolute(g_userIniPath).string());
        return true;
    }

    void LoadConfig()
    {
        g_config = {};
        ApplyIniFile(g_defaultIniPath);

        bool migrated = false;
        if (std::filesystem::exists(g_userIniPath)) {
            ApplyIniFile(g_userIniPath);
        } else if (std::filesystem::exists(g_legacyUserIniPath)) {
            ApplyIniFile(g_legacyUserIniPath);
            migrated = true;
        }

        ClampConfig();

        // v0.2 changes from a standalone 240x10 charge widget to the 772x68
        // core stats cluster. Migrate the old placement once so existing users
        // land on the Figma-authored bottom-centre position.
        if (g_config.layoutVersion < 2) {
            g_config.left = 574;
            g_config.bottom = 32;
            g_config.layoutVersion = 2;
            migrated = true;
        }

        SKSE::log::info(
            "Loaded HelluvaHUD Left={} Bottom={} Scale={}% CombatOnly={} LayoutVersion={} user config={}",
            g_config.left,
            g_config.bottom,
            g_config.scale,
            g_config.combatOnly,
            g_config.layoutVersion,
            std::filesystem::absolute(g_userIniPath).string());

        if (migrated) {
            SKSE::log::info("Writing migrated HelluvaHUD v0.2 layout");
            SaveConfig();
        }
    }

    void ApplyLayoutToHUD()
    {
        if (!g_prisma || !g_domReady.load() || !g_view || !g_prisma->IsValid(g_view)) {
            return;
        }

        const auto script = std::format(
            "window.HelluvaHUD&&window.HelluvaHUD.setLayout({},{},{});",
            g_config.left,
            g_config.bottom,
            g_config.scale);
        g_prisma->Invoke(g_view, script.c_str());
    }

    bool IsMenuFrameworkOpen()
    {
        if (!SKSEMenuFramework::IsInstalled()) {
            return false;
        }
        if (auto* window = SKSEMenuFramework::GetMainWindow()) {
            return window->IsOpen.load();
        }
        return false;
    }

    bool IsBlockingGameMenuOpen()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return true;
        }

        // Keep HelluvaHUD visible while its Menu Framework page is open so the
        // placement sliders can be previewed live.
        if (IsMenuFrameworkOpen()) {
            return false;
        }

        if (ui->GameIsPaused() ||
            ui->IsItemMenuOpen() ||
            ui->IsApplicationMenuOpen() ||
            ui->IsModalMenuOpen()) {
            return true;
        }

        return ui->IsMenuOpen("Main Menu") ||
               ui->IsMenuOpen("Loading Menu") ||
               ui->IsMenuOpen("InventoryMenu") ||
               ui->IsMenuOpen("MagicMenu") ||
               ui->IsMenuOpen("MapMenu") ||
               ui->IsMenuOpen("Journal Menu") ||
               ui->IsMenuOpen("Console");
    }

    bool ShouldShowHUD(RE::PlayerCharacter* a_player)
    {
        if (!g_gameLoaded || !a_player || !a_player->GetParentCell()) {
            return false;
        }

        if (IsBlockingGameMenuOpen()) {
            return false;
        }

        if (g_config.combatOnly && !a_player->IsInCombat()) {
            return false;
        }

        return true;
    }

    void ApplyVisibilityToHUD(bool a_visible, bool a_immediate = false)
    {
        if (!g_prisma || !g_domReady.load() || !g_view || !g_prisma->IsValid(g_view)) {
            return;
        }

        if (g_visibilityKnown && g_lastVisibility == a_visible && !a_immediate) {
            return;
        }

        const auto script = std::format(
            "window.HelluvaHUD&&window.HelluvaHUD.setVisible({},{});",
            a_visible ? "true" : "false",
            a_immediate ? "true" : "false");
        g_prisma->Invoke(g_view, script.c_str());

        g_lastVisibility = a_visible;
        g_visibilityKnown = true;
    }

    void ResolveForms()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            SKSE::log::error("TESDataHandler unavailable");
            return;
        }

        g_currentCharges = dataHandler->LookupForm<RE::TESGlobal>(kTechniqueChargesGlobalLocalID, kStonePlugin);
        g_maxCharges = dataHandler->LookupForm<RE::TESGlobal>(kTechniqueMaxChargesGlobalLocalID, kStonePlugin);
        g_rechargeProgress = dataHandler->LookupForm<RE::TESGlobal>(kTechniqueRechargeProgressGlobalLocalID, kStonePlugin);
        g_recoveryPercent = dataHandler->LookupForm<RE::TESGlobal>(kTechniqueRecoveryGlobalLocalID, kStonePlugin);

        SKSE::log::info(
            "Technique Charge globals: current={} max={} progress={} recovery={}",
            g_currentCharges ? "OK" : "MISSING",
            g_maxCharges ? "OK" : "MISSING",
            g_rechargeProgress ? "OK" : "MISSING",
            g_recoveryPercent ? "OK" : "MISSING");
    }

    bool MeaningfullyChanged(const HUDState& a_state)
    {
        if (!g_hasLastState) {
            return true;
        }

        return std::fabs(a_state.chargesCurrent - g_lastState.chargesCurrent) >= 0.001f ||
               std::fabs(a_state.chargesMax - g_lastState.chargesMax) >= 0.001f ||
               std::fabs(a_state.chargeProgress - g_lastState.chargeProgress) >= 0.0025f ||
               std::fabs(a_state.recovery - g_lastState.recovery) >= 0.01f ||
               std::fabs(a_state.healthCurrent - g_lastState.healthCurrent) >= 0.05f ||
               std::fabs(a_state.healthMax - g_lastState.healthMax) >= 0.05f ||
               std::fabs(a_state.magickaCurrent - g_lastState.magickaCurrent) >= 0.05f ||
               std::fabs(a_state.magickaMax - g_lastState.magickaMax) >= 0.05f ||
               std::fabs(a_state.staminaCurrent - g_lastState.staminaCurrent) >= 0.05f ||
               std::fabs(a_state.staminaMax - g_lastState.staminaMax) >= 0.05f ||
               std::fabs(a_state.level - g_lastState.level) >= 0.001f ||
               std::fabs(a_state.xpProgress - g_lastState.xpProgress) >= 0.001f;
    }

    void PushState(const HUDState& a_state)
    {
        if (!g_prisma || !g_domReady.load() || !g_view || !g_prisma->IsValid(g_view) || !MeaningfullyChanged(a_state)) {
            return;
        }

        const auto script = std::format(
            "window.HelluvaHUD&&window.HelluvaHUD.setState({{"
            "chargesCurrent:{:.3f},chargesMax:{:.3f},chargeProgress:{:.5f},recovery:{:.3f},"
            "healthCurrent:{:.3f},healthMax:{:.3f},"
            "magickaCurrent:{:.3f},magickaMax:{:.3f},"
            "staminaCurrent:{:.3f},staminaMax:{:.3f},"
            "level:{:.0f},xpProgress:{:.5f}"
            "}});",
            a_state.chargesCurrent,
            a_state.chargesMax,
            a_state.chargeProgress,
            a_state.recovery,
            a_state.healthCurrent,
            a_state.healthMax,
            a_state.magickaCurrent,
            a_state.magickaMax,
            a_state.staminaCurrent,
            a_state.staminaMax,
            a_state.level,
            a_state.xpProgress);

        g_prisma->Invoke(g_view, script.c_str());
        g_lastState = a_state;
        g_hasLastState = true;
    }

    void UpdateHUDOnGameThread()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        ApplyVisibilityToHUD(ShouldShowHUD(player));

        if (!g_currentCharges || !g_maxCharges || !g_rechargeProgress || !g_recoveryPercent) {
            return;
        }

        HUDState state{};
        state.chargesCurrent = std::clamp(g_currentCharges->value, 0.0f, 5.0f);
        state.chargesMax = std::clamp(g_maxCharges->value, 0.0f, 5.0f);
        state.chargeProgress = std::clamp(g_rechargeProgress->value, 0.0f, 1.0f);
        state.recovery = std::clamp(g_recoveryPercent->value, 0.0f, 95.0f);

        if (player) {
            if (auto* avOwner = player->AsActorValueOwner()) {
                state.healthCurrent = std::max(0.0f, avOwner->GetActorValue(RE::ActorValue::kHealth));
                state.healthMax = std::max(1.0f, avOwner->GetPermanentActorValue(RE::ActorValue::kHealth));
                state.magickaCurrent = std::max(0.0f, avOwner->GetActorValue(RE::ActorValue::kMagicka));
                state.magickaMax = std::max(1.0f, avOwner->GetPermanentActorValue(RE::ActorValue::kMagicka));
                state.staminaCurrent = std::max(0.0f, avOwner->GetActorValue(RE::ActorValue::kStamina));
                state.staminaMax = std::max(1.0f, avOwner->GetPermanentActorValue(RE::ActorValue::kStamina));
            }

            state.level = static_cast<float>(player->GetLevel());

            auto* skills = player->GetPlayerRuntimeData().skills;
            if (skills && skills->data) {
                const auto xp = std::max(0.0f, skills->data->xp);
                const auto threshold = skills->data->levelThreshold;
                state.xpProgress = threshold > 0.0f ?
                    std::clamp(xp / threshold, 0.0f, 1.0f) :
                    0.0f;
            }
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

    void CommitMenuChange()
    {
        ClampConfig();
        ApplyLayoutToHUD();
        g_visibilityKnown = false;
        UpdateHUDOnGameThread();
        SaveConfig();
    }

    void __stdcall RenderMenuFrameworkSettings()
    {
        ImGuiMCP::TextWrapped(
            "HelluvaHUD placement and visibility. Changes apply immediately and save automatically.");

        ImGuiMCP::Spacing();
        bool changed = false;

        changed |= ImGuiMCP::Checkbox(
            "Only show HUD in combat",
            &g_config.combatOnly);

        if (g_config.combatOnly) {
            ImGuiMCP::TextWrapped(
                "Immersive mode: HelluvaHUD fades in when combat starts and fades out when combat ends.");
        } else {
            ImGuiMCP::TextWrapped(
                "Always mode: HelluvaHUD stays visible during normal gameplay.");
        }

        ImGuiMCP::Spacing();

        ImGuiMCP::SetNextItemWidth(360.0f);
        changed |= ImGuiMCP::SliderInt("Horizontal position", &g_config.left, 0, 3840, "%d px");

        ImGuiMCP::SetNextItemWidth(360.0f);
        changed |= ImGuiMCP::SliderInt("Vertical position", &g_config.bottom, 0, 2160, "%d px");

        ImGuiMCP::SetNextItemWidth(360.0f);
        changed |= ImGuiMCP::SliderInt("HUD scale", &g_config.scale, 50, 250, "%d%%");

        if (changed) {
            CommitMenuChange();
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::Button("Reset HUD placement")) {
            g_config.left = 574;
            g_config.bottom = 32;
            g_config.scale = 100;
            g_config.layoutVersion = 2;
            CommitMenuChange();
        }

        ImGuiMCP::Separator();

        if (g_currentCharges && g_maxCharges && g_rechargeProgress) {
            const int current = std::clamp(static_cast<int>(std::lround(g_currentCharges->value)), 0, 5);
            const int maximum = std::clamp(static_cast<int>(std::lround(g_maxCharges->value)), 0, 5);
            const int progress = std::clamp(static_cast<int>(std::lround(g_rechargeProgress->value * 100.0f)), 0, 100);

            ImGuiMCP::Text("Technique Charges: %d / %d", current, maximum);
            if (current < maximum) {
                ImGuiMCP::Text("Next charge: %d%%", progress);
            }
        }

        ImGuiMCP::Spacing();
        ImGuiMCP::TextWrapped(
            "Saved to Data\\SKSE\\Plugins\\HelluvaHUD.user.ini. "
            "With Mod Organizer 2 this newly-created file should be written to Overwrite.");
    }

    void RegisterMenuFramework()
    {
        if (g_menuFrameworkRegistered) {
            return;
        }

        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::warn(
                "SKSE Menu Framework not installed; HelluvaHUD will work but its settings page is unavailable.");
            return;
        }

        SKSEMenuFramework::SetSection("HelluvaHUD");
        SKSEMenuFramework::AddSectionItem("HUD Settings", RenderMenuFrameworkSettings);
        g_menuFrameworkRegistered = true;

        SKSE::log::info(
            "Registered HelluvaHUD settings with SKSE Menu Framework v{:.2f}",
            SKSEMenuFramework::GetMenuFrameworkVersion());
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

            ApplyLayoutToHUD();
            g_prisma->SetOrder(g_view, 500);
            g_prisma->Show(g_view);

            g_visibilityKnown = false;
            ApplyVisibilityToHUD(false, true);

            SKSE::log::info("HelluvaHUD DOM ready");
        });

        if (!g_view || !g_prisma->IsValid(g_view)) {
            SKSE::log::error("PrismaUI CreateView failed for {}", kViewPath);
            g_view = 0;
        }
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        switch (a_message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            LoadConfig();
            ResolveForms();
            CreateHUDView();
            RegisterMenuFramework();
            StartPolling();
            break;

        case SKSE::MessagingInterface::kPreLoadGame:
            g_gameLoaded = false;
            g_visibilityKnown = false;
            ApplyVisibilityToHUD(false, true);
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            g_gameLoaded = true;
            g_hasLastState = false;
            g_visibilityKnown = false;
            ApplyLayoutToHUD();
            UpdateHUDOnGameThread();
            break;

        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    if (auto path = SKSE::log::log_directory()) {
        *path /= "HelluvaHUD.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("HelluvaHUD", std::move(sink));
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

    SKSE::log::info("HelluvaHUD v0.2.0 core stats module loaded");
    return true;
}
