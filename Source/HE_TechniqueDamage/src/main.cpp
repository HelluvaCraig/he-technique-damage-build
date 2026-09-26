#include "pch.h"
#include "PrecisionAPI.h"
#include "TechniqueDamageMap.h"

namespace
{
    constexpr auto kStonePlugin = "HE Elden Rim - Ash Rings.esp";
    constexpr auto kCooldownPlugin = "HE Elden Rim - Ash Cooldown.esp";
    constexpr RE::FormID kTechniqueMarkerLocalID = 0x920;

    RE::EffectSetting* g_techniqueMarker = nullptr;
    std::unordered_map<RE::FormID, const TechniqueDamageDefinition*> g_damageByStone;
    PRECISION_API::IVPrecision1* g_precision = nullptr;

    const TechniqueDamageDefinition* GetEquippedPhysicalStone(RE::Actor* a_actor)
    {
        if (!a_actor || g_damageByStone.empty()) {
            return nullptr;
        }

        const auto inventory = a_actor->GetInventory([](RE::TESBoundObject& a_object) {
            return a_object.IsArmor();
        });

        for (const auto& [item, data] : inventory) {
            const auto& [count, entry] = data;
            if (count <= 0 || !entry || !entry->IsWorn()) {
                continue;
            }
            if (const auto it = g_damageByStone.find(item->GetFormID()); it != g_damageByStone.end()) {
                return it->second;
            }
        }
        return nullptr;
    }

    bool TechniqueMarkerActive(RE::Actor* a_actor)
    {
        if (!a_actor || !g_techniqueMarker) {
            return false;
        }

        auto* magicTarget = a_actor->AsMagicTarget();
        auto* effects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!effects) {
            return false;
        }

