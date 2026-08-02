#include <cassert>
#include <cmath>

#include "dev_menu_model.h"

int main() {
    using namespace fearvr;

    constexpr wchar_t kMenuCharacters[] =
        L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:.-/%+";
    for (const wchar_t character : kMenuCharacters) {
        if (character != L'\0') {
            const auto* const glyph = DevMenuGlyphRows(character);
            assert(glyph != nullptr);
            bool hasPixel = false;
            for (const std::uint8_t row : *glyph) {
                hasPixel = hasPixel || row != 0;
                assert((row & 0xE0U) == 0);
            }
            assert(hasPixel);
        }
    }
    assert(DevMenuGlyphRows(L' ') == nullptr);
    assert(DevMenuGlyphRows(L'?') == nullptr);

    DevMenuPanelGeometry panel{};
    panel.center = {0.0F, 0.0F, -1.0F};
    panel.right = {1.0F, 0.0F, 0.0F};
    panel.up = {0.0F, 1.0F, 0.0F};
    panel.normal = {0.0F, 0.0F, -1.0F};
    panel.tabCount = 6;
    panel.rowCount = 10;

    DevMenuRayHit hit;
    assert(HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, hit));
    assert(std::fabs(hit.distance - 1.0F) < 1.0e-6F);
    assert(hit.region == DevMenuHitRegion::row);
    assert(hit.row == 3U);

    const float tabCenterY =
        panel.height * 0.5F - panel.titleHeight -
        panel.tabHeight * 0.5F;
    assert(HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, 0.0F},
        {0.0F, tabCenterY, -1.0F}, hit));
    assert(hit.region == DevMenuHitRegion::tab);
    assert(hit.tab == 3U);

    const float rowZeroY =
        panel.height * 0.5F - panel.headerHeight -
        panel.rowHeight * 0.5F;
    assert(HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, 0.0F},
        {0.0F, rowZeroY, -1.0F}, hit));
    assert(hit.region == DevMenuHitRegion::row);
    assert(hit.row == 0U);

    assert(!HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, hit));
    assert(!HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, -1.0F}, hit));
    assert(!HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, -2.0F}, {0.0F, 0.0F, -1.0F}, hit));

    panel.rowCount = 0;
    assert(!HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, hit));
    panel.rowCount = 10;
    panel.tabCount = 0;
    assert(!HitTestDevMenuPanel(
        panel, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, hit));
    return 0;
}
