#pragma once

#include <array>
#include <cstdint>

enum class MagicTechniqueTier : std::uint8_t
{
    kTier1 = 1,
    kTier2 = 2,
    kTier3 = 3
};

struct MagicTechniqueDefinition
{
    std::uint32_t localFormID;
    MagicTechniqueTier tier;
    float tierMultiplier;
    float baseMagickaCost;
    const char* name;
};

// Magic / Rune Technique Stones.
//
// The magic system intentionally uses the same whole-move weapon-damage budgets as
// physical Techniques. Magicka is handled separately so Alteration progression can
// modify the activation cost without changing the damage budget.
//
// v0.2.0 starts with one proof Technique. More surviving BLUE/ORANGE stones will be
// added once the cost path is validated in game.
inline constexpr std::array<MagicTechniqueDefinition, 1> kMagicTechniqueDefinitions = {{
    { 0x87A, MagicTechniqueTier::kTier1, 4.5f, 40.0f, "Gale Crescent" },
}};
