#pragma once

#include <array>
#include <cstdint>

struct TechniqueDamageDefinition
{
    std::uint32_t localFormID;
    float tierMultiplier;
    std::uint16_t contacts;
    const char* name;
};

// Physical Technique Stones only.
// tierMultiplier is the WHOLE-MOVE budget. The replacement layer applies
// tierMultiplier / contacts to each real Precision collision. Skyrim/Precision
// still selects the attacking weapon/hand for weapon contacts.
inline constexpr std::array<TechniqueDamageDefinition, 33> kTechniqueDamageDefinitions = {{
    { 0x808, 4.5f,  2, "Continuous Slash" },
    { 0x80C, 7.0f, 16, "Twinfang Dance" },
    { 0x80F, 7.0f,  1, "Hawkfall Dance" },
    { 0x811, 7.0f, 12, "Blooming Blades" },
    { 0x815, 4.5f,  2, "Vaulting Cleave" },
    { 0x81B, 7.0f,  3, "Arcane Heel" },
    { 0x81F, 4.5f,  2, "Half-Moon Slash" },
    { 0x820,10.0f, 12, "Titan's Dance" },
    { 0x82E, 7.0f,  7, "White Wolf Dance" },
    { 0x831, 7.0f,  6, "Soul Dance" },
    { 0x837, 4.5f,  1, "Reaver's Retreat" },
    { 0x843,10.0f,  6, "Storm Fist" },
    { 0x845,10.0f,  6, "Worldshaker" },
    { 0x84B, 7.0f,  9, "Relentless Rush" },
    { 0x84C, 7.0f, 10, "Predator's Pursuit" },
    { 0x84E, 7.0f,  9, "Aegis Assault" },
    { 0x852, 4.5f,  2, "Knight's Uppercut" },
    { 0x864, 4.5f,  2, "Direwolf's Leap" },
    { 0x86A, 7.0f,  2, "Whirling Steel" },
    { 0x86C, 7.0f,  4, "Falcon Vault" },
    { 0x86E, 4.5f,  1, "Skybreaker Kick" },
    { 0x870, 7.0f,  3, "Tornado Slash" },
    { 0x872, 7.0f,  7, "Colossus Strike" },
    { 0x873, 4.5f,  4, "Steel Flourish" },
    { 0x87C, 7.0f,  2, "Troll's Roar" },
    { 0x88B, 7.0f,  3, "Parting Grass" },
    { 0x895, 4.5f,  3, "High Guard" },
    { 0x896, 4.5f,  2, "Reaping Sweep" },
    { 0x89F,10.0f,  2, "Arcane Descent" },
    { 0x8A6, 4.5f,  4, "Cross Slash" },
    { 0x8B0, 7.0f,  2, "Sanguine Pursuit" },
    { 0x8B8, 4.5f,  1, "Knifehand Strike" },
    { 0x8B9, 7.0f,  1, "Arcane Impetus" },
}};
