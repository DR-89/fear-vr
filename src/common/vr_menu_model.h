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

// Fine live-tuning controls are more useful as uniform steps than as a long
// preset table. Snap the current value to the nearest step, advance once and
// wrap from the inclusive maximum back to the minimum.
inline float NextVrSteppedValue(
    float value, float minimum, float maximum, float step) noexcept {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
        !std::isfinite(step) || minimum > maximum || step <= 0.0F) {
        return value;
    }
    if (!std::isfinite(value)) {
        return minimum;
    }
    const float clamped = (std::max)(minimum, (std::min)(maximum, value));
    const float stepIndex = std::round((clamped - minimum) / step);
    const float next = minimum + (stepIndex + 1.0F) * step;
    if (next > maximum + step * 0.25F) {
        return minimum;
    }
    return (std::max)(minimum, (std::min)(maximum, next));
}

} // namespace fearvr
