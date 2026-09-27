#include "pch.h"
#include "PrecisionAPI.h"
#include "TechniqueDamageMap.h"
#include "MagicTechniqueMap.h"

namespace
{
    constexpr auto kStonePlugin = "HE Elden Rim - Ash Rings.esp";
    constexpr auto kCooldownPlugin = "HE Elden Rim - Ash Cooldown.esp";
    constexpr auto kRimSkillsPlugin = "EldenSkyrim_RimSkills.esp";
    constexpr auto kEldenSkyrimPlugin = "EldenSkyrim.esp";
    constexpr RE::FormID kTechniqueMarkerLocalID = 0x920;
    constexpr RE::FormID kMagicCandidatePrimaryLocalID = 0x800;
    constexpr RE::FormID kMagicCandidateSecondaryLocalID = 0x805;

    constexpr auto kSkyrimPlugin = "Skyrim.esm";
    constexpr RE::FormID kOakfleshLocalID = 0x5AD5C;
    constexpr RE::FormID kStonefleshLocalID = 0x5AD5D;
    constexpr RE::FormID kIronfleshLocalID = 0x51B16;

    // v0.3.0 representative Magic damage proof payload spells.
    constexpr RE::FormID kGaleCrescentDamageSpellLocalID = 0x0B43D;
    constexpr RE::FormID kDragonfireSigilDamageSpellLocalID = 0x000F27;
    constexpr RE::FormID kRadiantTriplecutSpell1LocalID = 0x0BA97;
    constexpr RE::FormID kRadiantTriplecutSpell2LocalID = 0x0BA99;
    constexpr RE::FormID kRadiantTriplecutSpell3LocalID = 0x0BA98;

    constexpr RE::FormID kMoonlitCleaveDamageSpellLocalID = 0x0BA1B;
    constexpr RE::FormID kMoonlitSeveranceSpell1LocalID = 0x0BA0D;
    constexpr RE::FormID kMoonlitSeveranceSpell2LocalID = 0x0BA0E;
    constexpr RE::FormID kMoonshardSigilDamageSpellLocalID = 0x0BA70;
    constexpr RE::FormID kMoonshardSigilExtraSpellLocalID = 0x0B44E;

    constexpr RE::FormID kElderMooncleaveDamageSpellLocalID = 0x0BA011;
    constexpr RE::FormID kCrimsonSeveranceSpell1LocalID = 0x000B35;
    constexpr RE::FormID kCrimsonSeveranceSpell3LocalID = 0x000B3B;
    constexpr RE::FormID kCrimsonSeveranceSpell4LocalID = 0x000B3C;
    constexpr RE::FormID kRadiantBladeDanceSpell30LocalID = 0x0BA34;
    constexpr RE::FormID kRadiantBladeDanceSpell60LocalID = 0x0BA35;
    constexpr RE::FormID kRadiantBladeDanceSpell80LocalID = 0x0BA36;
    constexpr RE::FormID kRadiantBladeDanceSpell120LocalID = 0x0BA37;
    constexpr RE::FormID kRadiantBladeDanceSpell150LocalID = 0x0BA38;
    constexpr RE::FormID kRadiantBladeDanceFinalLocalID = 0x0BA39;
    constexpr RE::FormID kRadiantCarianImpactSpellLocalID = 0x000B0A;
    constexpr RE::FormID kRadiantFinisherSkuldafnEffectLocalID = 0x000B02;
    constexpr RE::FormID kRadiantFinisherDragonrendEffectLocalID = 0x000AFF;
    constexpr RE::FormID kRadiantFinisherCarianImpactEffectLocalID = 0x000AF8;
    constexpr RE::FormID kRadiantFinisherExtraDamageEffectLocalID = 0x000AFA;
    constexpr float kRadiantFinisherDamageScale = 0.15f;

    RE::EffectSetting* g_techniqueMarker = nullptr;
    RE::EffectSetting* g_magicCandidatePrimary = nullptr;
    RE::EffectSetting* g_magicCandidateSecondary = nullptr;
    std::unordered_map<RE::FormID, const TechniqueDamageDefinition*> g_damageByStone;
    std::unordered_map<RE::FormID, const MagicTechniqueDefinition*> g_magicByStone;
    std::unordered_map<RE::FormID, const char*> g_magicActivationBySpell;

    RE::SpellItem* g_tier1AlterationProbe = nullptr;
    RE::SpellItem* g_tier2AlterationProbe = nullptr;
    RE::SpellItem* g_tier3AlterationProbe = nullptr;

    RE::SpellItem* g_galeCrescentDamageSpell = nullptr;
    RE::SpellItem* g_dragonfireSigilDamageSpell = nullptr;
    RE::SpellItem* g_radiantTriplecutSpell1 = nullptr;
    RE::SpellItem* g_radiantTriplecutSpell2 = nullptr;
    RE::SpellItem* g_radiantTriplecutSpell3 = nullptr;

    RE::SpellItem* g_moonlitCleaveDamageSpell = nullptr;
    RE::SpellItem* g_moonlitSeveranceSpell1 = nullptr;
    RE::SpellItem* g_moonlitSeveranceSpell2 = nullptr;
    RE::SpellItem* g_moonshardSigilDamageSpell = nullptr;
    RE::SpellItem* g_moonshardSigilExtraSpell = nullptr;

