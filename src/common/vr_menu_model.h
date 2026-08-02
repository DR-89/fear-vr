#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fearvr {

// Navigation state for the native Retail pause-menu integration. `None` means
// that the original system menu owns the list; every other value is a VR page
// rendered by selectively showing controls in that same list.
enum class VrSettingsPage : std::uint8_t {
    None,
    Root,
    Display,
    Comfort,
    Controls,
    Weapons,
    WeaponHandling,
    WeaponWeight,
    WeaponRecoil,
    Melee,
    Advanced,
};

inline constexpr bool IsVrSettingsPage(VrSettingsPage page) noexcept {
    return page != VrSettingsPage::None;
}

inline constexpr VrSettingsPage ParentVrSettingsPage(
    VrSettingsPage page) noexcept {
    switch (page) {
    case VrSettingsPage::Display:
    case VrSettingsPage::Comfort:
    case VrSettingsPage::Controls:
    case VrSettingsPage::Weapons:
    case VrSettingsPage::Melee:
    case VrSettingsPage::Advanced:
        return VrSettingsPage::Root;
    case VrSettingsPage::WeaponHandling:
    case VrSettingsPage::WeaponWeight:
    case VrSettingsPage::WeaponRecoil:
        return VrSettingsPage::Weapons;
    case VrSettingsPage::Root:
    case VrSettingsPage::None:
    default:
        return VrSettingsPage::None;
    }
}

template <typename Value, std::size_t Size>
inline std::size_t ClosestVrPresetIndex(
    Value value, const std::array<Value, Size>& presets) noexcept {
    static_assert(Size > 0, "A VR setting needs at least one preset.");
    static_assert(std::is_arithmetic_v<Value>,
                  "VR presets must contain arithmetic values.");

    if constexpr (std::is_floating_point_v<Value>) {
        if (!std::isfinite(value)) {
            return 0;
        }
    }

    std::size_t closest = 0;
    long double closestDistance = std::fabs(
        static_cast<long double>(value) -
        static_cast<long double>(presets[0]));
    for (std::size_t index = 1; index < Size; ++index) {
        const long double distance = std::fabs(
            static_cast<long double>(value) -
            static_cast<long double>(presets[index]));
        if (distance < closestDistance) {
            closest = index;
            closestDistance = distance;
        }
    }
    return closest;
}

template <typename Value, std::size_t Size>
inline std::size_t NextVrPresetIndex(
    Value value, const std::array<Value, Size>& presets) noexcept {
    static_assert(Size > 0, "A VR setting needs at least one preset.");
    return (ClosestVrPresetIndex(value, presets) + 1U) % Size;
}

} // namespace fearvr
