#include "pch.h"
#include "PrismaUI_API.h"

using namespace std::chrono_literals;

namespace
{
    constexpr auto kStonePlugin = "HE Elden Rim - Ash Rings.esp";
    constexpr auto kViewPath = "HE_TechniqueHUD/index.html";
    constexpr auto kDefaultIniRelativePath = "Data\\SKSE\\Plugins\\HE_TechniqueHUD.ini";
    constexpr auto kUserIniRelativePath = "Data\\SKSE\\Plugins\\HE_TechniqueHUD.user.ini";

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

    std::filesystem::path g_defaultIniPath;
    std::filesystem::path g_userIniPath;
    std::string g_defaultIniPathString;
    std::string g_userIniPathString;

    struct Config
    {
        int left = 22;
        int bottom = 112;
        int scale = 100;
        int pollMs = 50;
        int menuKey = 68;
    } g_config;

    struct HUDState
    {
        float current = 0.0f;
        float max = 0.0f;
        float progress = 0.0f;
        float recovery = 0.0f;
    };

    HUDState g_lastState{};
    bool g_hasLastState = false;
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
        g_config.scale = static_cast<int>(GetPrivateProfileIntA("HUD", "Scale", g_config.scale, g_defaultIniPathString.c_str()));
        g_config.pollMs = static_cast<int>(GetPrivateProfileIntA("HUD", "PollMs", g_config.pollMs, g_defaultIniPathString.c_str()));
        g_config.menuKey = static_cast<int>(GetPrivateProfileIntA("HUD", "MenuKey", g_config.menuKey, g_defaultIniPathString.c_str()));

        g_config.left = static_cast<int>(GetPrivateProfileIntA("HUD", "Left", g_config.left, g_userIniPathString.c_str()));
        g_config.bottom = static_cast<int>(GetPrivateProfileIntA("HUD", "Bottom", g_config.bottom, g_userIniPathString.c_str()));
        g_config.scale = static_cast<int>(GetPrivateProfileIntA("HUD", "Scale", g_config.scale, g_userIniPathString.c_str()));
        g_config.pollMs = static_cast<int>(GetPrivateProfileIntA("HUD", "PollMs", g_config.pollMs, g_userIniPathString.c_str()));
        g_config.menuKey = static_cast<int>(GetPrivateProfileIntA("HUD", "MenuKey", g_config.menuKey, g_userIniPathString.c_str()));

        g_config.left = std::clamp(g_config.left, 0, 4000);
        g_config.bottom = std::clamp(g_config.bottom, 0, 4000);
        g_config.scale = std::clamp(g_config.scale, 50, 250);
        g_config.pollMs = std::clamp(g_config.pollMs, 33, 250);
        g_config.menuKey = std::clamp(g_config.menuKey, 0, 255);

        SKSE::log::info("Loaded charge HUD layout Left={} Bottom={} Scale={}%", g_config.left, g_config.bottom, g_config.scale);
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
        const auto scale = std::to_string(g_config.scale);

        WritePrivateProfileStringA("HUD", "Left", left.c_str(), g_userIniPathString.c_str());
        WritePrivateProfileStringA("HUD", "Bottom", bottom.c_str(), g_userIniPathString.c_str());
        WritePrivateProfileStringA("HUD", "Scale", scale.c_str(), g_userIniPathString.c_str());
        WritePrivateProfileStringA(nullptr, nullptr, nullptr, g_userIniPathString.c_str());
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

        return std::fabs(a_state.current - g_lastState.current) >= 0.001f ||
               std::fabs(a_state.max - g_lastState.max) >= 0.001f ||
               std::fabs(a_state.progress - g_lastState.progress) >= 0.0025f ||
               std::fabs(a_state.recovery - g_lastState.recovery) >= 0.01f;
    }

    void PushState(const HUDState& a_state)
    {
        if (!g_prisma || !g_domReady.load() || !g_view || !g_prisma->IsValid(g_view) || !MeaningfullyChanged(a_state)) {
            return;
        }

        const auto script = std::format(
            "window.TechniqueHUD&&window.TechniqueHUD.setState({{current:{:.3f},max:{:.3f},progress:{:.5f},recovery:{:.3f}}});",
            a_state.current,
            a_state.max,
            a_state.progress,
            a_state.recovery);

        g_prisma->Invoke(g_view, script.c_str());
        g_lastState = a_state;
        g_hasLastState = true;
    }

    void UpdateHUDOnGameThread()
    {
        if (!g_currentCharges || !g_maxCharges || !g_rechargeProgress || !g_recoveryPercent) {
            return;
        }

        HUDState state{};
        state.current = std::clamp(g_currentCharges->value, 0.0f, 5.0f);
        state.max = std::clamp(g_maxCharges->value, 0.0f, 5.0f);
        state.progress = std::clamp(g_rechargeProgress->value, 0.0f, 1.0f);
        state.recovery = std::clamp(g_recoveryPercent->value, 0.0f, 95.0f);
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
        int scale = g_config.scale;
        if (std::sscanf(a_argument, "%d|%d|%d", &left, &bottom, &scale) != 3) {
            return;
        }

        g_config.left = std::clamp(left, 0, 4000);
        g_config.bottom = std::clamp(bottom, 0, 4000);
        g_config.scale = std::clamp(scale, 50, 250);
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
            g_config.left,
            g_config.bottom,
            g_config.scale);
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
                g_config.left,
                g_config.bottom,
                g_config.scale);
            g_prisma->Invoke(g_view, layout.c_str());
            g_prisma->SetOrder(g_view, 500);
            g_prisma->Show(g_view);
            SKSE::log::info("Technique Charge HUD DOM ready");
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
            g_hasLastState = false;
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

    SKSE::log::info("HE Technique HUD v5.7.6 shared charge bars loaded");
    return true;
}
