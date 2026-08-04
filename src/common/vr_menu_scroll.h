#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace fearvr {

// Returns the first visible row for a fixed-height menu viewport while keeping
// the selected row on screen. Rows are logical visible controls; hidden
// sibling controls in the Retail list must not participate in this calculation.
inline std::size_t VrMenuScrollStart(
    std::size_t currentStart,
    std::size_t selectedRow,
    std::size_t visibleRows,
    std::size_t pageRows) noexcept {
    if (visibleRows == 0 || pageRows == 0 || visibleRows <= pageRows) {
        return 0;
    }

    selectedRow = std::min(selectedRow, visibleRows - 1);
    const std::size_t lastStart = visibleRows - pageRows;
    currentStart = std::min(currentStart, lastStart);

    if (selectedRow < currentStart) {
        return selectedRow;
    }
    if (selectedRow >= currentStart + pageRows) {
        return std::min(selectedRow - pageRows + 1, lastStart);
    }
    return currentStart;
}

// Converts Retail's physical last-shown control index into the number of
// logical VR rows that actually fit in the current menu frame. The system
// menu resizes its list to the currently visible stock controls on focus, so
// its capacity is not a fixed number of rows. Hidden toggle siblings leave
// gaps in the physical indices and therefore cannot be counted directly.
inline std::size_t VrMenuVisibleRowCapacity(
    const std::uint32_t* controlIndices,
    std::size_t visibleRows,
    std::size_t firstRow,
    std::uint32_t lastShownControl) noexcept {
    if (controlIndices == nullptr || visibleRows == 0 ||
        firstRow >= visibleRows) {
        return 0;
    }

    std::size_t rows = 0;
    for (std::size_t row = firstRow; row < visibleRows; ++row) {
        if (controlIndices[row] > lastShownControl) {
            break;
        }
        ++rows;
    }
    return rows;
}

inline bool VrMenuControlOrderIsStrictlyIncreasing(
    const std::uint32_t* controlIndices,
    std::size_t visibleRows) noexcept {
    if (controlIndices == nullptr) {
        return false;
    }
    for (std::size_t row = 1; row < visibleRows; ++row) {
        if (controlIndices[row] <= controlIndices[row - 1]) {
            return false;
        }
    }
    return true;
}

// Retail sizes the list and its translucent parent panel separately. In the
// observed pause-menu layout, the final row accepted by m_nLastShown can sit
// below the visible parent panel. Keep one measured row as a visual margin.
inline std::size_t VrMenuSafePageRows(
    std::size_t retailVisibleRows) noexcept {
    return retailVisibleRows > 1
        ? retailVisibleRows - 1
        : retailVisibleRows;
}

} // namespace fearvr
