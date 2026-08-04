#pragma once

#include <algorithm>
#include <cstddef>

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

} // namespace fearvr
