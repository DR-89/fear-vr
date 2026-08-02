#include <array>
#include <cassert>
#include <limits>

#include "vr_menu_model.h"

int main() {
    using fearvr::VrSettingsPage;

    static_assert(!fearvr::IsVrSettingsPage(VrSettingsPage::None));
    static_assert(fearvr::IsVrSettingsPage(VrSettingsPage::Root));
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::Root) ==
        VrSettingsPage::None);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::Display) ==
        VrSettingsPage::Root);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::Comfort) ==
        VrSettingsPage::Root);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::Controls) ==
        VrSettingsPage::Root);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::Weapons) ==
        VrSettingsPage::Root);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::WeaponHandling) ==
        VrSettingsPage::Weapons);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::WeaponWeight) ==
        VrSettingsPage::Weapons);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::WeaponRecoil) ==
        VrSettingsPage::Weapons);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::Melee) ==
        VrSettingsPage::Root);
    static_assert(
        fearvr::ParentVrSettingsPage(VrSettingsPage::Advanced) ==
        VrSettingsPage::Root);

    constexpr std::array<int, 4> integerPresets{100, 150, 200, 250};
    assert(fearvr::ClosestVrPresetIndex(100, integerPresets) == 0);
    assert(fearvr::ClosestVrPresetIndex(174, integerPresets) == 1);
    assert(fearvr::ClosestVrPresetIndex(176, integerPresets) == 2);
    assert(fearvr::NextVrPresetIndex(200, integerPresets) == 3);
    assert(fearvr::NextVrPresetIndex(250, integerPresets) == 0);

    constexpr std::array<float, 4> floatPresets{
        0.5F, 1.0F, 1.5F, 2.0F};
    assert(fearvr::ClosestVrPresetIndex(1.49F, floatPresets) == 2);
    assert(fearvr::NextVrPresetIndex(1.49F, floatPresets) == 3);
    assert(fearvr::ClosestVrPresetIndex(
               std::numeric_limits<float>::quiet_NaN(), floatPresets) == 0);

    return 0;
}