    RE::SpellItem* g_elderMooncleaveDamageSpell = nullptr;
    RE::SpellItem* g_crimsonSeveranceSpell1 = nullptr;
    RE::SpellItem* g_crimsonSeveranceSpell3 = nullptr;
    RE::SpellItem* g_crimsonSeveranceSpell4 = nullptr;
    RE::SpellItem* g_radiantBladeDanceSpell30 = nullptr;
    RE::SpellItem* g_radiantBladeDanceSpell60 = nullptr;
    RE::SpellItem* g_radiantBladeDanceSpell80 = nullptr;
    RE::SpellItem* g_radiantBladeDanceSpell120 = nullptr;
    RE::SpellItem* g_radiantBladeDanceSpell150 = nullptr;
    RE::SpellItem* g_radiantBladeDanceFinal = nullptr;
    RE::SpellItem* g_radiantCarianImpactSpell = nullptr;

    struct RadiantFinisherEffectRef
    {
        RE::Effect* effect{ nullptr };
        float nativeMagnitude{ 0.0f };
        RE::FormID spellFormID{ 0 };
        RE::FormID effectFormID{ 0 };
    };

    std::vector<RadiantFinisherEffectRef> g_radiantFinisherDamageEffects;
    RE::BGSExplosion* g_radiantFinisherProjectileExplosion = nullptr;
    float g_radiantFinisherProjectileExplosionNativeDamage = 0.0f;

    PRECISION_API::IVPrecision1* g_precision = nullptr;
    bool g_spellCastSinkRegistered = false;
    bool g_magicEffectSinkRegistered = false;
    bool g_activeEffectSinkRegistered = false;

    std::chrono::steady_clock::time_point g_lastMagicActivationTime{};
    RE::FormID g_lastMagicActivationStone = 0;

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

        const float actualAttackMult = a_hit.attackData ? a_hit.attackData->data.damageMult : -1.0f;
        const bool actualLeftAttack = a_hit.attackData ? a_hit.attackData->IsLeftAttack() : false;
        const RE::FormID weaponFormID = a_hit.weapon ? a_hit.weapon->GetFormID() : 0;
        const auto* def = GetEquippedPhysicalStone(attacker);
        if (ShouldTrace(def)) {
            SKSE::log::info(
                "[TRACE POST] {} form={:X} weapon={:08X} leftAttack={} actualAttackMult={:.4f} totalDamage={:.4f} physicalDamage={:.4f} resistedPhysical={:.4f}",
                def->name, def->localFormID, weaponFormID, actualLeftAttack,
                actualAttackMult, a_hit.totalDamage, a_hit.physicalDamage, a_hit.resistedPhysicalDamage);
            return;
        }

        const auto* magicDef = GetEquippedMagicStone(attacker);
        if (magicDef && magicDef->localFormID == 0x832) {
            SKSE::log::info(
                "[RADIANT PHYSICAL TRACE] weapon={:08X} leftAttack={} actualAttackMult={:.4f} totalDamage={:.4f} physicalDamage={:.4f} resistedPhysical={:.4f}",
                weaponFormID,
                actualLeftAttack,
                actualAttackMult,
                a_hit.totalDamage,
                a_hit.physicalDamage,
                a_hit.resistedPhysicalDamage);
        }
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

    const char* GetMagicRankName(std::uint8_t a_tier)
    {
        switch (a_tier) {
        case 1:
            return "Adept";
        case 2:
            return "Expert";
        case 3:
            return "Master";
        default:
            return "Unknown";
        }
    }

    float GetMagicRankBaseDamage(std::uint8_t a_tier)
    {
        switch (a_tier) {
        case 1:
            return 40.0f;
        case 2:
            return 60.0f;
        case 3:
            return 80.0f;
        default:
            return 0.0f;
        }
    }

    float GetAlterationDamageMultiplier(RE::Actor* a_actor, float& a_alterationSkill)
    {
        a_alterationSkill = 15.0f;
        if (a_actor) {
            if (auto* avOwner = a_actor->AsActorValueOwner()) {
                a_alterationSkill = avOwner->GetActorValue(RE::ActorValue::kAlteration);
            }
        }

        // Technique magic starts at full base output at Alteration 15 and
        // rises linearly to +60% at Alteration 100.
        const float normalized = std::clamp((a_alterationSkill - 15.0f) / 85.0f, 0.0f, 1.0f);
        return 1.0f + (0.60f * normalized);
    }

    bool SetSpellEffectMagnitude(
        RE::SpellItem* a_spell,
        std::size_t a_effectIndex,
        float a_magnitude,
        const char* a_label)
    {
        if (!a_spell || a_effectIndex >= a_spell->effects.size()) {
            SKSE::log::warn(
                "[MAGIC DAMAGE CONFIG] {} unresolved/missing effect index {}",
                a_label ? a_label : "spell",
                a_effectIndex);
            return false;
        }

        auto* effect = a_spell->effects[a_effectIndex];
        if (!effect) {
            SKSE::log::warn(
                "[MAGIC DAMAGE CONFIG] {} null effect index {}",
                a_label ? a_label : "spell",
                a_effectIndex);
            return false;
        }

        const float before = effect->effectItem.magnitude;
        effect->effectItem.magnitude = a_magnitude;

        SKSE::log::info(
            "[MAGIC DAMAGE PAYLOAD] {} spell={:08X} effectIndex={} effect={:08X} magnitudeBefore={:.2f} magnitudeAfter={:.2f}",
            a_label ? a_label : "spell",
            a_spell->GetFormID(),
            a_effectIndex,
            effect->baseEffect ? effect->baseEffect->GetFormID() : 0,
            before,
            effect->effectItem.magnitude);
        return true;
    }

