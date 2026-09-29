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

    struct ModuleLayout
    {
        int x = 0;
        int y = 0;
        int scale = 100;
        int layer = 10;
        bool visible = true;
    };

    struct Config
    {
        int left = 574;
        int bottom = 32;
        int scale = 100;
        int pollMs = 50;
        bool combatOnly = false;
        int layoutVersion = 6;

        ModuleLayout healthText{ 0, 0, 100, 20, true };
        ModuleLayout healthBar{ 0, 0, 100, 10, true };
        ModuleLayout magickaText{ 0, 0, 100, 20, true };
        ModuleLayout magickaBar{ 0, 0, 100, 10, true };
        ModuleLayout stamina{ 0, 0, 100, 10, true };
        ModuleLayout charges{ 0, 0, 100, 12, true };

        ModuleLayout levelBlank{ 0, 0, 100, 10, true };
        ModuleLayout levelFill{ 0, 0, 100, 12, true };
        ModuleLayout levelAvailable{ 0, 0, 100, 14, true };
        ModuleLayout levelPlate{ 0, 0, 100, 20, true };
        ModuleLayout levelNumber{ 0, 0, 100, 30, true };
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
        float levelUpAvailable = 0.0f;
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

    bool FileContainsSetting(const std::filesystem::path& a_path, const std::string& a_key)
    {
        std::ifstream input(a_path);
        if (!input.is_open()) {
            return false;
        }

        const auto prefix = a_key + "=";
        std::string line;
        while (std::getline(input, line)) {
            line = Trim(line);
            if (line.starts_with(prefix)) {
                return true;
            }
        }
        return false;
    }

    void ClampConfig()
    {
        g_config.left = std::clamp(g_config.left, 0, 4000);
        g_config.bottom = std::clamp(g_config.bottom, 0, 4000);
        g_config.scale = std::clamp(g_config.scale, 50, 250);
        g_config.pollMs = std::clamp(g_config.pollMs, 33, 250);

        auto clampModule = [](ModuleLayout& a_module) {
            a_module.x = std::clamp(a_module.x, -2000, 2000);
            a_module.y = std::clamp(a_module.y, -2000, 2000);
            a_module.scale = std::clamp(a_module.scale, 50, 200);
            a_module.layer = std::clamp(a_module.layer, 0, 100);
        };
        clampModule(g_config.healthText);
        clampModule(g_config.healthBar);
        clampModule(g_config.magickaText);
        clampModule(g_config.magickaBar);
        clampModule(g_config.stamina);
        clampModule(g_config.charges);
        clampModule(g_config.levelBlank);
        clampModule(g_config.levelFill);
        clampModule(g_config.levelAvailable);
        clampModule(g_config.levelPlate);
        clampModule(g_config.levelNumber);
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
                } else if (key == "HealthTextX") {
                    g_config.healthText.x = std::stoi(value);
                } else if (key == "HealthTextY") {
                    g_config.healthText.y = std::stoi(value);
                } else if (key == "HealthTextScale") {
                    g_config.healthText.scale = std::stoi(value);
                } else if (key == "HealthTextLayer") {
                    g_config.healthText.layer = std::stoi(value);
                } else if (key == "HealthTextVisible") {
                    g_config.healthText.visible = ParseBool(value, g_config.healthText.visible);
                } else if (key == "HealthBarX") {
                    g_config.healthBar.x = std::stoi(value);
                } else if (key == "HealthBarY") {
                    g_config.healthBar.y = std::stoi(value);
                } else if (key == "HealthBarScale") {
                    g_config.healthBar.scale = std::stoi(value);
                } else if (key == "HealthBarLayer") {
                    g_config.healthBar.layer = std::stoi(value);
                } else if (key == "HealthBarVisible") {
                    g_config.healthBar.visible = ParseBool(value, g_config.healthBar.visible);
                } else if (key == "MagickaTextX") {
                    g_config.magickaText.x = std::stoi(value);
                } else if (key == "MagickaTextY") {
                    g_config.magickaText.y = std::stoi(value);
                } else if (key == "MagickaTextScale") {
                    g_config.magickaText.scale = std::stoi(value);
                } else if (key == "MagickaTextLayer") {
                    g_config.magickaText.layer = std::stoi(value);
                } else if (key == "MagickaTextVisible") {
                    g_config.magickaText.visible = ParseBool(value, g_config.magickaText.visible);
                } else if (key == "MagickaBarX") {
                    g_config.magickaBar.x = std::stoi(value);
                } else if (key == "MagickaBarY") {
                    g_config.magickaBar.y = std::stoi(value);
                } else if (key == "MagickaBarScale") {
                    g_config.magickaBar.scale = std::stoi(value);
                } else if (key == "MagickaBarLayer") {
                    g_config.magickaBar.layer = std::stoi(value);
                } else if (key == "MagickaBarVisible") {
                    g_config.magickaBar.visible = ParseBool(value, g_config.magickaBar.visible);
                } else if (key == "StaminaX") {
                    g_config.stamina.x = std::stoi(value);
                } else if (key == "StaminaY") {
                    g_config.stamina.y = std::stoi(value);
                } else if (key == "StaminaScale") {
                    g_config.stamina.scale = std::stoi(value);
                } else if (key == "StaminaLayer") {
                    g_config.stamina.layer = std::stoi(value);
                } else if (key == "StaminaVisible") {
                    g_config.stamina.visible = ParseBool(value, g_config.stamina.visible);
                } else if (key == "ChargesX") {
                    g_config.charges.x = std::stoi(value);
                } else if (key == "ChargesY") {
                    g_config.charges.y = std::stoi(value);
                } else if (key == "ChargesScale") {
                    g_config.charges.scale = std::stoi(value);
                } else if (key == "ChargesLayer") {
                    g_config.charges.layer = std::stoi(value);
                } else if (key == "ChargesVisible") {
                    g_config.charges.visible = ParseBool(value, g_config.charges.visible);
                } else if (key == "LevelBlankX") {
                    g_config.levelBlank.x = std::stoi(value);
                } else if (key == "LevelBlankY") {
                    g_config.levelBlank.y = std::stoi(value);
                } else if (key == "LevelBlankScale") {
                    g_config.levelBlank.scale = std::stoi(value);
                } else if (key == "LevelBlankLayer") {
                    g_config.levelBlank.layer = std::stoi(value);
                } else if (key == "LevelBlankVisible") {
                    g_config.levelBlank.visible = ParseBool(value, g_config.levelBlank.visible);
                } else if (key == "LevelFillX") {
                    g_config.levelFill.x = std::stoi(value);
                } else if (key == "LevelFillY") {
                    g_config.levelFill.y = std::stoi(value);
                } else if (key == "LevelFillScale") {
                    g_config.levelFill.scale = std::stoi(value);
                } else if (key == "LevelFillLayer") {
                    g_config.levelFill.layer = std::stoi(value);
                } else if (key == "LevelFillVisible") {
                    g_config.levelFill.visible = ParseBool(value, g_config.levelFill.visible);
                } else if (key == "LevelAvailableX") {
                    g_config.levelAvailable.x = std::stoi(value);
                } else if (key == "LevelAvailableY") {
                    g_config.levelAvailable.y = std::stoi(value);
                } else if (key == "LevelAvailableScale") {
                    g_config.levelAvailable.scale = std::stoi(value);
                } else if (key == "LevelAvailableLayer") {
                    g_config.levelAvailable.layer = std::stoi(value);
                } else if (key == "LevelAvailableVisible") {
                    g_config.levelAvailable.visible = ParseBool(value, g_config.levelAvailable.visible);
                } else if (key == "LevelPlateX") {
                    g_config.levelPlate.x = std::stoi(value);
                } else if (key == "LevelPlateY") {
                    g_config.levelPlate.y = std::stoi(value);
                } else if (key == "LevelPlateScale") {
                    g_config.levelPlate.scale = std::stoi(value);
                } else if (key == "LevelPlateLayer") {
                    g_config.levelPlate.layer = std::stoi(value);
                } else if (key == "LevelPlateVisible") {
                    g_config.levelPlate.visible = ParseBool(value, g_config.levelPlate.visible);
                } else if (key == "LevelNumberX") {
                    g_config.levelNumber.x = std::stoi(value);
                } else if (key == "LevelNumberY") {
                    g_config.levelNumber.y = std::stoi(value);
                } else if (key == "LevelNumberScale") {
                    g_config.levelNumber.scale = std::stoi(value);
                } else if (key == "LevelNumberLayer") {
                    g_config.levelNumber.layer = std::stoi(value);
                } else if (key == "LevelNumberVisible") {
                    g_config.levelNumber.visible = ParseBool(value, g_config.levelNumber.visible);
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
            << "LayoutVersion=" << g_config.layoutVersion << "\n"
            << "HealthTextX=" << g_config.healthText.x << "\n"
            << "HealthTextY=" << g_config.healthText.y << "\n"
            << "HealthTextScale=" << g_config.healthText.scale << "\n"
            << "HealthTextLayer=" << g_config.healthText.layer << "\n"
            << "HealthTextVisible=" << (g_config.healthText.visible ? 1 : 0) << "\n"
            << "HealthBarX=" << g_config.healthBar.x << "\n"
            << "HealthBarY=" << g_config.healthBar.y << "\n"
            << "HealthBarScale=" << g_config.healthBar.scale << "\n"
            << "HealthBarLayer=" << g_config.healthBar.layer << "\n"
            << "HealthBarVisible=" << (g_config.healthBar.visible ? 1 : 0) << "\n"
            << "MagickaTextX=" << g_config.magickaText.x << "\n"
            << "MagickaTextY=" << g_config.magickaText.y << "\n"
            << "MagickaTextScale=" << g_config.magickaText.scale << "\n"
            << "MagickaTextLayer=" << g_config.magickaText.layer << "\n"
            << "MagickaTextVisible=" << (g_config.magickaText.visible ? 1 : 0) << "\n"
            << "MagickaBarX=" << g_config.magickaBar.x << "\n"
            << "MagickaBarY=" << g_config.magickaBar.y << "\n"
            << "MagickaBarScale=" << g_config.magickaBar.scale << "\n"
            << "MagickaBarLayer=" << g_config.magickaBar.layer << "\n"
            << "MagickaBarVisible=" << (g_config.magickaBar.visible ? 1 : 0) << "\n"
            << "StaminaX=" << g_config.stamina.x << "\n"
            << "StaminaY=" << g_config.stamina.y << "\n"
            << "StaminaScale=" << g_config.stamina.scale << "\n"
            << "StaminaLayer=" << g_config.stamina.layer << "\n"
            << "StaminaVisible=" << (g_config.stamina.visible ? 1 : 0) << "\n"
            << "ChargesX=" << g_config.charges.x << "\n"
            << "ChargesY=" << g_config.charges.y << "\n"
            << "ChargesScale=" << g_config.charges.scale << "\n"
            << "ChargesLayer=" << g_config.charges.layer << "\n"
            << "ChargesVisible=" << (g_config.charges.visible ? 1 : 0) << "\n"
            << "LevelBlankX=" << g_config.levelBlank.x << "\n"
            << "LevelBlankY=" << g_config.levelBlank.y << "\n"
            << "LevelBlankScale=" << g_config.levelBlank.scale << "\n"
            << "LevelBlankLayer=" << g_config.levelBlank.layer << "\n"
            << "LevelBlankVisible=" << (g_config.levelBlank.visible ? 1 : 0) << "\n"
            << "LevelFillX=" << g_config.levelFill.x << "\n"
            << "LevelFillY=" << g_config.levelFill.y << "\n"
            << "LevelFillScale=" << g_config.levelFill.scale << "\n"
            << "LevelFillLayer=" << g_config.levelFill.layer << "\n"
            << "LevelFillVisible=" << (g_config.levelFill.visible ? 1 : 0) << "\n"
            << "LevelAvailableX=" << g_config.levelAvailable.x << "\n"
            << "LevelAvailableY=" << g_config.levelAvailable.y << "\n"
            << "LevelAvailableScale=" << g_config.levelAvailable.scale << "\n"
            << "LevelAvailableLayer=" << g_config.levelAvailable.layer << "\n"
            << "LevelAvailableVisible=" << (g_config.levelAvailable.visible ? 1 : 0) << "\n"
            << "LevelPlateX=" << g_config.levelPlate.x << "\n"
            << "LevelPlateY=" << g_config.levelPlate.y << "\n"
            << "LevelPlateScale=" << g_config.levelPlate.scale << "\n"
            << "LevelPlateLayer=" << g_config.levelPlate.layer << "\n"
            << "LevelPlateVisible=" << (g_config.levelPlate.visible ? 1 : 0) << "\n"
            << "LevelNumberX=" << g_config.levelNumber.x << "\n"
            << "LevelNumberY=" << g_config.levelNumber.y << "\n"
            << "LevelNumberScale=" << g_config.levelNumber.scale << "\n"
            << "LevelNumberLayer=" << g_config.levelNumber.layer << "\n"
            << "LevelNumberVisible=" << (g_config.levelNumber.visible ? 1 : 0) << "\n";
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
        bool needsCoreLayoutMigration = false;

        if (std::filesystem::exists(g_userIniPath)) {
            needsCoreLayoutMigration = !FileContainsSetting(g_userIniPath, "LayoutVersion");
            ApplyIniFile(g_userIniPath);
        } else if (std::filesystem::exists(g_legacyUserIniPath)) {
            needsCoreLayoutMigration = true;
            ApplyIniFile(g_legacyUserIniPath);
            migrated = true;
        }

        ClampConfig();

        // v0.3.7 native-size calibration: reset old offsets for the original-size artwork; all normal HUD sections can be hidden, and
        // each Level layer is independently movable/scaleable/layered.
        if (needsCoreLayoutMigration || g_config.layoutVersion < 2) {
            g_config.left = 574;
            g_config.bottom = 32;
            migrated = true;
        }
        if (g_config.layoutVersion < 6) {
            g_config.healthText = { 0, 0, 100, 20, true };
            g_config.healthBar = { 0, 0, 100, 10, true };
            g_config.magickaText = { 0, 0, 100, 20, true };
            g_config.magickaBar = { 0, 0, 100, 10, true };
            g_config.stamina = { 0, 0, 100, 10, true };
            g_config.charges = { 0, 0, 100, 12, true };
            g_config.levelBlank = { 0, 0, 100, 10, true };
            g_config.levelFill = { 0, 0, 100, 12, true };
            g_config.levelAvailable = { 0, 0, 100, 14, true };
            g_config.levelPlate = { 0, 0, 100, 20, true };
            g_config.levelNumber = { 0, 0, 100, 30, true };
            g_config.layoutVersion = 6;
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
            SKSE::log::info("Writing migrated HelluvaHUD v0.3.7 native-size PNG calibration layout");
            SaveConfig();
        }
    }

    void ApplyLayoutToHUD()
    {
        if (!g_prisma || !g_domReady.load() || !g_view || !g_prisma->IsValid(g_view)) {
            return;
        }

        const auto script = std::format(
            "window.HelluvaHUD&&window.HelluvaHUD.setLayout({{"
            "left:{},bottom:{},scale:{},"
            "healthText:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "healthBar:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "magickaText:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "magickaBar:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "stamina:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "charges:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "levelBlank:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "levelFill:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "levelAvailable:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "levelPlate:{{x:{},y:{},scale:{},layer:{},visible:{}}},"
            "levelNumber:{{x:{},y:{},scale:{},layer:{},visible:{}}}"
            "}});",
            g_config.left, g_config.bottom, g_config.scale,
            g_config.healthText.x, g_config.healthText.y, g_config.healthText.scale, g_config.healthText.layer, g_config.healthText.visible ? "true" : "false",
            g_config.healthBar.x, g_config.healthBar.y, g_config.healthBar.scale, g_config.healthBar.layer, g_config.healthBar.visible ? "true" : "false",
            g_config.magickaText.x, g_config.magickaText.y, g_config.magickaText.scale, g_config.magickaText.layer, g_config.magickaText.visible ? "true" : "false",
            g_config.magickaBar.x, g_config.magickaBar.y, g_config.magickaBar.scale, g_config.magickaBar.layer, g_config.magickaBar.visible ? "true" : "false",
            g_config.stamina.x, g_config.stamina.y, g_config.stamina.scale, g_config.stamina.layer, g_config.stamina.visible ? "true" : "false",
            g_config.charges.x, g_config.charges.y, g_config.charges.scale, g_config.charges.layer, g_config.charges.visible ? "true" : "false",
            g_config.levelBlank.x, g_config.levelBlank.y, g_config.levelBlank.scale, g_config.levelBlank.layer, g_config.levelBlank.visible ? "true" : "false",
            g_config.levelFill.x, g_config.levelFill.y, g_config.levelFill.scale, g_config.levelFill.layer, g_config.levelFill.visible ? "true" : "false",
            g_config.levelAvailable.x, g_config.levelAvailable.y, g_config.levelAvailable.scale, g_config.levelAvailable.layer, g_config.levelAvailable.visible ? "true" : "false",
            g_config.levelPlate.x, g_config.levelPlate.y, g_config.levelPlate.scale, g_config.levelPlate.layer, g_config.levelPlate.visible ? "true" : "false",
            g_config.levelNumber.x, g_config.levelNumber.y, g_config.levelNumber.scale, g_config.levelNumber.layer, g_config.levelNumber.visible ? "true" : "false");
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
               std::fabs(a_state.xpProgress - g_lastState.xpProgress) >= 0.001f ||
               std::fabs(a_state.levelUpAvailable - g_lastState.levelUpAvailable) >= 0.001f;
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
            "level:{:.0f},xpProgress:{:.5f},levelUpAvailable:{:.0f}"
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
            a_state.xpProgress,
            a_state.levelUpAvailable);

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
            state.levelUpAvailable = player->GetGameStatsData().perkCount > 0 ? 1.0f : 0.0f;

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
            "HelluvaHUD placement calibration. Every visual section can be moved, scaled, layered or hidden.");

        ImGuiMCP::Spacing();
        bool changed = false;

        changed |= ImGuiMCP::Checkbox("Only show HUD in combat", &g_config.combatOnly);

        ImGuiMCP::Spacing();
        ImGuiMCP::Text("Whole HUD");
        ImGuiMCP::SetNextItemWidth(360.0f);
        changed |= ImGuiMCP::SliderInt("Horizontal position", &g_config.left, 0, 3840, "%d px");
        ImGuiMCP::SetNextItemWidth(360.0f);
        changed |= ImGuiMCP::SliderInt("Vertical position", &g_config.bottom, 0, 2160, "%d px");
        ImGuiMCP::SetNextItemWidth(360.0f);
        changed |= ImGuiMCP::SliderInt("HUD scale", &g_config.scale, 50, 250, "%d%%");

        auto moduleControls = [&](const char* a_name, ModuleLayout& a_module) {
            ImGuiMCP::Separator();
            ImGuiMCP::Text("%s", a_name);

            std::string showLabel = std::string("Show##") + a_name;
            std::string xLabel = std::string("X offset##") + a_name;
            std::string yLabel = std::string("Y offset##") + a_name;
            std::string scaleLabel = std::string("Scale##") + a_name;
            std::string layerLabel = std::string("Layer##") + a_name;

            changed |= ImGuiMCP::Checkbox(showLabel.c_str(), &a_module.visible);
            ImGuiMCP::SetNextItemWidth(360.0f);
            changed |= ImGuiMCP::SliderInt(xLabel.c_str(), &a_module.x, -1000, 1000, "%d px");
            ImGuiMCP::SetNextItemWidth(360.0f);
            changed |= ImGuiMCP::SliderInt(yLabel.c_str(), &a_module.y, -1000, 1000, "%d px");
            ImGuiMCP::SetNextItemWidth(360.0f);
            changed |= ImGuiMCP::SliderInt(scaleLabel.c_str(), &a_module.scale, 50, 200, "%d%%");
            ImGuiMCP::SetNextItemWidth(360.0f);
            changed |= ImGuiMCP::SliderInt(layerLabel.c_str(), &a_module.layer, 0, 100, "%d");
        };

        moduleControls("Health Text + Icon", g_config.healthText);
        moduleControls("Health Bar", g_config.healthBar);
        moduleControls("Magicka Text + Icon", g_config.magickaText);
        moduleControls("Magicka Bar", g_config.magickaBar);
        moduleControls("Stamina", g_config.stamina);
        moduleControls("Charges", g_config.charges);

        ImGuiMCP::Separator();
        ImGuiMCP::Text("Level layers");
        moduleControls("Level - Blank BG", g_config.levelBlank);
        moduleControls("Level - XP Fill", g_config.levelFill);
        moduleControls("Level - Available", g_config.levelAvailable);
        moduleControls("Level - Plate", g_config.levelPlate);
        moduleControls("Level - Number", g_config.levelNumber);

        if (changed) {
            CommitMenuChange();
        }

        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();

        if (ImGuiMCP::Button("Reset all HUD placement")) {
            g_config.left = 574;
            g_config.bottom = 32;
            g_config.scale = 100;
            g_config.healthText = { 0, 0, 100, 20, true };
            g_config.healthBar = { 0, 0, 100, 10, true };
            g_config.magickaText = { 0, 0, 100, 20, true };
            g_config.magickaBar = { 0, 0, 100, 10, true };
            g_config.stamina = { 0, 0, 100, 10, true };
            g_config.charges = { 0, 0, 100, 12, true };
            g_config.levelBlank = { 0, 0, 100, 10, true };
            g_config.levelFill = { 0, 0, 100, 12, true };
            g_config.levelAvailable = { 0, 0, 100, 14, true };
            g_config.levelPlate = { 0, 0, 100, 20, true };
            g_config.levelNumber = { 0, 0, 100, 30, true };
            g_config.layoutVersion = 5;
            CommitMenuChange();
        }

        ImGuiMCP::Separator();
        ImGuiMCP::TextWrapped(
            "When the layout is correct, send HelluvaHUD.user.ini from MO2 Overwrite\\SKSE\\Plugins. "
            "Every Show/X/Y/Scale/Layer value will be preserved as the final default composition.");
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

    SKSE::log::info("HelluvaHUD v0.3.7 native-size PNG calibration HUD loaded");
    return true;
}
