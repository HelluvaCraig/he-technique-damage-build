#include "pch.h"
#include "PrecisionAPI.h"
#include "TechniqueDamageMap.h"
#include "MagicTechniqueMap.h"

namespace
{
    constexpr auto kStonePlugin = "HE Elden Rim - Ash Rings.esp";
    constexpr auto kCooldownPlugin = "HE Elden Rim - Ash Cooldown.esp";
    constexpr RE::FormID kTechniqueMarkerLocalID = 0x920;

    constexpr auto kSkyrimPlugin = "Skyrim.esm";
    constexpr RE::FormID kOakfleshLocalID = 0x5AD5C;
    constexpr RE::FormID kStonefleshLocalID = 0x5AD5D;
    constexpr RE::FormID kIronfleshLocalID = 0x51B16;

    RE::EffectSetting* g_techniqueMarker = nullptr;
    std::unordered_map<RE::FormID, const TechniqueDamageDefinition*> g_damageByStone;
    std::unordered_map<RE::FormID, const MagicTechniqueDefinition*> g_magicByStone;
    std::unordered_map<RE::FormID, const char*> g_magicActivationBySpell;

    RE::SpellItem* g_tier1AlterationProbe = nullptr;
    RE::SpellItem* g_tier2AlterationProbe = nullptr;
    RE::SpellItem* g_tier3AlterationProbe = nullptr;

    PRECISION_API::IVPrecision1* g_precision = nullptr;
    bool g_spellCastSinkRegistered = false;
    bool g_magicEffectSinkRegistered = false;

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

