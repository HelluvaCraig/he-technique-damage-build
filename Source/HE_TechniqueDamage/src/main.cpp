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
    constexpr RE::FormID kCoveringFireActivationSpellLocalID = 0x0BA9B;
    constexpr RE::FormID kTechniqueMarkerLocalID = 0x920;
    constexpr RE::FormID kMagicCandidatePrimaryLocalID = 0x800;
    constexpr RE::FormID kMagicCandidateSecondaryLocalID = 0x805;
    constexpr bool kBatchAuditDisableCooldowns = false;
    constexpr bool kPhysicalTraceEnabled = false;

    // Shared Technique Charge system.
    // OAR reads dedicated TESGlobal records instead of generic ActorValues so
    // this system cannot collide with other mods that repurpose Variable01-10.
    constexpr RE::FormID kTechniqueChargesGlobalLocalID = 0xE23;
    constexpr RE::FormID kTechniqueMaxChargesGlobalLocalID = 0xE24;
    constexpr RE::FormID kTechniqueRechargeProgressGlobalLocalID = 0xE25;
    constexpr RE::FormID kTechniqueRecoveryGlobalLocalID = 0xE26;
    constexpr RE::FormID kTechniqueRecoveryEffectLocalID = 0xE20;

    // The existing rank signal spells apply these rank-specific effects.
    // Payload Interpreter @CAST does not reliably emit TESSpellCastEvent, so
    // charge spending is driven by the applied MGEF instead.
    constexpr RE::FormID kAdeptChargeEffectLocalID = 0x804;
    constexpr RE::FormID kExpertChargeEffectLocalID = 0x805;
    constexpr RE::FormID kMasterChargeEffectLocalID = 0x806;

    constexpr float kBaseTechniqueRechargeSeconds = 30.0f;
    constexpr float kMaxTechniqueRecoveryPercent = 90.0f;

    constexpr std::uint32_t FourCC(char a, char b, char c, char d)
    {
        return static_cast<std::uint32_t>(a) |
               (static_cast<std::uint32_t>(b) << 8) |
               (static_cast<std::uint32_t>(c) << 16) |
               (static_cast<std::uint32_t>(d) << 24);
    }

    constexpr std::uint32_t kSerializationUniqueID = FourCC('H', 'E', 'T', 'C');
    constexpr std::uint32_t kChargeStateRecord = FourCC('C', 'H', 'R', 'G');
    constexpr std::uint32_t kChargeStateVersion = 1;

    constexpr auto kSkyrimPlugin = "Skyrim.esm";
    constexpr RE::FormID kOakfleshLocalID = 0x5AD5C;
    constexpr RE::FormID kStonefleshLocalID = 0x5AD5D;
    constexpr RE::FormID kIronfleshLocalID = 0x51B16;

    // v0.3.0 representative Magic damage proof payload spells.
    constexpr RE::FormID kGaleCrescentDamageSpellLocalID = 0x0B43D;
    constexpr RE::FormID kDragonfireSigilDamageSpellLocalID = 0x000F27;
    constexpr RE::FormID kRunicFirebrandDamageSpellLocalID = 0x000DE2;
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

    // Tempest Crescent: seven light-line projectiles share one Tier 2 damage budget.
    constexpr RE::FormID kTempestCrescentDamageEffectLocalID = 0x000E5B;
    constexpr float kTempestCrescentLines = 7.0f;

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
    RE::SpellItem* g_runicFirebrandDamageSpell = nullptr;
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
    RE::SpellItem* g_coveringFireActivationSpell = nullptr;

    RE::TESGlobal* g_techniqueChargesGlobal = nullptr;
    RE::TESGlobal* g_techniqueMaxChargesGlobal = nullptr;
    RE::TESGlobal* g_techniqueRechargeProgressGlobal = nullptr;
    RE::TESGlobal* g_techniqueRecoveryGlobal = nullptr;
    RE::EffectSetting* g_techniqueRecoveryEffect = nullptr;
    RE::EffectSetting* g_adeptChargeEffect = nullptr;
    RE::EffectSetting* g_expertChargeEffect = nullptr;
    RE::EffectSetting* g_masterChargeEffect = nullptr;

    struct TechniqueChargeState
    {
        std::uint32_t currentCharges{ 0 };
        std::uint32_t previousMaxCharges{ 0 };
        float rechargeProgressSeconds{ 0.0f };
        bool initialized{ false };
    };

    struct SerializedTechniqueChargeState
    {
        std::uint32_t currentCharges{ 0 };
        std::uint32_t previousMaxCharges{ 0 };
        float rechargeProgressSeconds{ 0.0f };
        std::uint32_t initialized{ 0 };
    };

    TechniqueChargeState g_chargeState{};
    bool g_chargeClockStarted = false;
    bool g_chargeClockPaused = false;
    bool g_chargeInputSinkRegistered = false;
    bool g_chargeMenuSinkRegistered = false;
    std::chrono::steady_clock::time_point g_chargeLastUpdate{};
    std::chrono::steady_clock::time_point g_lastChargeSignalTime{};
    RE::FormID g_lastChargeSignalSpell = 0;

    // Physical Technique Stones share AABL's attack channel, which normally
    // spends Stamina. Stone Techniques are charge-only, so snapshot Stamina on
    // the user's V press and refund only after a rank charge signal confirms
    // that a physical Stone Technique actually activated. Built-in Additional
    // Attacks never emit that rank signal and therefore keep their Stamina cost.
    constexpr std::uint32_t kTechniqueInputKeyCode = 0x2F;  // keyboard V scan code
    constexpr float kMaxPhysicalTechniqueStaminaRefund = 250.0f;

    // Built-in Bow Additional Attack (Covering Fire) uses the separate DKAF
    // ranged bridge and therefore bypasses AABL's normal stamina-cost spell.
    // Charge it here at the same base cost as the current normal 1H sword
    // power-attack class in the built-in balance table.
    constexpr float kCoveringFireBaseStaminaCost = 55.0f;
    bool g_physicalStaminaRefundArmed = false;
    float g_physicalStaminaSnapshot = 0.0f;
    std::chrono::steady_clock::time_point g_physicalStaminaSnapshotTime{};

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

    struct TempestDamageEffectRef
    {
        RE::Effect* effect{ nullptr };
        float nativeMagnitude{ 0.0f };
        RE::FormID spellFormID{ 0 };
        RE::FormID effectFormID{ 0 };
    };

    std::vector<TempestDamageEffectRef> g_tempestCrescentDamageEffects;

    struct BatchMagicPayloadKey
    {
        const char* effectPlugin;
        RE::FormID effectLocalID;
    };

    struct BatchMagicPayloadRef
    {
        RE::Effect* effect{ nullptr };
        float nativeMagnitude{ 0.0f };
        RE::FormID spellFormID{ 0 };
        RE::FormID effectFormID{ 0 };
    };

    struct BatchMagicPayloadResolved
    {
        BatchMagicPayloadKey key{};
        std::vector<BatchMagicPayloadRef> refs;
        float representativeAbsMagnitude{ 0.0f };
    };

    struct BatchMagicPayloadUse
    {
        const char* effectPlugin;
        RE::FormID effectLocalID;
        float expectedPerTarget{ 1.0f };
    };

    inline const std::array<BatchMagicPayloadKey, 20> kBatchMagicPayloadKeys = {{
        { kEldenSkyrimPlugin, 0x000B1D },  // Frostbreak - Frost Stomp
        { kRimSkillsPlugin,    0x00B34C }, // Honed Bolt - location strike
        { kRimSkillsPlugin,    0x00B34E }, // Honed Bolt - bolt
        { kRimSkillsPlugin,    0x000ACB }, // Avalanche - impact blast
        { kRimSkillsPlugin,    0x00B3C6 }, // Gale/Psionic - Vacuum impact
        { kRimSkillsPlugin,    0x000E3F }, // Gale/Psionic - travelling damage
        { kRimSkillsPlugin,    0x000D9E }, // Titan Spear
        { kRimSkillsPlugin,    0x00B5E2 }, // Lionfire / Ember / Inferno flame wave
        { kRimSkillsPlugin,    0x000E88 }, // Runic Disruption
        { kRimSkillsPlugin,    0x000C2E }, // Fire elemental impact
        { kRimSkillsPlugin,    0x000F5F }, // Fire Storm 100
        { kRimSkillsPlugin,    0x000F60 }, // Fire Storm 65
        { kRimSkillsPlugin,    0x000F61 }, // Fire Storm 25
        { kRimSkillsPlugin,    0x00B47B }, // Stormwyrm shock damage
        { kRimSkillsPlugin,    0x00B3EA }, // Stormwyrm bolt explosion
        { kRimSkillsPlugin,    0x00B414 }, // Wyrmforce skewer explosion
        { kRimSkillsPlugin,    0x00B777 }, // Judgment Arc
        { kRimSkillsPlugin,    0x00B46C }, // Runic Might drain
        { kRimSkillsPlugin,    0x00B730 }, // Flamewake elemental shock
        { kRimSkillsPlugin,    0x00B753 }, // Frostwake elemental shock
    }};

    std::vector<BatchMagicPayloadResolved> g_batchMagicPayloads;

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

    void ArmPhysicalTechniqueStaminaRefund(RE::Actor* a_actor)
    {
        if (!a_actor || !GetEquippedPhysicalStone(a_actor)) {
            g_physicalStaminaRefundArmed = false;
            return;
        }

        const float stamina = a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        if (!std::isfinite(stamina)) {
            g_physicalStaminaRefundArmed = false;
            return;
        }

        g_physicalStaminaSnapshot = stamina;
        g_physicalStaminaSnapshotTime = std::chrono::steady_clock::now();
        g_physicalStaminaRefundArmed = true;
        SKSE::log::info("[STONE STAMINA ARM] snapshot={:.2f}", stamina);
    }

    void QueuePhysicalTechniqueStaminaRefund(RE::Actor* a_actor)
    {
        if (!a_actor || !g_physicalStaminaRefundArmed || !GetEquippedPhysicalStone(a_actor)) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - g_physicalStaminaSnapshotTime).count();
        if (ageMs < 0 || ageMs > 2000) {
            g_physicalStaminaRefundArmed = false;
            SKSE::log::info("[STONE STAMINA REFUND] expired snapshot age={}ms", ageMs);
            return;
        }

        const float snapshot = g_physicalStaminaSnapshot;
        g_physicalStaminaRefundArmed = false;

        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([snapshot]() {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player) {
                    return;
                }

                const float current = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
                if (!std::isfinite(current) || !std::isfinite(snapshot)) {
                    return;
                }

                const float refund = snapshot - current;
                if (refund > 0.01f && refund <= kMaxPhysicalTechniqueStaminaRefund) {
                    player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, refund);
                    SKSE::log::info(
                        "[STONE STAMINA REFUND] before={:.2f} snapshot={:.2f} refunded={:.2f}",
                        current,
                        snapshot,
                        refund);
                } else {
                    SKSE::log::info(
                        "[STONE STAMINA REFUND] no refund current={:.2f} snapshot={:.2f} delta={:.2f}",
                        current,
                        snapshot,
                        refund);
                }
            });
        }
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

    bool HasAnyTechniqueStone(RE::Actor* a_actor)
    {
        return GetEquippedPhysicalStone(a_actor) != nullptr ||
               GetEquippedMagicStone(a_actor) != nullptr;
    }

    bool IsBowEquippedRight(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        auto* equipped = a_actor->GetEquippedObject(false);
        auto* weapon = equipped ? equipped->As<RE::TESObjectWEAP>() : nullptr;
        return weapon && weapon->IsBow();
    }

    void SpendCoveringFireStamina(RE::Actor* a_actor)
    {
        if (!a_actor || HasAnyTechniqueStone(a_actor) || !IsBowEquippedRight(a_actor)) {
            return;
        }

        auto* avOwner = a_actor->AsActorValueOwner();
        if (!avOwner) {
            return;
        }

        const float before = avOwner->GetActorValue(RE::ActorValue::kStamina);
        if (!std::isfinite(before) || before <= 0.0f) {
            return;
        }

        // Match normal power-attack behaviour at low Stamina by spending the
        // remaining amount rather than forcing the ActorValue below zero.
        const float spent = std::min(before, kCoveringFireBaseStaminaCost);
        avOwner->RestoreActorValue(
            RE::ACTOR_VALUE_MODIFIER::kDamage,
            RE::ActorValue::kStamina,
            -spent);

        const float after = avOwner->GetActorValue(RE::ActorValue::kStamina);
        SKSE::log::info(
            "[COVERING FIRE STAMINA] before={:.2f} spent={:.2f} after={:.2f}",
            before,
            spent,
            after);
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
        // Physical contact-count validation is complete. Keep production logs
        // quiet unless a targeted diagnostic build explicitly re-enables this.
        return kPhysicalTraceEnabled && a_def != nullptr;
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

    float GetMaxMagickaTechniqueMultiplier(RE::Actor* a_actor, float& a_maxMagicka)
    {
        a_maxMagicka = 100.0f;
        if (a_actor) {
            if (auto* avOwner = a_actor->AsActorValueOwner()) {
                a_maxMagicka = std::max(0.0f, avOwner->GetPermanentActorValue(RE::ActorValue::kMagicka));
            }
        }

        // Keep the already-balanced magic curve almost unchanged:
        // +1% Technique damage per 100 maximum Magicka above the 100 base.
        const float aboveBase = std::max(0.0f, a_maxMagicka - 100.0f);
        return 1.0f + ((aboveBase / 100.0f) * 0.01f);
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

    BatchMagicPayloadResolved* FindBatchMagicPayload(const char* a_effectPlugin, RE::FormID a_effectLocalID)
    {
        for (auto& payload : g_batchMagicPayloads) {
            if (payload.key.effectLocalID == a_effectLocalID &&
                std::string_view(payload.key.effectPlugin) == std::string_view(a_effectPlugin)) {
                return std::addressof(payload);
            }
        }
        return nullptr;
    }

    void ResolveBatchMagicPayloads(RE::TESDataHandler* a_dataHandler)
    {
        g_batchMagicPayloads.clear();
        if (!a_dataHandler) {
            return;
        }

        for (const auto& key : kBatchMagicPayloadKeys) {
            BatchMagicPayloadResolved resolved{};
            resolved.key = key;

            for (auto* spell : a_dataHandler->GetFormArray<RE::SpellItem>()) {
                if (!spell) {
                    continue;
                }

                const auto* spellFile = spell->GetFile();
                if (!spellFile) {
                    continue;
                }

                const std::string_view spellPlugin = spellFile->GetFilename();
                if (spellPlugin != kRimSkillsPlugin && spellPlugin != kEldenSkyrimPlugin) {
                    continue;
                }

                for (auto* effect : spell->effects) {
                    if (!effect || !effect->baseEffect) {
                        continue;
                    }

                    auto* baseEffect = effect->baseEffect;
                    const auto* effectFile = baseEffect->GetFile();
                    if (!effectFile || std::string_view(effectFile->GetFilename()) != key.effectPlugin) {
                        continue;
                    }

                    const RE::FormID localID = effectFile->IsLight() ?
                        (baseEffect->GetFormID() & 0x00000FFF) :
                        (baseEffect->GetFormID() & 0x00FFFFFF);
                    if (localID != key.effectLocalID) {
                        continue;
                    }

                    resolved.refs.push_back({
                        effect,
                        effect->effectItem.magnitude,
                        spell->GetFormID(),
                        baseEffect->GetFormID()
                    });
                    resolved.representativeAbsMagnitude = std::max(
                        resolved.representativeAbsMagnitude,
                        std::fabs(effect->effectItem.magnitude));
                }
            }

            SKSE::log::info(
                "[BATCH MAGIC RESOLVE] effectPlugin={} effectLocal={:06X} refs={} representativeAbsMagnitude={:.3f}",
                key.effectPlugin,
                key.effectLocalID,
                resolved.refs.size(),
                resolved.representativeAbsMagnitude);

            g_batchMagicPayloads.push_back(std::move(resolved));
        }
    }

    bool ConfigureBatchMagicPayloadGroup(
        float a_scaledBudget,
        std::initializer_list<BatchMagicPayloadUse> a_uses,
        const char* a_label,
        float a_budgetShare = 1.0f)
    {
        float nativeWeightedTotal = 0.0f;
        bool allResolved = true;

        for (const auto& use : a_uses) {
            auto* payload = FindBatchMagicPayload(use.effectPlugin, use.effectLocalID);
            if (!payload || payload->refs.empty() || payload->representativeAbsMagnitude <= 0.0001f) {
                SKSE::log::warn(
                    "[BATCH MAGIC CONFIG] {} unresolved payload {}:{:06X}",
                    a_label,
                    use.effectPlugin,
                    use.effectLocalID);
                allResolved = false;
                continue;
            }
            nativeWeightedTotal += payload->representativeAbsMagnitude * use.expectedPerTarget;
        }

        if (nativeWeightedTotal <= 0.0001f) {
            return false;
        }

        const float targetBudget = a_scaledBudget * a_budgetShare;
        const float scale = targetBudget / nativeWeightedTotal;

        for (const auto& use : a_uses) {
            auto* payload = FindBatchMagicPayload(use.effectPlugin, use.effectLocalID);
            if (!payload) {
                continue;
            }

            for (auto& ref : payload->refs) {
                if (!ref.effect) {
                    continue;
                }
                const float before = ref.effect->effectItem.magnitude;
                ref.effect->effectItem.magnitude = ref.nativeMagnitude * scale;
                SKSE::log::info(
                    "[BATCH MAGIC BALANCE] {} spell={:08X} effect={:08X} native={:.3f} before={:.3f} after={:.3f} expectedPerTarget={:.2f} scale={:.5f}",
                    a_label,
                    ref.spellFormID,
                    ref.effectFormID,
                    ref.nativeMagnitude,
                    before,
                    ref.effect->effectItem.magnitude,
                    use.expectedPerTarget,
                    scale);
            }
        }

        SKSE::log::info(
            "[BATCH MAGIC BUDGET] {} scaledBudget={:.3f} budgetShare={:.3f} targetBudget={:.3f} nativeWeightedTotal={:.3f} scale={:.5f} resolved={}",
            a_label,
            a_scaledBudget,
            a_budgetShare,
            targetBudget,
            nativeWeightedTotal,
            scale,
            allResolved);
        return allResolved;
    }

    void ResolveTempestCrescentDamagePayloads(RE::TESDataHandler* a_dataHandler)
    {
        g_tempestCrescentDamageEffects.clear();
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
                if (localID != kTempestCrescentDamageEffectLocalID) {
                    continue;
                }

                g_tempestCrescentDamageEffects.push_back({
                    effect,
                    effect->effectItem.magnitude,
                    spell->GetFormID(),
                    baseEffect->GetFormID()
                });

                SKSE::log::info(
                    "[TEMPEST RESOLVE] spell={:08X} spellName={} effect={:08X} effectName={} nativeMagnitude={:.3f}",
                    spell->GetFormID(),
                    spell->GetName(),
                    baseEffect->GetFormID(),
                    baseEffect->GetName(),
                    effect->effectItem.magnitude);
            }
        }

        SKSE::log::info(
            "[TEMPEST RESOLVE] damageEffectRefs={} expectedLines={:.0f}",
            g_tempestCrescentDamageEffects.size(),
            kTempestCrescentLines);
    }

    bool ConfigureTempestCrescentDamage(float a_scaledBudget)
    {
        if (g_tempestCrescentDamageEffects.empty()) {
            SKSE::log::warn("[TEMPEST BALANCE] no E5B spell payload references resolved");
            return false;
        }

        const float perLine = a_scaledBudget / kTempestCrescentLines;
        for (auto& ref : g_tempestCrescentDamageEffects) {
            if (!ref.effect) {
                continue;
            }

            const float before = ref.effect->effectItem.magnitude;
            ref.effect->effectItem.magnitude = perLine;
            SKSE::log::info(
                "[TEMPEST BALANCE] spell={:08X} effect={:08X} magnitudeBefore={:.3f} native={:.3f} magnitudeAfter={:.3f} totalBudget={:.3f} lines={:.0f}",
                ref.spellFormID,
                ref.effectFormID,
                before,
                ref.nativeMagnitude,
                perLine,
                a_scaledBudget,
                kTempestCrescentLines);
        }
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
        float maxMagicka = 100.0f;
        const float maxMagickaMult = GetMaxMagickaTechniqueMultiplier(a_actor, maxMagicka);
        const float baseBudget = GetMagicRankBaseDamage(a_def.tier);
        const float scaledBudget = baseBudget * alterationMult * maxMagickaMult;

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

        case 0x809:  // Frostbreak: one dedicated frost-stomp payload; slow remains untouched.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kEldenSkyrimPlugin, 0x000B1D, 1.0f } },
                "Frostbreak");
            break;

        case 0x86F:  // Honed Bolt: location strike + bolt share the Master budget.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                {
                    { kRimSkillsPlugin, 0x00B34C, 1.0f },
                    { kRimSkillsPlugin, 0x00B34E, 1.0f }
                },
                "Honed Bolt");
            break;

        case 0x827:  // Avalanche: dedicated slam blast; external frost/slow stays native.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x000ACB, 1.0f } },
                "Avalanche");
            break;

        case 0x82A:  // Gale Sever: six travelling air-blade pulses, each with two damage components.
        case 0x82B:  // Psionic Reaping uses the same six-pulse payload family.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                {
                    { kRimSkillsPlugin, 0x00B3C6, 6.0f },
                    { kRimSkillsPlugin, 0x000E3F, 6.0f }
                },
                a_def.localFormID == 0x82A ? "Gale Sever" : "Psionic Reaping");
            break;

        case 0x835:  // Titan Spear: one summoned spear damage payload.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x000D9E, 1.0f } },
                "Titan Spear");
            break;

        case 0x846:  // Lionfire: one flame-wave payload. T1 after batch retier.
        case 0x89A:  // Ember Infusion shares the flame-wave payload; weapon fire remains native.
        case 0x8C6:  // Inferno Infusion uses the same wave at T3.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x00B5E2, 1.0f } },
                a_def.localFormID == 0x846 ? "Lionfire" :
                    (a_def.localFormID == 0x89A ? "Ember Infusion" : "Inferno Infusion"));
            break;

        case 0x853:  // Runic Firebrand: dedicated RimSkills fireball spell, effect 0 is the damage payload.
            configured = SetSpellEffectMagnitude(
                g_runicFirebrandDamageSpell,
                0,
                scaledBudget,
                "Runic Firebrand / Fireball");
            break;

        case 0x855:  // Runic Disruption: one dedicated air/impact blast.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x000E88, 1.0f } },
                "Runic Disruption");
            break;

        case 0x856:  // Gravefire: two elemental impacts plus the 100/65/25 fire-storm sequence.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                {
                    { kRimSkillsPlugin, 0x000C2E, 2.0f },
                    { kRimSkillsPlugin, 0x000F5F, 1.0f },
                    { kRimSkillsPlugin, 0x000F60, 1.0f },
                    { kRimSkillsPlugin, 0x000F61, 1.0f }
                },
                "Gravefire");
            break;

        case 0x875:  // Stormwyrm Spear: three shock contacts plus two bolt-explosion contacts.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                {
                    { kRimSkillsPlugin, 0x00B47B, 3.0f },
                    { kRimSkillsPlugin, 0x00B3EA, 2.0f }
                },
                "Stormwyrm Spear");
            break;

        case 0x877:  // Wyrmforce: three raw-draconic skewer explosions per centred target.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x00B414, 3.0f } },
                "Wyrmforce");
            break;

        case 0x897:  // Serpentflame Assault: elemental opener + 100/65/25 storm sequence.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                {
                    { kRimSkillsPlugin, 0x000C2E, 1.0f },
                    { kRimSkillsPlugin, 0x000F5F, 1.0f },
                    { kRimSkillsPlugin, 0x000F60, 1.0f },
                    { kRimSkillsPlugin, 0x000F61, 1.0f }
                },
                "Serpentflame Assault");
            break;

        case 0x8AF:  // Judgment Arc: one wide Judgment Blast per target.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x00B777, 1.0f } },
                "Judgment Arc");
            break;

        case 0x82C:  // Grave Ember: elemental fire impact; low Ignite DoT remains native.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x000C2E, 1.0f } },
                "Grave Ember");
            break;

        case 0x84A: {  // Razorwind Sigil: five air lines share one Expert budget.
            constexpr float kRazorwindLines = 5.0f;
            const float perLine = scaledBudget / kRazorwindLines;
            bool any = false;
            for (auto& ref : g_tempestCrescentDamageEffects) {
                if (!ref.effect) {
                    continue;
                }
                ref.effect->effectItem.magnitude = std::copysign(perLine, ref.nativeMagnitude == 0.0f ? 1.0f : ref.nativeMagnitude);
                any = true;
                SKSE::log::info(
                    "[RAZORWIND BALANCE] spell={:08X} effect={:08X} native={:.3f} magnitudeAfter={:.3f} lines={:.0f} totalBudget={:.3f}",
                    ref.spellFormID,
                    ref.effectFormID,
                    ref.nativeMagnitude,
                    ref.effect->effectItem.magnitude,
                    kRazorwindLines,
                    scaledBudget);
            }
            configured = any;
            break;
        }

        case 0x87E:  // Runic Might: keep its drain/impact package at the new T1 budget.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x00B46C, 1.0f } },
                "Runic Might");
            break;

        case 0x8AA:  // Flamewake: T2 area strike; reserve half the magic budget for its native weapon/hazard package.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x00B730, 1.0f } },
                "Flamewake Sigil",
                0.50f);
            break;

        case 0x8AC:  // Frostwake mirrors Flamewake and keeps its slow/hazard payload native.
            configured = ConfigureBatchMagicPayloadGroup(
                scaledBudget,
                { { kRimSkillsPlugin, 0x00B753, 1.0f } },
                "Frostwake Sigil",
                0.50f);
            break;

        case 0x86D: {  // Tempest Crescent: seven light lines share one Expert budget.
            configured = ConfigureTempestCrescentDamage(scaledBudget);
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
            "[MAGIC DAMAGE CONFIG] Technique={} stone={:03X} rank={} baseMagicBudget={:.2f} alteration={:.2f} alterationMult={:.4f} maxMagicka={:.2f} maxMagickaMult={:.4f} scaledMagicBudget={:.2f} configured={}",
            a_def.name,
            a_def.localFormID,
            GetMagicRankName(a_def.tier),
            baseBudget,
            alterationSkill,
            alterationMult,
            maxMagicka,
            maxMagickaMult,
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


    bool IsTechniqueChargePlayerReady(RE::Actor* a_actor)
    {
        // Input events begin firing before a loaded/new game has placed the
        // PlayerCharacter into a cell. Calling Actor::GetLevel() during that
        // startup window can dereference uninitialized player state.
        return a_actor &&
               a_actor->IsPlayerRef() &&
               a_actor->GetParentCell() != nullptr;
    }

    std::uint32_t GetTechniqueMaxCharges(RE::Actor* a_actor)
    {
        if (!IsTechniqueChargePlayerReady(a_actor)) {
            return g_chargeState.previousMaxCharges > 0 ?
                g_chargeState.previousMaxCharges : 1u;
        }

        const auto level = a_actor->GetLevel();
        if (level >= 40) {
            return 5;
        }
        if (level >= 30) {
            return 4;
        }
        if (level >= 20) {
            return 3;
        }
        if (level >= 10) {
            return 2;
        }
        return 1;
    }

    float GetTechniqueRecoveryPercent(RE::Actor* a_actor)
    {
        if (!a_actor || !g_techniqueRecoveryEffect) {
            return 0.0f;
        }

        auto* magicTarget = a_actor->AsMagicTarget();
        auto* effects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!effects) {
            return 0.0f;
        }

        float bestMagnitude = 0.0f;
        for (const auto* activeEffect : *effects) {
            if (!activeEffect ||
                activeEffect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled) ||
                activeEffect->GetBaseObject() != g_techniqueRecoveryEffect) {
                continue;
            }

            const float magnitude = activeEffect->GetMagnitude();
            if (std::isfinite(magnitude)) {
                bestMagnitude = std::max(bestMagnitude, magnitude);
            }
        }

        return std::clamp(bestMagnitude, 0.0f, kMaxTechniqueRecoveryPercent);
    }

    float GetTechniqueRechargeSeconds(RE::Actor* a_actor)
    {
        const float recoveryPercent = GetTechniqueRecoveryPercent(a_actor);
        return kBaseTechniqueRechargeSeconds * (1.0f - (recoveryPercent / 100.0f));
    }

    void SyncTechniqueChargeGlobals(RE::Actor* a_actor)
    {
        if (!a_actor || !g_chargeState.initialized) {
            return;
        }

        const std::uint32_t maxCharges = GetTechniqueMaxCharges(a_actor);
        const float rechargeSeconds = std::max(0.01f, GetTechniqueRechargeSeconds(a_actor));
        const float normalizedProgress =
            g_chargeState.currentCharges >= maxCharges ?
                0.0f :
                std::clamp(g_chargeState.rechargeProgressSeconds / rechargeSeconds, 0.0f, 1.0f);
        const float recoveryPercent = GetTechniqueRecoveryPercent(a_actor);

        if (g_techniqueChargesGlobal) {
            g_techniqueChargesGlobal->value = static_cast<float>(g_chargeState.currentCharges);
        }
        if (g_techniqueMaxChargesGlobal) {
            g_techniqueMaxChargesGlobal->value = static_cast<float>(maxCharges);
        }
        if (g_techniqueRechargeProgressGlobal) {
            g_techniqueRechargeProgressGlobal->value = normalizedProgress;
        }
        if (g_techniqueRecoveryGlobal) {
            g_techniqueRecoveryGlobal->value = recoveryPercent;
        }
    }

    void InitializeTechniqueChargeState(RE::Actor* a_actor)
    {
        if (!IsTechniqueChargePlayerReady(a_actor)) {
            return;
        }

        const std::uint32_t maxCharges = GetTechniqueMaxCharges(a_actor);
        g_chargeState.currentCharges = maxCharges;
        g_chargeState.previousMaxCharges = maxCharges;
        g_chargeState.rechargeProgressSeconds = 0.0f;
        g_chargeState.initialized = true;
        g_chargeLastUpdate = std::chrono::steady_clock::now();
        g_chargeClockStarted = true;

        if (auto* ui = RE::UI::GetSingleton()) {
            g_chargeClockPaused = ui->GameIsPaused();
        } else {
            g_chargeClockPaused = false;
        }

        SyncTechniqueChargeGlobals(a_actor);
        SKSE::log::info(
            "[CHARGE INIT] level={} current={}/{} recharge={:.2f}s recovery={:.1f}%",
            a_actor->GetLevel(),
            g_chargeState.currentCharges,
            maxCharges,
            GetTechniqueRechargeSeconds(a_actor),
            GetTechniqueRecoveryPercent(a_actor));
    }

    void AdvanceTechniqueChargeState(
        RE::Actor* a_actor,
        std::chrono::steady_clock::time_point a_now,
        bool a_countElapsed)
    {
        if (!a_actor) {
            return;
        }

        if (!g_chargeState.initialized) {
            InitializeTechniqueChargeState(a_actor);
            return;
        }

        const std::uint32_t maxCharges = GetTechniqueMaxCharges(a_actor);

        if (g_chargeState.previousMaxCharges == 0) {
            g_chargeState.previousMaxCharges = maxCharges;
        }

        if (maxCharges > g_chargeState.previousMaxCharges) {
            const std::uint32_t gained = maxCharges - g_chargeState.previousMaxCharges;
            g_chargeState.currentCharges =
                std::min(maxCharges, g_chargeState.currentCharges + gained);
            SKSE::log::info(
                "[CHARGE CAPACITY] level={} max {}->{} current={} (+{} unlocked)",
                a_actor->GetLevel(),
                g_chargeState.previousMaxCharges,
                maxCharges,
                g_chargeState.currentCharges,
                gained);
        } else if (maxCharges < g_chargeState.previousMaxCharges) {
            g_chargeState.currentCharges = std::min(g_chargeState.currentCharges, maxCharges);
        }
        g_chargeState.previousMaxCharges = maxCharges;

        if (!g_chargeClockStarted) {
            g_chargeLastUpdate = a_now;
            g_chargeClockStarted = true;
            SyncTechniqueChargeGlobals(a_actor);
            return;
        }

        float elapsedSeconds = 0.0f;
        if (a_countElapsed) {
            elapsedSeconds = std::chrono::duration<float>(a_now - g_chargeLastUpdate).count();
            if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f || elapsedSeconds > 3600.0f) {
                elapsedSeconds = 0.0f;
            }
        }
        g_chargeLastUpdate = a_now;

        if (g_chargeState.currentCharges < maxCharges && elapsedSeconds > 0.0f) {
            g_chargeState.rechargeProgressSeconds += elapsedSeconds;

            const float rechargeSeconds = std::max(0.01f, GetTechniqueRechargeSeconds(a_actor));
            std::uint32_t restored = 0;
            while (g_chargeState.currentCharges < maxCharges &&
                   g_chargeState.rechargeProgressSeconds + 0.0001f >= rechargeSeconds) {
                g_chargeState.rechargeProgressSeconds -= rechargeSeconds;
                ++g_chargeState.currentCharges;
                ++restored;
            }

            if (g_chargeState.currentCharges >= maxCharges) {
                g_chargeState.rechargeProgressSeconds = 0.0f;
            }

            if (restored > 0) {
                SKSE::log::info(
                    "[CHARGE RECHARGE] +{} current={}/{} nextProgress={:.2f}/{:.2f}s recovery={:.1f}%",
                    restored,
                    g_chargeState.currentCharges,
                    maxCharges,
                    g_chargeState.rechargeProgressSeconds,
                    rechargeSeconds,
                    GetTechniqueRecoveryPercent(a_actor));
            }
        } else if (g_chargeState.currentCharges >= maxCharges) {
            g_chargeState.rechargeProgressSeconds = 0.0f;
        }

        SyncTechniqueChargeGlobals(a_actor);
    }

    void UpdateTechniqueChargeState(RE::Actor* a_actor)
    {
        if (!IsTechniqueChargePlayerReady(a_actor)) {
            return;
        }

        auto* ui = RE::UI::GetSingleton();
        const bool pausedNow = ui && ui->GameIsPaused();
        const auto now = std::chrono::steady_clock::now();

        // g_chargeClockPaused describes the interval since g_chargeLastUpdate.
        AdvanceTechniqueChargeState(a_actor, now, !g_chargeClockPaused);
        g_chargeClockPaused = pausedNow;
    }

    bool SpendTechniqueCharges(RE::Actor* a_actor, std::uint32_t a_cost, RE::FormID a_signalSpell)
    {
        if (!a_actor || a_cost == 0) {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();

        // Protect against duplicate SpellCast events from the same animation payload.
        if (g_lastChargeSignalSpell == a_signalSpell &&
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - g_lastChargeSignalTime).count() < 250) {
            SKSE::log::info("[CHARGE SPEND] duplicate signal {:08X} ignored", a_signalSpell);
            return true;
        }

        UpdateTechniqueChargeState(a_actor);

        const std::uint32_t maxCharges = GetTechniqueMaxCharges(a_actor);
        if (g_chargeState.currentCharges < a_cost) {
            SKSE::log::warn(
                "[CHARGE SPEND] insufficient cost={} current={}/{} signal={:08X}",
                a_cost,
                g_chargeState.currentCharges,
                maxCharges,
                a_signalSpell);
            return false;
        }

        const bool wasFull = g_chargeState.currentCharges >= maxCharges;
        g_chargeState.currentCharges -= a_cost;
        if (wasFull) {
            // Starting a new recharge queue begins at zero progress.
            g_chargeState.rechargeProgressSeconds = 0.0f;
            g_chargeLastUpdate = now;
            g_chargeClockStarted = true;
        }

        g_lastChargeSignalSpell = a_signalSpell;
        g_lastChargeSignalTime = now;

        SyncTechniqueChargeGlobals(a_actor);
        SKSE::log::info(
            "[CHARGE SPEND] cost={} current={}/{} recharge={:.2f}s recovery={:.1f}% signal={:08X}",
            a_cost,
            g_chargeState.currentCharges,
            maxCharges,
            GetTechniqueRechargeSeconds(a_actor),
            GetTechniqueRecoveryPercent(a_actor),
            a_signalSpell);
        return true;
    }

    bool HandleTechniqueChargeEffect(RE::Actor* a_actor, RE::FormID a_effectFormID)
    {
        if (g_adeptChargeEffect && a_effectFormID == g_adeptChargeEffect->GetFormID()) {
            const bool spent = SpendTechniqueCharges(a_actor, 1, a_effectFormID);
            if (spent && GetEquippedPhysicalStone(a_actor)) {
                QueuePhysicalTechniqueStaminaRefund(a_actor);
            }
            return true;
        }
        if (g_expertChargeEffect && a_effectFormID == g_expertChargeEffect->GetFormID()) {
            const bool spent = SpendTechniqueCharges(a_actor, 2, a_effectFormID);
            if (spent && GetEquippedPhysicalStone(a_actor)) {
                QueuePhysicalTechniqueStaminaRefund(a_actor);
            }
            return true;
        }
        if (g_masterChargeEffect && a_effectFormID == g_masterChargeEffect->GetFormID()) {
            const bool spent = SpendTechniqueCharges(a_actor, 3, a_effectFormID);
            if (spent && GetEquippedPhysicalStone(a_actor)) {
                QueuePhysicalTechniqueStaminaRefund(a_actor);
            }
            return true;
        }
        return false;
    }

    class TechniqueChargeInputSink final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_events,
            RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!a_events) {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                // Safe during title/loading screens: Update exits until the
                // player has a valid parent cell.
                UpdateTechniqueChargeState(player);

                for (const RE::InputEvent* input = *a_events; input; input = input->next) {
                    const auto* button = input->AsButtonEvent();
                    if (!button || !button->IsDown()) {
                        continue;
                    }
                    if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                        button->GetIDCode() == kTechniqueInputKeyCode) {
                        ArmPhysicalTechniqueStaminaRefund(player);
                        break;
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class TechniqueChargeMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                const auto now = std::chrono::steady_clock::now();

                // Count or discard the interval based on the pause state that
                // was active BEFORE this menu transition.
                AdvanceTechniqueChargeState(player, now, !g_chargeClockPaused);

                if (auto* ui = RE::UI::GetSingleton()) {
                    g_chargeClockPaused = ui->GameIsPaused();
                }
                g_chargeLastUpdate = now;
                g_chargeClockStarted = true;
                SyncTechniqueChargeGlobals(player);
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void RegisterTechniqueChargeSinks()
    {
        if (!g_chargeInputSinkRegistered) {
            static TechniqueChargeInputSink inputSink;
            if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
                input->AddEventSink(&inputSink);
                g_chargeInputSinkRegistered = true;
                SKSE::log::info("Technique Charge input sink registered");
            }
        }

        if (!g_chargeMenuSinkRegistered) {
            static TechniqueChargeMenuSink menuSink;
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
                g_chargeMenuSinkRegistered = true;
                g_chargeClockPaused = ui->GameIsPaused();
                SKSE::log::info("Technique Charge menu/pause sink registered");
            }
        }
    }

    void ResetTechniqueChargeState()
    {
        g_chargeState = {};
        g_chargeClockStarted = false;
        g_chargeClockPaused = false;
        g_lastChargeSignalSpell = 0;
        g_lastChargeSignalTime = {};
        g_physicalStaminaRefundArmed = false;
        g_physicalStaminaSnapshot = 0.0f;
        g_physicalStaminaSnapshotTime = {};
    }

    void SaveTechniqueChargeState(SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc) {
            return;
        }

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            UpdateTechniqueChargeState(player);
        }

        SerializedTechniqueChargeState saved{};
        saved.currentCharges = g_chargeState.currentCharges;
        saved.previousMaxCharges = g_chargeState.previousMaxCharges;
        saved.rechargeProgressSeconds = g_chargeState.rechargeProgressSeconds;
        saved.initialized = g_chargeState.initialized ? 1u : 0u;

        if (!a_intfc->WriteRecord(kChargeStateRecord, kChargeStateVersion, saved)) {
            SKSE::log::error("[CHARGE SAVE] failed to write state");
        } else {
            SKSE::log::info(
                "[CHARGE SAVE] current={} previousMax={} progress={:.2f} initialized={}",
                saved.currentCharges,
                saved.previousMaxCharges,
                saved.rechargeProgressSeconds,
                saved.initialized);
        }
    }

    void LoadTechniqueChargeState(SKSE::SerializationInterface* a_intfc)
    {
        ResetTechniqueChargeState();
        if (!a_intfc) {
            return;
        }

        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;
        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            if (type != kChargeStateRecord || version != kChargeStateVersion) {
                continue;
            }

            SerializedTechniqueChargeState saved{};
            if (length != sizeof(saved) ||
                a_intfc->ReadRecordData(saved) != sizeof(saved)) {
                SKSE::log::warn(
                    "[CHARGE LOAD] invalid record length={} expected={}",
                    length,
                    sizeof(saved));
                continue;
            }

            g_chargeState.currentCharges = saved.currentCharges;
            g_chargeState.previousMaxCharges = saved.previousMaxCharges;
            g_chargeState.rechargeProgressSeconds =
                std::max(0.0f, saved.rechargeProgressSeconds);
            g_chargeState.initialized = saved.initialized != 0;
            SKSE::log::info(
                "[CHARGE LOAD] current={} previousMax={} progress={:.2f} initialized={}",
                g_chargeState.currentCharges,
                g_chargeState.previousMaxCharges,
                g_chargeState.rechargeProgressSeconds,
                g_chargeState.initialized);
        }

        g_chargeLastUpdate = std::chrono::steady_clock::now();
        g_chargeClockStarted = true;
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

            if (g_coveringFireActivationSpell &&
                a_event->spell == g_coveringFireActivationSpell->GetFormID()) {
                SpendCoveringFireStamina(player);
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
        // v0.4.6 batch audit: trace every equipped Magic/Rune Technique so the
        // remaining payload mappings can be captured in one play session.
        (void)a_localFormID;
        return true;
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
        return elapsed >= 0 && elapsed <= 8000;
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

            if (target->IsPlayerRef() && HandleTechniqueChargeEffect(player, a_event->magicEffect)) {
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
                if (!activeEffect) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto* baseEffect = activeEffect->GetBaseObject();
                if (!baseEffect) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                // Test-only batch-audit bypass. Cooldown effects live in the
                // Ash Cooldown plugin, while these three effects are essential
                // plumbing and must remain active long enough to route/trace.
                if (kBatchAuditDisableCooldowns) {
                    const auto sourcePlugin = GetSourcePlugin(baseEffect);
                    const auto localID = GetLocalFormID(baseEffect);
                    const bool essential =
                        localID == kTechniqueMarkerLocalID ||
                        localID == kMagicCandidatePrimaryLocalID ||
                        localID == kMagicCandidateSecondaryLocalID;

                    if (sourcePlugin == kCooldownPlugin && !essential) {
                        SKSE::log::info(
                            "[BATCH COOLDOWN BYPASS] Technique={} stone={:03X} effect={:08X} local={:06X} name={} uniqueID={} -> dispel",
                            def->name,
                            def->localFormID,
                            baseEffect->GetFormID(),
                            localID,
                            baseEffect->GetName(),
                            a_event->activeEffectUniqueID);
                        activeEffect->Dispel(true);
                        return RE::BSEventNotifyControl::kContinue;
                    }
                }

                if (!g_techniqueMarker || baseEffect != g_techniqueMarker) {
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

        g_techniqueChargesGlobal = dataHandler->LookupForm<RE::TESGlobal>(
            kTechniqueChargesGlobalLocalID, kStonePlugin);
        g_techniqueMaxChargesGlobal = dataHandler->LookupForm<RE::TESGlobal>(
            kTechniqueMaxChargesGlobalLocalID, kStonePlugin);
        g_techniqueRechargeProgressGlobal = dataHandler->LookupForm<RE::TESGlobal>(
            kTechniqueRechargeProgressGlobalLocalID, kStonePlugin);
        g_techniqueRecoveryGlobal = dataHandler->LookupForm<RE::TESGlobal>(
            kTechniqueRecoveryGlobalLocalID, kStonePlugin);
        g_techniqueRecoveryEffect = dataHandler->LookupForm<RE::EffectSetting>(
            kTechniqueRecoveryEffectLocalID, kStonePlugin);

        g_adeptChargeEffect = dataHandler->LookupForm<RE::EffectSetting>(
            kAdeptChargeEffectLocalID, kCooldownPlugin);
        g_expertChargeEffect = dataHandler->LookupForm<RE::EffectSetting>(
            kExpertChargeEffectLocalID, kCooldownPlugin);
        g_masterChargeEffect = dataHandler->LookupForm<RE::EffectSetting>(
            kMasterChargeEffectLocalID, kCooldownPlugin);

        g_tier1AlterationProbe = dataHandler->LookupForm<RE::SpellItem>(kOakfleshLocalID, kSkyrimPlugin);
        g_tier2AlterationProbe = dataHandler->LookupForm<RE::SpellItem>(kStonefleshLocalID, kSkyrimPlugin);
        g_tier3AlterationProbe = dataHandler->LookupForm<RE::SpellItem>(kIronfleshLocalID, kSkyrimPlugin);

        g_galeCrescentDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kGaleCrescentDamageSpellLocalID, kRimSkillsPlugin);
        g_dragonfireSigilDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kDragonfireSigilDamageSpellLocalID, kRimSkillsPlugin);
        g_runicFirebrandDamageSpell = dataHandler->LookupForm<RE::SpellItem>(
            kRunicFirebrandDamageSpellLocalID, kRimSkillsPlugin);
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
        g_coveringFireActivationSpell = dataHandler->LookupForm<RE::SpellItem>(
            kCoveringFireActivationSpellLocalID, kRimSkillsPlugin);

        g_techniqueMarker = dataHandler->LookupForm<RE::EffectSetting>(kTechniqueMarkerLocalID, kCooldownPlugin);
        g_magicCandidatePrimary = dataHandler->LookupForm<RE::EffectSetting>(kMagicCandidatePrimaryLocalID, kCooldownPlugin);
        g_magicCandidateSecondary = dataHandler->LookupForm<RE::EffectSetting>(kMagicCandidateSecondaryLocalID, kCooldownPlugin);

        SKSE::log::info("Resolved {}/{} physical Technique Stones", resolved, kTechniqueDamageDefinitions.size());
        SKSE::log::info("Resolved {}/{} Magic/Rune Technique Stones", magicResolved, kMagicTechniqueDefinitions.size());
        SKSE::log::info("Resolved {}/{} Magic activation signals", activationResolved, kMagicActivationSignals.size());
        SKSE::log::info(
            "Technique Charge globals: current={} max={} progress={} recovery={} effect={}",
            g_techniqueChargesGlobal ? "OK" : "MISSING",
            g_techniqueMaxChargesGlobal ? "OK" : "MISSING",
            g_techniqueRechargeProgressGlobal ? "OK" : "MISSING",
            g_techniqueRecoveryGlobal ? "OK" : "MISSING",
            g_techniqueRecoveryEffect ? "OK" : "MISSING");
        SKSE::log::info(
            "Technique Charge rank effects: Adept={} Expert={} Master={}",
            g_adeptChargeEffect ? "OK" : "MISSING",
            g_expertChargeEffect ? "OK" : "MISSING",
            g_masterChargeEffect ? "OK" : "MISSING");

        SKSE::log::info(
            "Alteration probes: T1={} T2={} T3={}",
            g_tier1AlterationProbe ? "Oakflesh" : "MISSING",
            g_tier2AlterationProbe ? "Stoneflesh" : "MISSING",
            g_tier3AlterationProbe ? "Ironflesh" : "MISSING");

        SKSE::log::info(
            "Magic damage proof spells: Gale={} Dragonfire={} RunicFirebrand={} Triplecut={}/{}/{}",
            g_galeCrescentDamageSpell ? "OK" : "MISSING",
            g_dragonfireSigilDamageSpell ? "OK" : "MISSING",
            g_runicFirebrandDamageSpell ? "OK" : "MISSING",
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
        SKSE::log::info(
            "Covering Fire activation spell: {}",
            g_coveringFireActivationSpell ? "OK" : "MISSING");
        ResolveBatchMagicPayloads(dataHandler);
        ResolveTempestCrescentDamagePayloads(dataHandler);
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
        case SKSE::MessagingInterface::kInputLoaded:
            RegisterTechniqueChargeSinks();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            ResolveForms();
            RegisterMagicDiagnosticSinks();
            RegisterTechniqueChargeSinks();
            RegisterPrecision();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            RegisterTechniqueChargeSinks();
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                if (!g_chargeState.initialized) {
                    InitializeTechniqueChargeState(player);
                } else {
                    g_chargeLastUpdate = std::chrono::steady_clock::now();
                    g_chargeClockStarted = true;
                    if (auto* ui = RE::UI::GetSingleton()) {
                        g_chargeClockPaused = ui->GameIsPaused();
                    }
                    AdvanceTechniqueChargeState(player, g_chargeLastUpdate, false);
                }
            }
            break;
        case SKSE::MessagingInterface::kNewGame:
            ResetTechniqueChargeState();
            RegisterTechniqueChargeSinks();
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                InitializeTechniqueChargeState(player);
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
        *path /= "HE_TechniqueDamage.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("HE_TechniqueDamage", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }

    SKSE::Init(a_skse);

    if (const auto* serialization = SKSE::GetSerializationInterface()) {
        serialization->SetUniqueID(kSerializationUniqueID);
        serialization->SetSaveCallback(SaveTechniqueChargeState);
        serialization->SetLoadCallback(LoadTechniqueChargeState);
        serialization->SetRevertCallback([](SKSE::SerializationInterface*) {
            ResetTechniqueChargeState();
        });
    } else {
        SKSE::log::error("SKSE serialization interface unavailable - Technique Charges will not persist");
    }

    if (const auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener("SKSE", MessageHandler);
    } else {
        return false;
    }

    SKSE::log::info("HE Technique Damage v0.6.6 Covering Fire activation-gated stamina loaded");
    return true;
}