        for (const auto* effect : *effects) {
            if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
                continue;
            }
            if (effect->GetBaseObject() == g_techniqueMarker) {
                return true;
            }
        }
        return false;
    }

    bool ShouldTrace(const TechniqueDamageDefinition* a_def)
    {
        return a_def &&
            (a_def->localFormID == 0x80F ||   // Hawkfall Dance
             a_def->localFormID == 0x843 ||   // Storm Fist
             a_def->localFormID == 0x845);    // Worldshaker control
    }

    float GetNativeAttackDataMultiplier(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 1.0f;
        }

        const auto* process = a_actor->GetActorRuntimeData().currentProcess;
        if (!process || !process->high || !process->high->attackData) {
            return 1.0f;
        }

        const float native = process->high->attackData->data.damageMult;
        if (!std::isfinite(native) || std::fabs(native) < 0.001f) {
            return 1.0f;
        }
        return native;
    }

    PRECISION_API::PreHitCallbackReturn OnPrecisionPreHit(const PRECISION_API::PrecisionHitData& a_hit)
    {
        PRECISION_API::PreHitCallbackReturn result{};

        auto* attacker = a_hit.attacker;
        if (!attacker || !attacker->IsPlayerRef() || !TechniqueMarkerActive(attacker)) {
            return result;
        }

        const auto* def = GetEquippedPhysicalStone(attacker);
        if (!def || def->contacts == 0) {
            return result;
        }

        const float perContactShare = def->tierMultiplier / static_cast<float>(def->contacts);

        // Precision multiplies this modifier into Skyrim's weapon-damage path.
        // Divide away the animation's native attack-data multiplier so the
        // Technique starts from the equipped weapon's calculated damage.
        const float nativeAttackMult = GetNativeAttackDataMultiplier(attacker);
        const float precisionMultiplier = std::clamp(perContactShare / nativeAttackMult, 0.01f, 100.0f);

        if (ShouldTrace(def)) {
            SKSE::log::info(
                "[TRACE PRE] {} form={:X} tier={} contacts={} share={:.4f} currentAttackMult={:.4f} appliedPrecisionMult={:.4f}",
                def->name, def->localFormID, def->tierMultiplier, def->contacts,
                perContactShare, nativeAttackMult, precisionMultiplier);
        }

        result.modifiers.push_back({
            PRECISION_API::PreHitModifier::ModifierType::Damage,
            PRECISION_API::PreHitModifier::ModifierOperation::Multiplicative,
            precisionMultiplier
        });

        return result;
    }

    void OnPrecisionPostHit(const PRECISION_API::PrecisionHitData& a_precisionHit, const RE::HitData& a_hit)
    {
        auto* attacker = a_precisionHit.attacker;
        if (!attacker || !attacker->IsPlayerRef() || !TechniqueMarkerActive(attacker)) {
            return;
        }

        const auto* def = GetEquippedPhysicalStone(attacker);
        if (!ShouldTrace(def)) {
            return;
        }

        const float actualAttackMult = a_hit.attackData ? a_hit.attackData->data.damageMult : -1.0f;
        const bool actualLeftAttack = a_hit.attackData ? a_hit.attackData->IsLeftAttack() : false;
        const RE::FormID weaponFormID = a_hit.weapon ? a_hit.weapon->GetFormID() : 0;

        SKSE::log::info(
            "[TRACE POST] {} form={:X} weapon={:08X} leftAttack={} actualAttackMult={:.4f} totalDamage={:.4f} physicalDamage={:.4f} resistedPhysical={:.4f}",
            def->name, def->localFormID, weaponFormID, actualLeftAttack,
            actualAttackMult, a_hit.totalDamage, a_hit.physicalDamage, a_hit.resistedPhysicalDamage);
    }

    void ResolveForms()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            SKSE::log::error("TESDataHandler unavailable");
            return;
        }

        g_damageByStone.clear();
        std::size_t resolved = 0;
        for (const auto& def : kTechniqueDamageDefinitions) {
            if (auto* stone = dataHandler->LookupForm<RE::TESObjectARMO>(def.localFormID, kStonePlugin)) {
                g_damageByStone.emplace(stone->GetFormID(), &def);
                ++resolved;
            }
        }

        g_techniqueMarker = dataHandler->LookupForm<RE::EffectSetting>(kTechniqueMarkerLocalID, kCooldownPlugin);
        SKSE::log::info("Resolved {}/{} physical Technique Stones", resolved, kTechniqueDamageDefinitions.size());
        SKSE::log::info("Technique marker {}", g_techniqueMarker ? "resolved" : "NOT resolved");
    }

    void RegisterPrecision()
    {
        if (g_precision) {
            return;
        }

        g_precision = PRECISION_API::RequestPluginAPI();
        if (!g_precision) {
            SKSE::log::error("Precision API unavailable - physical Technique replacement damage disabled");
            return;
        }

        const auto preRes = g_precision->AddPreHitCallback(SKSE::GetPluginHandle(), OnPrecisionPreHit);
        if (preRes == PRECISION_API::APIResult::OK || preRes == PRECISION_API::APIResult::AlreadyRegistered) {
            SKSE::log::info("Precision pre-hit Technique damage callback registered");
        } else {
            SKSE::log::error("Precision pre-hit callback registration failed ({})", static_cast<int>(preRes));
        }

        const auto postRes = g_precision->AddPostHitCallback(SKSE::GetPluginHandle(), OnPrecisionPostHit);
        if (postRes == PRECISION_API::APIResult::OK || postRes == PRECISION_API::APIResult::AlreadyRegistered) {
            SKSE::log::info("Precision post-hit diagnostic callback registered");
        } else {
            SKSE::log::error("Precision post-hit callback registration failed ({})", static_cast<int>(postRes));
        }
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        switch (a_message->type) {
        case SKSE::MessagingInterface::kPostLoad:
            RegisterPrecision();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            ResolveForms();
            RegisterPrecision();
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    if (auto path = SKSE::log::log_directory()) {
        *path /= "HE_TechniqueDamage.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("HE_TechniqueDamage", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }

    SKSE::Init(a_skse);

    if (const auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener("SKSE", MessageHandler);
    } else {
        return false;
    }

    SKSE::log::info("HE Technique Damage v0.1.0 loaded");
    return true;
}