    bool IsRadiantFinisherDamageEffectLocalID(RE::FormID a_localID)
    {
        switch (a_localID) {
        case kRadiantFinisherSkuldafnEffectLocalID:
        case kRadiantFinisherDragonrendEffectLocalID:
        case kRadiantFinisherCarianImpactEffectLocalID:
        case kRadiantFinisherExtraDamageEffectLocalID:
            return true;
        default:
            return false;
        }
    }

    void ResolveRadiantFinisherDamagePayloads(RE::TESDataHandler* a_dataHandler)
    {
        g_radiantFinisherDamageEffects.clear();
        g_radiantFinisherProjectileExplosion = nullptr;
        g_radiantFinisherProjectileExplosionNativeDamage = 0.0f;

        if (!a_dataHandler) {
            return;
        }

        for (auto* spell : a_dataHandler->GetFormArray<RE::SpellItem>()) {
            if (!spell) {
                continue;
            }

            const auto* spellFile = spell->GetFile();
            if (!spellFile || std::string_view(spellFile->GetFilename()) != kRimSkillsPlugin) {
                continue;
            }

            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect) {
                    continue;
                }

                auto* baseEffect = effect->baseEffect;
                const RE::FormID localID = baseEffect->GetFormID() & 0x00FFFFFF;
                if (!IsRadiantFinisherDamageEffectLocalID(localID)) {
                    continue;
                }

                g_radiantFinisherDamageEffects.push_back({
                    effect,
                    effect->effectItem.magnitude,
                    spell->GetFormID(),
                    baseEffect->GetFormID()
                });

                SKSE::log::info(
                    "[RADIANT FINISHER RESOLVE] spell={:08X} spellName={} effect={:08X} effectName={} nativeMagnitude={:.3f}",
                    spell->GetFormID(),
                    spell->GetName(),
                    baseEffect->GetFormID(),
                    baseEffect->GetName(),
                    effect->effectItem.magnitude);

                if (localID == kRadiantFinisherCarianImpactEffectLocalID &&
                    baseEffect->data.projectileBase &&
                    baseEffect->data.projectileBase->data.explosionType &&
                    !g_radiantFinisherProjectileExplosion) {
                    g_radiantFinisherProjectileExplosion = baseEffect->data.projectileBase->data.explosionType;
                    g_radiantFinisherProjectileExplosionNativeDamage =
                        g_radiantFinisherProjectileExplosion->data.damage;

                    SKSE::log::info(
                        "[RADIANT FINISHER RESOLVE] projectileExplosion={:08X} nativeDamage={:.3f}",
                        g_radiantFinisherProjectileExplosion->GetFormID(),
                        g_radiantFinisherProjectileExplosionNativeDamage);
                }
            }
        }

        SKSE::log::info(
            "[RADIANT FINISHER RESOLVE] damageEffectRefs={} explosionResolved={}",
            g_radiantFinisherDamageEffects.size(),
            g_radiantFinisherProjectileExplosion != nullptr);
    }

    void ConfigureRadiantFinisherDamageScale()
    {
        for (auto& ref : g_radiantFinisherDamageEffects) {
            if (!ref.effect) {
                continue;
            }

            const float before = ref.effect->effectItem.magnitude;
            const float after = ref.nativeMagnitude * kRadiantFinisherDamageScale;
            ref.effect->effectItem.magnitude = after;

            SKSE::log::info(
                "[RADIANT FINISHER BALANCE] spell={:08X} effect={:08X} magnitudeBefore={:.3f} native={:.3f} magnitudeAfter={:.3f} scale={:.3f}",
                ref.spellFormID,
                ref.effectFormID,
                before,
                ref.nativeMagnitude,
                after,
                kRadiantFinisherDamageScale);
        }

        if (g_radiantFinisherProjectileExplosion) {
            const float before = g_radiantFinisherProjectileExplosion->data.damage;
            const float after = g_radiantFinisherProjectileExplosionNativeDamage * kRadiantFinisherDamageScale;
            g_radiantFinisherProjectileExplosion->data.damage = after;

            SKSE::log::info(
                "[RADIANT FINISHER BALANCE] projectileExplosion={:08X} damageBefore={:.3f} native={:.3f} damageAfter={:.3f} scale={:.3f}",
                g_radiantFinisherProjectileExplosion->GetFormID(),
                before,
                g_radiantFinisherProjectileExplosionNativeDamage,
                after,
                kRadiantFinisherDamageScale);
        }
    }

    void DumpRadiantCarianImpactPayload()
    {
        auto* spell = g_radiantCarianImpactSpell;
        if (!spell) {
            SKSE::log::warn("[RADIANT FINISHER DUMP] Carian Impact spell unresolved");
            return;
        }

        SKSE::log::info(
            "[RADIANT FINISHER DUMP] spell={:08X} name={} effects={}",
            spell->GetFormID(),
            spell->GetName(),
            spell->effects.size());

        for (std::size_t i = 0; i < spell->effects.size(); ++i) {
            auto* effect = spell->effects[i];
            if (!effect) {
                SKSE::log::info("[RADIANT FINISHER EFFECT] index={} NULL", i);
                continue;
            }

            auto* mgef = effect->baseEffect;
            auto* projectile = mgef ? mgef->data.projectileBase : nullptr;
            auto* projectileExplosion = projectile ? projectile->data.explosionType : nullptr;
            auto* directExplosion = mgef ? mgef->data.explosion : nullptr;

            SKSE::log::info(
                "[RADIANT FINISHER EFFECT] index={} mgef={:08X} name={} magnitude={:.3f} duration={} area={} "
                "archetype={} primaryAV={} hostile={} detrimental={} projectile={:08X} "
                "projectileExplosion={:08X} projectileExplosionDamage={:.3f} directExplosion={:08X} directExplosionDamage={:.3f}",
                i,
                mgef ? mgef->GetFormID() : 0,
                mgef ? mgef->GetName() : "",
                effect->effectItem.magnitude,
                effect->effectItem.duration,
                effect->effectItem.area,
                mgef ? static_cast<std::uint32_t>(mgef->data.archetype) : 0,
                mgef ? static_cast<std::uint32_t>(mgef->data.primaryAV) : 0,
                mgef ? mgef->IsHostile() : false,
                mgef ? mgef->IsDetrimental() : false,
                projectile ? projectile->GetFormID() : 0,
                projectileExplosion ? projectileExplosion->GetFormID() : 0,
                projectileExplosion ? projectileExplosion->data.damage : -1.0f,
                directExplosion ? directExplosion->GetFormID() : 0,
                directExplosion ? directExplosion->data.damage : -1.0f);
        }
    }

    void ConfigureRepresentativeMagicDamage(
        RE::Actor* a_actor,
        const MagicTechniqueDefinition& a_def)
    {
        float alterationSkill = 15.0f;
        const float alterationMult = GetAlterationDamageMultiplier(a_actor, alterationSkill);
        const float baseBudget = GetMagicRankBaseDamage(a_def.tier);
        const float scaledBudget = baseBudget * alterationMult;

        bool configured = false;

        switch (a_def.localFormID) {
        case 0x87A:  // Gale Crescent: B43D spell -> B43E hostile Health projectile.
            configured = SetSpellEffectMagnitude(
                g_galeCrescentDamageSpell,
                0,
                scaledBudget,
                "Gale Crescent / Solitary Moon blast");
            break;

        case 0x847: {  // Dragonfire Sigil: native payload fires three times.
            // One Expert/Master budget belongs to the whole Technique, so divide
            // the native 40 instant + 4/sec x5 shape across all three pulses.
            constexpr float kDragonfirePulses = 3.0f;
            const float perPulseBudget = scaledBudget / kDragonfirePulses;
            const float instantDamage = perPulseBudget * (40.0f / 60.0f);
            const float burnPerSecond = perPulseBudget / 15.0f;  // remaining 1/3 over 5 sec

            const bool instant = SetSpellEffectMagnitude(
                g_dragonfireSigilDamageSpell,
                0,
                instantDamage,
                "Dragonfire Sigil / Fire Blast");
            const bool burn = SetSpellEffectMagnitude(
                g_dragonfireSigilDamageSpell,
                1,
                burnPerSecond,
                "Dragonfire Sigil / Flame Smite burn");
            configured = instant && burn;
            break;
        }

        case 0x8A0: {  // Radiant Triplecut: native 15/20/25 weighting, total 60.
            const float blade1 = scaledBudget * (15.0f / 60.0f);
            const float blade2 = scaledBudget * (20.0f / 60.0f);
            const float blade3 = scaledBudget * (25.0f / 60.0f);

            const bool one = SetSpellEffectMagnitude(
                g_radiantTriplecutSpell1, 0, blade1, "Radiant Triplecut / blade 1 (D72)");
            const bool two = SetSpellEffectMagnitude(
                g_radiantTriplecutSpell2, 0, blade2, "Radiant Triplecut / blade 2 (D71)");
            const bool three = SetSpellEffectMagnitude(
                g_radiantTriplecutSpell3, 0, blade3, "Radiant Triplecut / blade 3 (D72)");
            configured = one && two && three;
            break;
        }

        case 0x829:  // Moonlit Cleave: one unique Moonlight Blast projectile.
            configured = SetSpellEffectMagnitude(
                g_moonlitCleaveDamageSpell,
                0,
                scaledBudget,
                "Moonlit Cleave / Moonlight Blast");
            break;

        case 0x87B: {  // Moonlit Severance: two unique magical slices share one Expert budget.
            const float perSlice = scaledBudget / 2.0f;
            const bool one = SetSpellEffectMagnitude(
                g_moonlitSeveranceSpell1, 0, perSlice, "Moonlit Severance / slice 1");
            const bool two = SetSpellEffectMagnitude(
                g_moonlitSeveranceSpell2, 0, perSlice, "Moonlit Severance / slice 2");
            configured = one && two;
            break;
        }

        case 0x87D: {  // Moonshard Sigil: trigger beam + nine spawned damage beams.
            constexpr float kMoonshardExtraBeams = 9.0f;

            // The first beam is only the hit-confirm/sector trigger.
            const bool trigger = SetSpellEffectMagnitude(
                g_moonshardSigilDamageSpell,
                0,
                0.0f,
                "Moonshard Sigil / trigger beam");

            // The spawned secondary beams own the entire Expert magic budget.
            const float perBeam = scaledBudget / kMoonshardExtraBeams;
            const bool extras = SetSpellEffectMagnitude(
                g_moonshardSigilExtraSpell,
                0,
                perBeam,
                "Moonshard Sigil / spawned beam");

            configured = trigger && extras;
            break;
        }

        case 0x893:  // Elder Mooncleave: one Master magic beam.
            configured = SetSpellEffectMagnitude(
                g_elderMooncleaveDamageSpell,
                0,
                scaledBudget,
                "Elder Mooncleave / Moonlight Beam");
            break;

        case 0x891: {  // Crimson Severance: three magic slashes share one Expert budget.
            const float perSlice = scaledBudget / 3.0f;
            const bool one = SetSpellEffectMagnitude(
                g_crimsonSeveranceSpell1, 0, perSlice, "Crimson Severance / slice 1");
            const bool three = SetSpellEffectMagnitude(
                g_crimsonSeveranceSpell3, 0, perSlice, "Crimson Severance / slice 3");
            const bool four = SetSpellEffectMagnitude(
                g_crimsonSeveranceSpell4, 0, perSlice, "Crimson Severance / slice 4");
            configured = one && three && four;
            break;
        }

        case 0x832: {
            ConfigureRadiantFinisherDamageScale();
            DumpRadiantCarianImpactPayload();

            // Actual animation event counts:
            // 30 x1, 60 x3, 80 x3, 120 x3, 150 x1, final x1.
            // Native weighting is therefore (11 * 45) + 120 = 615.
            constexpr float kNativeWeightedTotal = 615.0f;
            const float normalBlade = scaledBudget * (45.0f / kNativeWeightedTotal);
            const float finalBlade = scaledBudget * (120.0f / kNativeWeightedTotal);

            const bool a = SetSpellEffectMagnitude(
                g_radiantBladeDanceSpell30, 0, normalBlade, "Radiant Blade Dance / blade 30");
            const bool b = SetSpellEffectMagnitude(
                g_radiantBladeDanceSpell60, 0, normalBlade, "Radiant Blade Dance / blade 60");
            const bool c = SetSpellEffectMagnitude(
                g_radiantBladeDanceSpell80, 0, normalBlade, "Radiant Blade Dance / blade 80");
            const bool d = SetSpellEffectMagnitude(
                g_radiantBladeDanceSpell120, 0, normalBlade, "Radiant Blade Dance / blade 120");
            const bool e = SetSpellEffectMagnitude(
                g_radiantBladeDanceSpell150, 0, normalBlade, "Radiant Blade Dance / blade 150");
            const bool f = SetSpellEffectMagnitude(
                g_radiantBladeDanceFinal, 0, finalBlade, "Radiant Blade Dance / final blade");
            configured = a && b && c && d && e && f;
            break;
        }

        default:
            return;
        }

        SKSE::log::info(
            "[MAGIC DAMAGE CONFIG] Technique={} stone={:03X} rank={} baseMagicBudget={:.2f} alteration={:.2f} alterationMult={:.4f} scaledMagicBudget={:.2f} configured={}",
            a_def.name,
            a_def.localFormID,
            GetMagicRankName(a_def.tier),
            baseBudget,
            alterationSkill,
            alterationMult,
            scaledBudget,
            configured);
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

    void ChargeMagicTechniqueCost(
        RE::Actor* a_actor,
        const MagicTechniqueDefinition& a_def,
        RE::FormID a_signalFormID)
    {
        if (!a_actor) {
            return;
        }

        float naturalProbeCost = -1.0f;
        const float adjustedCost = CalculateAlterationAdjustedTechniqueCost(
            a_actor,
            a_def,
            naturalProbeCost);

        auto* avOwner = a_actor->AsActorValueOwner();
        if (!avOwner) {
            SKSE::log::error("[MAGIC CHARGE] ActorValueOwner unavailable for {}", a_def.name);
            return;
        }

        const float before = std::max(0.0f, avOwner->GetActorValue(RE::ActorValue::kMagicka));
        const bool sufficient = before + 0.01f >= adjustedCost;
        const float charged = std::clamp(adjustedCost, 0.0f, before);

        if (charged > 0.0f) {
            avOwner->RestoreActorValue(
                RE::ACTOR_VALUE_MODIFIER::kDamage,
                RE::ActorValue::kMagicka,
                -charged);
        }

        const float after = std::max(0.0f, avOwner->GetActorValue(RE::ActorValue::kMagicka));

        SKSE::log::info(
            "[MAGIC CHARGE] Technique={} stone={:03X} tier={} baseMagicka={:.1f} "
            "probe={} naturalProbeCost={:.2f} adjustedCost={:.2f} "
            "magickaBefore={:.2f} charged={:.2f} magickaAfter={:.2f} sufficient={} "
            "signal={:08X}",
            a_def.name,
            a_def.localFormID,
            a_def.tier,
            a_def.baseMagicka,
            GetAlterationProbeName(a_def.tier),
            naturalProbeCost,
            adjustedCost,
            before,
            charged,
            after,
            sufficient,
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

    bool IsRepresentativeMagicTraceStone(RE::FormID a_localFormID)
    {
        switch (a_localFormID) {
        case 0x87A:  // Gale Crescent
        case 0x8A0:  // Radiant Triplecut
        case 0x87B:  // Moonlit Severance
        case 0x847:  // Dragonfire Sigil
        case 0x88F:  // Aurochs Charge
        case 0x89A:  // Ember Infusion
        case 0x87D:  // Moonshard Sigil outlier trace
        case 0x832:  // Radiant Blade Dance end-burst trace
        case 0x86D:  // Tempest Crescent mapping trace
            return true;
        default:
            return false;
        }
    }

    RE::FormID GetLocalFormID(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return 0;
        }

        const auto* file = a_form->GetFile();
        if (!file) {
            return a_form->GetFormID();
        }

        return file->IsLight() ? (a_form->GetFormID() & 0x00000FFF) : (a_form->GetFormID() & 0x00FFFFFF);
    }

    std::string_view GetSourcePlugin(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return "<none>";
        }

        const auto* file = a_form->GetFile();
        return file ? file->GetFilename() : std::string_view("<dynamic>");
    }

    bool IsInsideMagicTraceWindow(const MagicTechniqueDefinition* a_def)
    {
        if (!a_def || g_lastMagicActivationStone != a_def->localFormID) {
            return false;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - g_lastMagicActivationTime).count();
        return elapsed >= 0 && elapsed <= 5000;
    }

    class MagicCandidateEffectSink final : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESMagicEffectApplyEvent* a_event,
            RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
        {
            if (!a_event || !a_event->caster || !a_event->target || a_event->magicEffect == 0) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* caster = a_event->caster.get();
            auto* target = a_event->target.get();
            if (!caster || !caster->IsPlayerRef() || !target) {
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

            const RE::FormID primaryID = g_magicCandidatePrimary ? g_magicCandidatePrimary->GetFormID() : 0;
            const RE::FormID secondaryID = g_magicCandidateSecondary ? g_magicCandidateSecondary->GetFormID() : 0;

            if (target->IsPlayerRef()) {
                if (a_event->magicEffect != primaryID && a_event->magicEffect != secondaryID) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                const bool primary = a_event->magicEffect == primaryID;
                SKSE::log::info(
                    "[MAGIC CANDIDATE] Technique={} stone={:03X} effect={:08X} candidate={} target=player",
                    def->name,
                    def->localFormID,
                    a_event->magicEffect,
                    primary ? "0x800-primary" : "0x805-secondary");

                if (primary) {
                    g_lastMagicActivationTime = std::chrono::steady_clock::now();
                    g_lastMagicActivationStone = def->localFormID;

                    ConfigureRepresentativeMagicDamage(player, *def);

                    ChargeMagicTechniqueCost(
                        player,
                        *def,
                        a_event->magicEffect);
                }

                return RE::BSEventNotifyControl::kContinue;
            }

            if (IsRepresentativeMagicTraceStone(def->localFormID)) {
                auto* mgef = RE::TESForm::LookupByID<RE::EffectSetting>(a_event->magicEffect);

                if (mgef) {
                    const char* editorID = mgef->GetFormEditorID();
                    const char* effectName = mgef->GetName();
                    auto* projectileExplosion =
                        (mgef->data.projectileBase && mgef->data.projectileBase->data.explosionType) ?
                            mgef->data.projectileBase->data.explosionType : nullptr;
                    auto* directExplosion = mgef->data.explosion;
                    const float projectileExplosionDamage =
                        projectileExplosion ? projectileExplosion->data.damage : -1.0f;
                    const float directExplosionDamage =
                        directExplosion ? directExplosion->data.damage : -1.0f;

                    SKSE::log::info(
                        "[MAGIC MGEF TRACE] Technique={} stone={:03X} target={:08X} "
                        "effectPlugin={} effectLocal={:06X} effectRuntime={:08X} "
                        "editor={} name={} archetype={} primaryAV={} resistAV={} "
                        "detrimental={} hostile={} baseCost={:.3f} area={} projectile={:08X} "
                        "projectileExplosion={:08X} projectileExplosionDamage={:.3f} "
                        "explosion={:08X} explosionDamage={:.3f}",
                        def->name,
                        def->localFormID,
                        target->GetFormID(),
                        GetSourcePlugin(mgef),
                        GetLocalFormID(mgef),
                        mgef->GetFormID(),
                        editorID ? editorID : "",
                        effectName ? effectName : "",
                        static_cast<std::uint32_t>(mgef->data.archetype),
                        static_cast<std::uint32_t>(mgef->data.primaryAV),
                        static_cast<std::uint32_t>(mgef->data.resistVariable),
                        mgef->IsDetrimental(),
                        mgef->IsHostile(),
                        mgef->data.baseCost,
                        mgef->data.spellmakingArea,
                        mgef->data.projectileBase ? mgef->data.projectileBase->GetFormID() : 0,
                        projectileExplosion ? projectileExplosion->GetFormID() : 0,
                        projectileExplosionDamage,
                        directExplosion ? directExplosion->GetFormID() : 0,
                        directExplosionDamage);
                } else {
                    SKSE::log::info(
                        "[MAGIC MGEF TRACE] Technique={} stone={:03X} effect={:08X} target={:08X} effectLookup=NOT_FOUND",
                        def->name,
                        def->localFormID,
                        a_event->magicEffect,
                        target->GetFormID());
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    RE::ActiveEffect* FindActiveEffectByUniqueID(RE::Actor* a_actor, std::uint16_t a_uniqueID)
    {
        if (!a_actor) {
            return nullptr;
        }

        auto* magicTarget = a_actor->AsMagicTarget();
        auto* effects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!effects) {
            return nullptr;
        }

        for (auto* effect : *effects) {
            if (!effect) {
                continue;
            }
            if (effect->usUniqueID == a_uniqueID) {
                return effect;
            }
        }
        return nullptr;
    }

    class MagicActiveEffectSink final : public RE::BSTEventSink<RE::TESActiveEffectApplyRemoveEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESActiveEffectApplyRemoveEvent* a_event,
            RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>*) override
        {
            if (!a_event || !a_event->isApplied || !a_event->target) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* targetRef = a_event->target.get();
            if (!targetRef) {
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

            if (targetRef->IsPlayerRef()) {
                auto* activeEffect = FindActiveEffectByUniqueID(player, a_event->activeEffectUniqueID);
                if (!activeEffect || !g_techniqueMarker) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto* baseEffect = activeEffect->GetBaseObject();
                if (baseEffect != g_techniqueMarker) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                SKSE::log::info(
                    "[MAGIC ACTIVATION] Technique={} stone={:03X} marker={:08X} uniqueID={}",
                    def->name,
                    def->localFormID,
                    baseEffect->GetFormID(),
                    a_event->activeEffectUniqueID);

                return RE::BSEventNotifyControl::kContinue;
            }

            if (!IsRepresentativeMagicTraceStone(def->localFormID) || !IsInsideMagicTraceWindow(def)) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* targetActor = targetRef->As<RE::Actor>();
            if (!targetActor) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* activeEffect = FindActiveEffectByUniqueID(targetActor, a_event->activeEffectUniqueID);
            if (!activeEffect) {
                SKSE::log::info(
                    "[MAGIC ACTIVE TRACE] Technique={} target={:08X} uniqueID={} activeEffect=NOT_FOUND",
                    def->name,
                    targetActor->GetFormID(),
                    a_event->activeEffectUniqueID);
                return RE::BSEventNotifyControl::kContinue;
            }

            auto caster = activeEffect->GetCasterActor();
            if (!caster || !caster->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* baseEffect = activeEffect->GetBaseObject();
            if (!baseEffect) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* spell = activeEffect->spell;
            const char* effectEditorID = baseEffect->GetFormEditorID();
            const char* spellEditorID = spell ? spell->GetFormEditorID() : "";
            const char* effectName = baseEffect->GetName();
            const char* spellName = spell ? spell->GetName() : "";

            SKSE::log::info(
                "[MAGIC ACTIVE TRACE] Technique={} stone={:03X} target={:08X} "
                "effectPlugin={} effectLocal={:06X} effectRuntime={:08X} effectEditor={} effectName={} "
                "magnitude={:.3f} duration={:.3f} archetype={} primaryAV={} resistAV={} baseCost={:.3f} "
                "spellPlugin={} spellLocal={:06X} spellRuntime={:08X} spellEditor={} spellName={}",
                def->name,
                def->localFormID,
                targetActor->GetFormID(),
                GetSourcePlugin(baseEffect),
                GetLocalFormID(baseEffect),
                baseEffect->GetFormID(),
                effectEditorID ? effectEditorID : "",
                effectName ? effectName : "",
                activeEffect->GetMagnitude(),
                activeEffect->duration,
                static_cast<std::uint32_t>(baseEffect->data.archetype),
                static_cast<std::uint32_t>(baseEffect->data.primaryAV),
                static_cast<std::uint32_t>(baseEffect->data.resistVariable),
                baseEffect->data.baseCost,
                GetSourcePlugin(spell),
                GetLocalFormID(spell),
                spell ? spell->GetFormID() : 0,
                spellEditorID ? spellEditorID : "",
                spellName ? spellName : "");

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
            static MagicCandidateEffectSink magicEffectSink;
            source->AddEventSink<RE::TESMagicEffectApplyEvent>(&magicEffectSink);
            g_magicEffectSinkRegistered = true;
            SKSE::log::info("Magic Technique candidate-effect diagnostic sink registered");
        }

        if (!g_activeEffectSinkRegistered) {
            static MagicActiveEffectSink activeEffectSink;
            source->AddEventSink<RE::TESActiveEffectApplyRemoveEvent>(&activeEffectSink);
            g_activeEffectSinkRegistered = true;
            SKSE::log::info("Magic Technique active-effect diagnostic sink registered");
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

        g_galeCrescentDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kGaleCrescentDamageSpellLocalID, kRimSkillsPlugin);
        g_dragonfireSigilDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kDragonfireSigilDamageSpellLocalID, kRimSkillsPlugin);
        g_radiantTriplecutSpell1 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantTriplecutSpell1LocalID, kRimSkillsPlugin);
        g_radiantTriplecutSpell2 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantTriplecutSpell2LocalID, kRimSkillsPlugin);
        g_radiantTriplecutSpell3 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantTriplecutSpell3LocalID, kRimSkillsPlugin);

        g_moonlitCleaveDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kMoonlitCleaveDamageSpellLocalID, kRimSkillsPlugin);
        g_moonlitSeveranceSpell1 = dataHandler->LookupForm<RE::SpellItem>(
            kMoonlitSeveranceSpell1LocalID, kRimSkillsPlugin);
        g_moonlitSeveranceSpell2 = dataHandler->LookupForm<RE::SpellItem>(
            kMoonlitSeveranceSpell2LocalID, kRimSkillsPlugin);
        g_moonshardSigilDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kMoonshardSigilDamageSpellLocalID, kRimSkillsPlugin);
        g_moonshardSigilExtraSpell = dataHandler->LookupForm<RE::SpellItem>(
            kMoonshardSigilExtraSpellLocalID, kRimSkillsPlugin);

        g_elderMooncleaveDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kElderMooncleaveDamageSpellLocalID, kEldenSkyrimPlugin);
        g_crimsonSeveranceSpell1 = dataHandler->LookupForm<RE::SpellItem>(
            kCrimsonSeveranceSpell1LocalID, kEldenSkyrimPlugin);
        g_crimsonSeveranceSpell3 = dataHandler->LookupForm<RE::SpellItem>(
            kCrimsonSeveranceSpell3LocalID, kEldenSkyrimPlugin);
        g_crimsonSeveranceSpell4 = dataHandler->LookupForm<RE::SpellItem>(
            kCrimsonSeveranceSpell4LocalID, kEldenSkyrimPlugin);
        g_radiantBladeDanceSpell30 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantBladeDanceSpell30LocalID, kRimSkillsPlugin);
        g_radiantBladeDanceSpell60 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantBladeDanceSpell60LocalID, kRimSkillsPlugin);
        g_radiantBladeDanceSpell80 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantBladeDanceSpell80LocalID, kRimSkillsPlugin);
        g_radiantBladeDanceSpell120 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantBladeDanceSpell120LocalID, kRimSkillsPlugin);
        g_radiantBladeDanceSpell150 = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantBladeDanceSpell150LocalID, kRimSkillsPlugin);
        g_radiantBladeDanceFinal = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantBladeDanceFinalLocalID, kRimSkillsPlugin);
        g_radiantCarianImpactSpell = dataHandler->LookupForm<RE::SpellItem>(
            kRadiantCarianImpactSpellLocalID, kRimSkillsPlugin);

        g_techniqueMarker = dataHandler->LookupForm<RE::EffectSetting>(kTechniqueMarkerLocalID, kCooldownPlugin);
        g_magicCandidatePrimary = dataHandler->LookupForm<RE::EffectSetting>(kMagicCandidatePrimaryLocalID, kCooldownPlugin);
        g_magicCandidateSecondary = dataHandler->LookupForm<RE::EffectSetting>(kMagicCandidateSecondaryLocalID, kCooldownPlugin);

        SKSE::log::info("Resolved {}/{} physical Technique Stones", resolved, kTechniqueDamageDefinitions.size());
        SKSE::log::info("Resolved {}/{} Magic/Rune Technique Stones", magicResolved, kMagicTechniqueDefinitions.size());
        SKSE::log::info("Resolved {}/{} Magic activation signals", activationResolved, kMagicActivationSignals.size());
        SKSE::log::info(
            "Alteration probes: T1={} T2={} T3={}",
            g_tier1AlterationProbe ? "Oakflesh" : "MISSING",
            g_tier2AlterationProbe ? "Stoneflesh" : "MISSING",
            g_tier3AlterationProbe ? "Ironflesh" : "MISSING");

        SKSE::log::info(
            "Magic damage proof spells: Gale={} Dragonfire={} Triplecut={}/{}/{}",
            g_galeCrescentDamageSpell ? "OK" : "MISSING",
            g_dragonfireSigilDamageSpell ? "OK" : "MISSING",
            g_radiantTriplecutSpell1 ? "OK" : "MISSING",
            g_radiantTriplecutSpell2 ? "OK" : "MISSING",
            g_radiantTriplecutSpell3 ? "OK" : "MISSING");

        SKSE::log::info(
            "Direct Magic batch spells: MoonlitCleave={} MoonlitSeverance={}/{} Moonshard={}/{}",
            g_moonlitCleaveDamageSpell ? "OK" : "MISSING",
            g_moonlitSeveranceSpell1 ? "OK" : "MISSING",
            g_moonlitSeveranceSpell2 ? "OK" : "MISSING",
            g_moonshardSigilDamageSpell ? "OK" : "MISSING",
            g_moonshardSigilExtraSpell ? "OK" : "MISSING");

        SKSE::log::info(
            "Direct Magic batch 2: Elder={} Crimson={}/{}/{} RadiantDance={}/{}/{}/{}/{}/{}",
            g_elderMooncleaveDamageSpell ? "OK" : "MISSING",
            g_crimsonSeveranceSpell1 ? "OK" : "MISSING",
            g_crimsonSeveranceSpell3 ? "OK" : "MISSING",
            g_crimsonSeveranceSpell4 ? "OK" : "MISSING",
            g_radiantBladeDanceSpell30 ? "OK" : "MISSING",
            g_radiantBladeDanceSpell60 ? "OK" : "MISSING",
            g_radiantBladeDanceSpell80 ? "OK" : "MISSING",
            g_radiantBladeDanceSpell120 ? "OK" : "MISSING",
            g_radiantBladeDanceSpell150 ? "OK" : "MISSING",
            g_radiantBladeDanceFinal ? "OK" : "MISSING");
        SKSE::log::info(
            "Radiant finisher spell: CarianImpact={}",
            g_radiantCarianImpactSpell ? "OK" : "MISSING");
        ResolveRadiantFinisherDamagePayloads(dataHandler);

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            for (const auto& def : kMagicTechniqueDefinitions) {
                ConfigureRepresentativeMagicDamage(player, def);
            }
            SKSE::log::info("Pre-normalized mapped Magic payload spells using current Alteration");
        }

        if (g_techniqueMarker) {
            SKSE::log::info("Technique marker resolved runtimeForm={:08X}", g_techniqueMarker->GetFormID());
        } else {
            SKSE::log::info("Technique marker NOT resolved");
        }
        SKSE::log::info(
            "Magic candidate effects: primary={} secondary={}",
            g_magicCandidatePrimary ? fmt::format("{:08X}", g_magicCandidatePrimary->GetFormID()) : "MISSING",
            g_magicCandidateSecondary ? fmt::format("{:08X}", g_magicCandidateSecondary->GetFormID()) : "MISSING");
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

    SKSE::log::info("HE Technique Damage v0.4.3 Tempest Crescent mapping diagnostic loaded");
    return true;
}