    const MagicTechniqueDefinition* GetEquippedMagicStone(RE::Actor* a_actor)
    {
        if (!a_actor || g_magicByStone.empty()) {
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
            if (const auto it = g_magicByStone.find(item->GetFormID()); it != g_magicByStone.end()) {
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
        // Keep diagnostics enabled for every physical Technique during the
        // contact-count validation pass. This does not change damage.
        return a_def != nullptr;
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

    RE::SpellItem* GetAlterationProbe(std::uint8_t a_tier)
    {
        switch (a_tier) {
        case 1:
            return g_tier1AlterationProbe;
        case 2:
            return g_tier2AlterationProbe;
        case 3:
            return g_tier3AlterationProbe;
        default:
            return nullptr;
        }
    }

    const char* GetAlterationProbeName(std::uint8_t a_tier)
    {
        switch (a_tier) {
        case 1:
            return "Oakflesh/Novice";
        case 2:
            return "Stoneflesh/Apprentice";
        case 3:
            return "Ironflesh/Adept";
        default:
            return "missing";
        }
    }

    float CalculateAlterationAdjustedTechniqueCost(
        RE::Actor* a_actor,
        const MagicTechniqueDefinition& a_def,
        float& a_naturalProbeCost)
    {
        a_naturalProbeCost = -1.0f;

        auto* probe = GetAlterationProbe(a_def.tier);
        if (!a_actor || !probe) {
            return a_def.baseMagicka;
        }

        // The loaded Skyrim spell is already the final winning record after
        // Adamant/Mysticism/other overrides. Temporarily replace only its base
        // cost, ask Skyrim for the final player-adjusted cost, then restore the
        // spell immediately. Nothing is permanently changed by this proof.
        a_naturalProbeCost = probe->CalculateMagickaCost(a_actor);

        const auto oldCostOverride = probe->data.costOverride;
        const auto oldFlags = probe->data.flags;

        probe->data.costOverride = static_cast<std::int32_t>(std::lround(a_def.baseMagicka));
        probe->data.flags.set(RE::SpellItem::SpellFlag::kCostOverride);

        const float adjusted = probe->CalculateMagickaCost(a_actor);

        probe->data.costOverride = oldCostOverride;
        probe->data.flags = oldFlags;

        if (!std::isfinite(adjusted) || adjusted < 0.0f) {
            return a_def.baseMagicka;
        }

        return adjusted;
    }

    void LogMagicCostProof(
        RE::Actor* a_actor,
        const MagicTechniqueDefinition& a_def,
        RE::FormID a_signalFormID,
        const char* a_signalLabel)
    {
        float naturalProbeCost = -1.0f;
        const float adjustedCost = CalculateAlterationAdjustedTechniqueCost(
            a_actor,
            a_def,
            naturalProbeCost);

        SKSE::log::info(
            "[MAGIC PROOF] Technique={} stone={:03X} tier={} damageBudget={:.1f}x "
            "baseMagicka={:.1f} probe={} naturalProbeCost={:.2f} "
            "adjustedTechniqueCost={:.2f} signal={} ({:08X})",
            a_def.name,
            a_def.localFormID,
            a_def.tier,
            a_def.tierMultiplier,
            a_def.baseMagicka,
            GetAlterationProbeName(a_def.tier),
            naturalProbeCost,
            adjustedCost,
            a_signalLabel ? a_signalLabel : "unknown",
            a_signalFormID);
    }

    class MagicSpellCastSink final : public RE::BSTEventSink<RE::TESSpellCastEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESSpellCastEvent* a_event,
            RE::BSTEventSource<RE::TESSpellCastEvent>*) override
        {
            if (!a_event || !a_event->object || a_event->spell == 0) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* ref = a_event->object.get();
            if (!ref || !ref->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const auto* def = GetEquippedMagicStone(player);
            if (!def) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const auto signalIt = g_magicActivationBySpell.find(a_event->spell);
            if (signalIt != g_magicActivationBySpell.end()) {
                SKSE::log::info(
                    "[MAGIC SIGNAL SPELL] Technique={} stone={:03X} spell={:08X} label={}",
                    def->name,
                    def->localFormID,
                    a_event->spell,
                    signalIt->second);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class MagicEffectApplySink final : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESMagicEffectApplyEvent* a_event,
            RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
        {
            if (!a_event || !a_event->caster || !a_event->target || a_event->magicEffect == 0 || !g_techniqueMarker) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* caster = a_event->caster.get();
            auto* target = a_event->target.get();
            if (!caster || !caster->IsPlayerRef() || !target || !target->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // The cooldown/Technique marker is already the shared, reliable signal
            // used by the physical damage layer. Treat its application to the
            // player as the once-per-activation Magic Technique trigger.
            if (a_event->magicEffect != g_techniqueMarker->GetFormID()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return RE::BSEventNotifyControl::kContinue;
            }

            const auto* def = GetEquippedMagicStone(player);
            if (!def) {
                return RE::BSEventNotifyControl::kContinue;
            }

            SKSE::log::info(
                "[MAGIC ACTIVATION] Technique={} stone={:03X} marker={:08X}",
                def->name,
                def->localFormID,
                a_event->magicEffect);

            LogMagicCostProof(
                player,
                *def,
                a_event->magicEffect,
                "Technique marker activation");

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void RegisterMagicDiagnosticSinks()
    {
        auto* source = RE::ScriptEventSourceHolder::GetSingleton();
        if (!source) {
            SKSE::log::error("ScriptEventSourceHolder unavailable - Magic event diagnostics disabled");
            return;
        }

        if (!g_spellCastSinkRegistered) {
            static MagicSpellCastSink spellSink;
            source->AddEventSink<RE::TESSpellCastEvent>(&spellSink);
            g_spellCastSinkRegistered = true;
            SKSE::log::info("Magic Technique spell-cast diagnostic sink registered");
        }

        if (!g_magicEffectSinkRegistered) {
            static MagicEffectApplySink effectSink;
            source->AddEventSink<RE::TESMagicEffectApplyEvent>(&effectSink);
            g_magicEffectSinkRegistered = true;
            SKSE::log::info("Magic Technique magic-effect diagnostic sink registered");
        }
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

        g_magicByStone.clear();
        std::size_t magicResolved = 0;
        for (const auto& def : kMagicTechniqueDefinitions) {
            if (auto* stone = dataHandler->LookupForm<RE::TESObjectARMO>(def.localFormID, kStonePlugin)) {
                g_magicByStone.emplace(stone->GetFormID(), &def);
                ++magicResolved;
            }
        }

        g_magicActivationBySpell.clear();
        std::size_t activationResolved = 0;
        for (const auto& signal : kMagicActivationSignals) {
            if (auto* spell = dataHandler->LookupForm<RE::SpellItem>(signal.localFormID, signal.plugin)) {
                g_magicActivationBySpell.emplace(spell->GetFormID(), signal.label);
                ++activationResolved;
            } else {
                SKSE::log::warn(
                    "Magic activation signal NOT resolved: {} {:06X} ({})",
                    signal.plugin,
                    signal.localFormID,
                    signal.label);
            }
        }

        g_tier1AlterationProbe = dataHandler->LookupForm<RE::SpellItem>(kOakfleshLocalID, kSkyrimPlugin);
        g_tier2AlterationProbe = dataHandler->LookupForm<RE::SpellItem>(kStonefleshLocalID, kSkyrimPlugin);
        g_tier3AlterationProbe = dataHandler->LookupForm<RE::SpellItem>(kIronfleshLocalID, kSkyrimPlugin);

        g_techniqueMarker = dataHandler->LookupForm<RE::EffectSetting>(kTechniqueMarkerLocalID, kCooldownPlugin);

        SKSE::log::info("Resolved {}/{} physical Technique Stones", resolved, kTechniqueDamageDefinitions.size());
        SKSE::log::info("Resolved {}/{} Magic/Rune Technique Stones", magicResolved, kMagicTechniqueDefinitions.size());
        SKSE::log::info("Resolved {}/{} Magic activation signals", activationResolved, kMagicActivationSignals.size());
        SKSE::log::info(
            "Alteration probes: T1={} T2={} T3={}",
            g_tier1AlterationProbe ? "Oakflesh" : "MISSING",
            g_tier2AlterationProbe ? "Stoneflesh" : "MISSING",
            g_tier3AlterationProbe ? "Ironflesh" : "MISSING");
        if (g_techniqueMarker) {
            SKSE::log::info("Technique marker resolved runtimeForm={:08X}", g_techniqueMarker->GetFormID());
        } else {
            SKSE::log::info("Technique marker NOT resolved");
        }
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
            RegisterMagicDiagnosticSinks();
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

    SKSE::log::info("HE Technique Damage v0.2.2 Magic marker activation proof loaded");
    return true;
}
