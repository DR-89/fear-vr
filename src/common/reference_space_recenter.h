#pragma once

#include <cstdint>

namespace fearvr {

// OpenXR may replace the origin of a LOCAL reference space while an
// application is already running.  The change is announced for a future
// display time.  Keep rendering against the old origin until that time, then
// recenter exactly on the first fully tracked pose in the new origin.
struct ReferenceSpaceRecenterState {
    std::int64_t pendingChangeTime{0};
    std::uint32_t generation{0};
    bool pending{false};
};

inline void ScheduleReferenceSpaceRecenter(
    ReferenceSpaceRecenterState& state,
    std::int64_t changeTime) noexcept {
    if (!state.pending || changeTime > state.pendingChangeTime) {
        state.pendingChangeTime = changeTime;
    }
    state.pending = true;
}

inline bool CommitReferenceSpaceRecenterIfReady(
    ReferenceSpaceRecenterState& state,
    std::int64_t predictedDisplayTime,
    bool fullyTracked) noexcept {
    if (!state.pending || !fullyTracked ||
        (state.pendingChangeTime > 0 &&
         predictedDisplayTime < state.pendingChangeTime)) {
        return false;
    }

    state.pending = false;
    state.pendingChangeTime = 0;
    ++state.generation;
    if (state.generation == 0) {
        ++state.generation;
    }
    return true;
}

} // namespace fearvr
