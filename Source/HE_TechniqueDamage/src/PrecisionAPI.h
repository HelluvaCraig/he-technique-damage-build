#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <functional>
#include <vector>
#include <Windows.h>

namespace PRECISION_API
{
    constexpr const auto PrecisionPluginName = "Precision";

    enum class InterfaceVersion : std::uint8_t { V1, V2, V3, V4 };
    enum class APIResult : std::uint8_t { OK, AlreadyRegistered, NotRegistered };

    struct PreHitModifier
    {
        enum class ModifierType : std::uint8_t { Damage, Stagger };
        enum class ModifierOperation : std::uint8_t { Additive, Multiplicative };
        ModifierType modifierType;
        ModifierOperation modifierOperation;
        float modifierValue;
    };

    struct PreHitCallbackReturn
    {
        bool bIgnoreHit = false;
        std::vector<PreHitModifier> modifiers;
    };

    enum class CollisionFilterComparisonResult : std::uint8_t { Continue, Collide, Ignore };
    enum class RequestedAttackCollisionType : std::uint8_t { Default, Current, RightWeapon, LeftWeapon };

    struct PrecisionHitData
    {
        PrecisionHitData(RE::Actor* a_attacker, RE::TESObjectREFR* a_target, RE::hkpRigidBody* a_hitRigidBody,
            RE::hkpRigidBody* a_hittingRigidBody, const RE::NiPoint3& a_hitPos, const RE::NiPoint3& a_separatingNormal,
            const RE::NiPoint3& a_hitPointVelocity, RE::hkpShapeKey a_hitBodyShapeKey, RE::hkpShapeKey a_hittingBodyShapeKey) :
            attacker(a_attacker), target(a_target), hitRigidBody(a_hitRigidBody), hittingRigidBody(a_hittingRigidBody),
            hitPos(a_hitPos), separatingNormal(a_separatingNormal), hitPointVelocity(a_hitPointVelocity),
            hitBodyShapeKey(a_hitBodyShapeKey), hittingBodyShapeKey(a_hittingBodyShapeKey) {}

        RE::Actor* attacker;
        RE::TESObjectREFR* target;
        RE::hkpRigidBody* hitRigidBody;
        RE::hkpRigidBody* hittingRigidBody;
        RE::NiPoint3 hitPos;
        RE::NiPoint3 separatingNormal;
        RE::NiPoint3 hitPointVelocity;
        RE::hkpShapeKey hitBodyShapeKey;
        RE::hkpShapeKey hittingBodyShapeKey;
    };

    using PreHitCallback = std::function<PreHitCallbackReturn(const PrecisionHitData&)>;
    using PostHitCallback = std::function<void(const PrecisionHitData&, const RE::HitData&)>;
    using PrePhysicsStepCallback = std::function<void(RE::bhkWorld*)>;
    using CollisionFilterComparisonCallback = std::function<CollisionFilterComparisonResult(RE::bhkCollisionFilter*, std::uint32_t, std::uint32_t)>;

    class IVPrecision1
    {
    public:
        virtual APIResult AddPreHitCallback(SKSE::PluginHandle, PreHitCallback&&) noexcept = 0;
        virtual APIResult AddPostHitCallback(SKSE::PluginHandle, PostHitCallback&&) noexcept = 0;
        virtual APIResult AddPrePhysicsStepCallback(SKSE::PluginHandle, PrePhysicsStepCallback&&) noexcept = 0;
        virtual APIResult AddCollisionFilterComparisonCallback(SKSE::PluginHandle, CollisionFilterComparisonCallback&&) noexcept = 0;
        virtual APIResult RemovePreHitCallback(SKSE::PluginHandle) noexcept = 0;
        virtual APIResult RemovePostHitCallback(SKSE::PluginHandle) noexcept = 0;
        virtual APIResult RemovePrePhysicsStepCallback(SKSE::PluginHandle) noexcept = 0;
        virtual APIResult RemoveCollisionFilterComparisonCallback(SKSE::PluginHandle) noexcept = 0;
        virtual float GetAttackCollisionCapsuleLength(RE::ActorHandle, RequestedAttackCollisionType = RequestedAttackCollisionType::Default) const noexcept = 0;
    };

    using _RequestPluginAPI = void* (*)(InterfaceVersion);

    [[nodiscard]] inline IVPrecision1* RequestPluginAPI()
    {
        auto module = ::GetModuleHandleA("Precision.dll");
        if (!module) {
            return nullptr;
        }
        auto proc = reinterpret_cast<_RequestPluginAPI>(::GetProcAddress(module, "RequestPluginAPI"));
        return proc ? static_cast<IVPrecision1*>(proc(InterfaceVersion::V1)) : nullptr;
    }
}
