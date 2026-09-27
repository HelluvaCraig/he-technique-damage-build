#pragma once

#include <array>
#include <cstdint>

struct MagicTechniqueDefinition
{
    std::uint32_t localFormID;
    std::uint8_t tier;
    float tierMultiplier;
    float baseMagicka;
    const char* name;
};

// Blue + Orange Technique Stones only.
// Damage budgets mirror the physical model:
//   T1 = 4.5x equipped weapon damage
//   T2 = 7.0x equipped weapon damage
//   T3 = 10.0x equipped weapon damage
//
// v0.2 is a cost-integration proof only. The multipliers are recorded here now
// so the later magic-damage pass can use the same authoritative map.
inline constexpr std::array<MagicTechniqueDefinition, 36> kMagicTechniqueDefinitions = {{
    { 0x809, 1,  4.5f,  40.0f, "Frostbreak" },
    { 0x827, 2,  7.0f,  65.0f, "Avalanche" },
    { 0x829, 1,  4.5f,  40.0f, "Moonlit Cleave" },
    { 0x82A, 2,  7.0f,  65.0f, "Gale Sever" },
    { 0x82B, 2,  7.0f,  65.0f, "Psionic Reaping" },
    { 0x82C, 3, 10.0f, 100.0f, "Grave Ember" },
    { 0x832, 3, 10.0f, 100.0f, "Radiant Blade Dance" },
    { 0x835, 2,  7.0f,  65.0f, "Titan Spear" },
    { 0x846, 2,  7.0f,  65.0f, "Lionfire" },
    { 0x847, 2,  7.0f,  65.0f, "Dragonfire Sigil" },
    { 0x848, 2,  7.0f,  65.0f, "Aegis Recall" },
    { 0x84A, 3, 10.0f, 100.0f, "Razorwind Sigil" },
    { 0x853, 2,  7.0f,  65.0f, "Runic Firebrand" },
    { 0x855, 2,  7.0f,  65.0f, "Runic Disruption" },
    { 0x856, 2,  7.0f,  65.0f, "Gravefire" },
    { 0x86D, 2,  7.0f,  65.0f, "Tempest Crescent" },
    { 0x86F, 1,  4.5f,  40.0f, "Honed Bolt" },
    { 0x875, 2,  7.0f,  65.0f, "Stormwyrm Spear" },
    { 0x877, 2,  7.0f,  65.0f, "Wyrmforce" },
    { 0x87A, 2,  7.0f,  65.0f, "Gale Crescent" },
    { 0x87B, 2,  7.0f,  65.0f, "Moonlit Severance" },
    { 0x87D, 2,  7.0f,  65.0f, "Moonshard Sigil" },
    { 0x87E, 3, 10.0f, 100.0f, "Runic Might" },
    { 0x88E, 3, 10.0f, 100.0f, "Telekinetic Maelstrom" },
    { 0x88F, 2,  7.0f,  65.0f, "Aurochs Charge" },
    { 0x890, 1,  4.5f,  40.0f, "Chilling Mist" },
    { 0x891, 2,  7.0f,  65.0f, "Crimson Severance" },
    { 0x893, 3, 10.0f, 100.0f, "Elder Mooncleave" },
    { 0x894, 3, 10.0f, 100.0f, "Moonlance" },
    { 0x897, 2,  7.0f,  65.0f, "Serpentflame Assault" },
    { 0x89A, 2,  7.0f,  65.0f, "Ember Infusion" },
    { 0x8A0, 2,  7.0f,  65.0f, "Radiant Triplecut" },
    { 0x8AA, 3, 10.0f, 100.0f, "Flamewake Sigil" },
    { 0x8AC, 3, 10.0f, 100.0f, "Frostwake Sigil" },
    { 0x8AF, 2,  7.0f,  65.0f, "Judgment Arc" },
    { 0x8C6, 3, 10.0f, 100.0f, "Inferno Infusion" },
}};

struct MagicActivationSignalDefinition
{
    std::uint32_t localFormID;
    const char* plugin;
    const char* label;
};

// These are the spells that currently carry the once-per-activation Magicka
// payment in the v5.6.7 Technique package. The shared carrier covers most
// T2/T3 techniques; the remaining entries cover direct-paid openers.
inline constexpr std::array<MagicActivationSignalDefinition, 14> kMagicActivationSignals = {{
    { 0x0B6D3, "EldenSkyrim_RimSkills.esp", "shared carrier" },
    { 0x0B5D0, "EldenSkyrim_RimSkills.esp", "Frostbreak opener" },
    { 0x0BA53, "EldenSkyrim_RimSkills.esp", "Lionfire opener" },
    { 0x0B352, "EldenSkyrim_RimSkills.esp", "Honed Bolt opener" },
    { 0x0BA7D, "EldenSkyrim_RimSkills.esp", "Chilling Mist opener" },
    { 0x00090F, "EldenSkyrim.esp",           "Gale Sever opener" },
    { 0x0BA19, "EldenSkyrim_RimSkills.esp", "retier opener A" },
    { 0x0BA1D, "EldenSkyrim_RimSkills.esp", "retier opener B" },
    { 0x000E6F, "EldenSkyrim_RimSkills.esp", "retier opener C" },
    { 0x000FCD, "EldenSkyrim_RimSkills.esp", "retier opener D" },
    { 0x0BA1A, "EldenSkyrim_RimSkills.esp", "Moonlit Cleave opener" },
    { 0x0BA97, "EldenSkyrim_RimSkills.esp", "direct-paid opener" },
    { 0x0B739, "EldenSkyrim_RimSkills.esp", "Flamewake Sigil opener" },
    { 0x0B73A, "EldenSkyrim_RimSkills.esp", "Frostwake Sigil opener" },
}};
