#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace fearvr {

struct DevMenuVector3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

inline DevMenuVector3 operator-(
    const DevMenuVector3& left,
    const DevMenuVector3& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline float DevMenuDot(
    const DevMenuVector3& left,
    const DevMenuVector3& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

struct DevMenuPanelGeometry {
    DevMenuVector3 center;
    DevMenuVector3 right;
    DevMenuVector3 up;
    DevMenuVector3 normal;
    float width{0.64F};
    float height{0.60F};
    float headerHeight{0.12F};
    float titleHeight{0.055F};
    float tabHeight{0.055F};
    float rowHeight{0.055F};
    std::size_t tabCount{0};
    std::size_t rowCount{0};
};

enum class DevMenuHitRegion {
    none,
    tab,
    row,
};

struct DevMenuRayHit {
    float distance{0.0F};
    float horizontal{0.0F};
    float vertical{0.0F};
    DevMenuHitRegion region{DevMenuHitRegion::none};
    std::size_t tab{0};
    std::size_t row{0};
};

inline const std::array<std::uint8_t, 7>* DevMenuGlyphRows(
    wchar_t character) noexcept {
    static constexpr wchar_t kCharacters[] =
        L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:.-/%+";
    static constexpr std::array<std::uint8_t, 7> kGlyphs[] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, // 3
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
        {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}, // 5
        {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}, // 9
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
        {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F}, // C
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
        {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F}, // G
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}, // I
        {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, // J
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, // S
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
        {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}, // V
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, // W
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
        {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}, // :
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}, // .
        {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, // -
        {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}, // /
        {0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13}, // %
        {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, // +
    };
    static_assert(
        sizeof(kCharacters) / sizeof(kCharacters[0]) - 1U ==
            sizeof(kGlyphs) / sizeof(kGlyphs[0]),
        "Developer-menu glyph table and character map differ.");
    for (std::size_t index = 0; kCharacters[index] != L'\0'; ++index) {
        if (kCharacters[index] == character) {
            return &kGlyphs[index];
        }
    }
    return nullptr;
}

inline bool HitTestDevMenuPanel(
    const DevMenuPanelGeometry& panel,
    const DevMenuVector3& rayOrigin,
    const DevMenuVector3& rayDirection,
    DevMenuRayHit& hit) noexcept {
    hit = {};
    if (panel.tabCount == 0 || panel.rowCount == 0 ||
        panel.width <= 0.0F ||
        panel.height <= 0.0F || panel.headerHeight < 0.0F ||
        panel.titleHeight < 0.0F || panel.tabHeight <= 0.0F ||
        panel.headerHeight < panel.titleHeight + panel.tabHeight ||
        panel.rowHeight <= 0.0F) {
        return false;
    }

    const float denominator = DevMenuDot(rayDirection, panel.normal);
    if (!std::isfinite(denominator) ||
        std::fabs(denominator) < 1.0e-5F) {
        return false;
    }
    const float distance =
        DevMenuDot(panel.center - rayOrigin, panel.normal) / denominator;
    if (!std::isfinite(distance) || distance <= 0.0F) {
        return false;
    }

    const DevMenuVector3 point{
        rayOrigin.x + rayDirection.x * distance,
        rayOrigin.y + rayDirection.y * distance,
        rayOrigin.z + rayDirection.z * distance};
    const DevMenuVector3 relative = point - panel.center;
    const float horizontal = DevMenuDot(relative, panel.right);
    const float vertical = DevMenuDot(relative, panel.up);
    if (!std::isfinite(horizontal) || !std::isfinite(vertical) ||
        std::fabs(horizontal) > panel.width * 0.5F ||
        std::fabs(vertical) > panel.height * 0.5F) {
        return false;
    }

    const float panelTop = panel.height * 0.5F;
    const float tabTop = panelTop - panel.titleHeight;
    const float tabBottom = tabTop - panel.tabHeight;
    if (vertical <= tabTop && vertical >= tabBottom) {
        const float normalized =
            (horizontal + panel.width * 0.5F) / panel.width;
        std::size_t tab = static_cast<std::size_t>(
            normalized * static_cast<float>(panel.tabCount));
        if (tab >= panel.tabCount) {
            tab = panel.tabCount - 1U;
        }
        hit.distance = distance;
        hit.horizontal = horizontal;
        hit.vertical = vertical;
        hit.region = DevMenuHitRegion::tab;
        hit.tab = tab;
        return true;
    }

    const float rowTop = panelTop - panel.headerHeight;
    const float rowPosition = (rowTop - vertical) / panel.rowHeight;
    if (!std::isfinite(rowPosition) || rowPosition < 0.0F ||
        rowPosition >= static_cast<float>(panel.rowCount)) {
        return false;
    }

    hit.distance = distance;
    hit.horizontal = horizontal;
    hit.vertical = vertical;
    hit.region = DevMenuHitRegion::row;
    hit.row = static_cast<std::size_t>(rowPosition);
    return true;
}

} // namespace fearvr
