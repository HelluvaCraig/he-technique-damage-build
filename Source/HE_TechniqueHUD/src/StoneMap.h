#pragma once

#include <array>
#include <cstdint>

enum class Category : std::uint8_t { None, Red, Green, Blue, Orange };
enum class Resource : std::uint8_t { Stamina, Magicka };
enum class EquipRule : std::uint8_t {
    Any,
    DualWield1H,
    TwoHanded,
    NotTwoHanded,
    ShieldLeft,
    Colossus,
    Bow,
    Crossbow,
    OneHanded,
    OneHandedOrCrossbow
};

struct StoneDefinition {
    std::uint32_t localFormID;
    Category category;
    Resource resource;
    float resourceCost;
    EquipRule equipRule;
    const char* name;
};

inline constexpr std::array<StoneDefinition, 79> kStoneDefinitions = {{
    { 0x808, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "Continuous Slash" },
    { 0x809, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Frostbreak" },
    { 0x80C, Category::Red, Resource::Stamina, 65.0f, EquipRule::DualWield1H, "Twinfang Dance" },
    { 0x80F, Category::Red, Resource::Stamina, 65.0f, EquipRule::TwoHanded, "Hawkfall Dance" },
    { 0x811, Category::Red, Resource::Stamina, 65.0f, EquipRule::DualWield1H, "Blooming Blades" },
    { 0x815, Category::Red, Resource::Stamina, 40.0f, EquipRule::TwoHanded, "Vaulting Cleave" },
    { 0x81B, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Arcane Heel" },
    { 0x81F, Category::Red, Resource::Stamina, 40.0f, EquipRule::TwoHanded, "Half-Moon Slash" },
    { 0x820, Category::Red, Resource::Stamina, 100.0f, EquipRule::TwoHanded, "Titan's Dance" },
    { 0x827, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Avalanche" },
    { 0x829, Category::Blue, Resource::Magicka, 40.0f, EquipRule::Any, "Moonlit Cleave" },
    { 0x82A, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Gale Sever" },
    { 0x82B, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Psionic Reaping" },
    { 0x82C, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Grave Ember" },
    { 0x82E, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "White Wolf Dance" },
    { 0x831, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Soul Dance" },
    { 0x832, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Radiant Blade Dance" },
    { 0x835, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Titan Spear" },
    { 0x837, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "Reaver's Retreat" },
    { 0x843, Category::Red, Resource::Stamina, 100.0f, EquipRule::NotTwoHanded, "Storm Fist" },
    { 0x845, Category::Red, Resource::Stamina, 100.0f, EquipRule::Any, "Worldshaker" },
    { 0x846, Category::Blue, Resource::Magicka, 40.0f, EquipRule::Any, "Lionfire" },
    { 0x847, Category::Orange, Resource::Magicka, 65.0f, EquipRule::Any, "Dragonfire Sigil" },
    { 0x848, Category::Blue, Resource::Magicka, 40.0f, EquipRule::ShieldLeft, "Aegis Recall" },
    { 0x84A, Category::Orange, Resource::Magicka, 65.0f, EquipRule::Any, "Razorwind Sigil" },
    { 0x84B, Category::Red, Resource::Stamina, 65.0f, EquipRule::NotTwoHanded, "Relentless Rush" },
    { 0x84C, Category::Red, Resource::Stamina, 65.0f, EquipRule::DualWield1H, "Predator's Pursuit" },
    { 0x84E, Category::Red, Resource::Stamina, 65.0f, EquipRule::ShieldLeft, "Aegis Assault" },
    { 0x852, Category::Red, Resource::Stamina, 40.0f, EquipRule::TwoHanded, "Knight's Uppercut" },
    { 0x853, Category::Orange, Resource::Magicka, 65.0f, EquipRule::Any, "Runic Firebrand" },
    { 0x855, Category::Orange, Resource::Magicka, 65.0f, EquipRule::Any, "Runic Disruption" },
    { 0x856, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Gravefire" },
    { 0x859, Category::None, Resource::Stamina, 0.0f, EquipRule::Any, "Driving Thrust (Retired)" },
    { 0x864, Category::Red, Resource::Stamina, 40.0f, EquipRule::TwoHanded, "Direwolf's Leap" },
    { 0x86A, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Whirling Steel" },
    { 0x86B, Category::None, Resource::Stamina, 0.0f, EquipRule::TwoHanded, "Crucible Smash (Retired)" },
    { 0x86C, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Falcon Vault" },
    { 0x86D, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Tempest Crescent" },
    { 0x86E, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "Skybreaker Kick" },
    { 0x86F, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Honed Bolt" },
    { 0x870, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Tornado Slash" },
    { 0x872, Category::Red, Resource::Stamina, 65.0f, EquipRule::Colossus, "Colossus Strike" },
    { 0x873, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "Steel Flourish" },
    { 0x875, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Stormwyrm Spear" },
    { 0x877, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Wyrmforce" },
    { 0x87A, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Gale Crescent" },
    { 0x87B, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Moonlit Severance" },
    { 0x87C, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Troll's Roar" },
    { 0x87D, Category::Orange, Resource::Magicka, 65.0f, EquipRule::Any, "Moonshard Sigil" },
    { 0x87E, Category::Orange, Resource::Magicka, 40.0f, EquipRule::Any, "Runic Might" },
    { 0x881, Category::Green, Resource::Stamina, 65.0f, EquipRule::Bow, "Power Shot" },
    { 0x883, Category::Green, Resource::Stamina, 70.0f, EquipRule::Crossbow, "Arbalest Reprisal" },
    { 0x887, Category::Green, Resource::Stamina, 100.0f, EquipRule::Bow, "Hurricane Volley" },
    { 0x889, Category::Green, Resource::Stamina, 70.0f, EquipRule::OneHandedOrCrossbow, "Spectral Volley" },
    { 0x88B, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Parting Grass" },
    { 0x88E, Category::Blue, Resource::Magicka, 40.0f, EquipRule::Any, "Telekinetic Maelstrom" },
    { 0x88F, Category::Blue, Resource::Magicka, 40.0f, EquipRule::Any, "Aurochs Charge" },
    { 0x890, Category::Blue, Resource::Magicka, 40.0f, EquipRule::Any, "Chilling Mist" },
    { 0x891, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Crimson Severance" },
    { 0x893, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Elder Mooncleave" },
    { 0x894, Category::Blue, Resource::Magicka, 40.0f, EquipRule::Any, "Moonlance" },
    { 0x895, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "High Guard" },
    { 0x896, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "Reaping Sweep" },
    { 0x897, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Serpentflame Assault" },
    { 0x89A, Category::Blue, Resource::Magicka, 40.0f, EquipRule::Any, "Ember Infusion" },
    { 0x89F, Category::Red, Resource::Stamina, 100.0f, EquipRule::Any, "Arcane Descent" },
    { 0x8A0, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Radiant Triplecut" },
    { 0x8A3, Category::Green, Resource::Stamina, 100.0f, EquipRule::Bow, "Tenfold Flurry" },
    { 0x8A6, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "Cross Slash" },
    { 0x8A7, Category::Green, Resource::Stamina, 70.0f, EquipRule::OneHanded, "Arcane Bombard" },
    { 0x8A9, Category::Green, Resource::Stamina, 105.0f, EquipRule::OneHanded, "Aetherfall" },
    { 0x8AA, Category::Orange, Resource::Magicka, 65.0f, EquipRule::Any, "Flamewake Sigil" },
    { 0x8AC, Category::Orange, Resource::Magicka, 65.0f, EquipRule::Any, "Frostwake Sigil" },
    { 0x8AF, Category::Blue, Resource::Magicka, 65.0f, EquipRule::Any, "Judgment Arc" },
    { 0x8B0, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Sanguine Pursuit" },
    { 0x8B8, Category::Red, Resource::Stamina, 40.0f, EquipRule::Any, "Knifehand Strike" },
    { 0x8B9, Category::Red, Resource::Stamina, 65.0f, EquipRule::Any, "Arcane Impetus" },
    { 0x8C6, Category::Blue, Resource::Magicka, 100.0f, EquipRule::Any, "Inferno Infusion" },
}};
