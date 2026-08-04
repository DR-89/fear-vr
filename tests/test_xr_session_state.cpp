#include <cstdio>

#include "reference_space_recenter.h"
#include "xr_session_state.h"

namespace {

int failed = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("FAIL: %s (line %d)\n", #condition, __LINE__);          \
            ++failed;                                                           \
        }                                                                       \
    } while (false)

} // namespace

int main() {
    using fearvr::XrLifecycleAction;
    using fearvr::XrLifecycleState;
    using fearvr::XrSessionStateMachine;

    XrSessionStateMachine state;
    CHECK(!state.IsSessionRunning());

    auto transition = state.OnStateChanged(XrLifecycleState::Idle);
    CHECK(transition.action == XrLifecycleAction::None);
    CHECK(!transition.sessionRunning);

    transition = state.OnStateChanged(XrLifecycleState::Ready);
    CHECK(transition.action == XrLifecycleAction::BeginSession);
    CHECK(transition.sessionRunning);

    transition = state.OnStateChanged(XrLifecycleState::Ready);
    CHECK(transition.action == XrLifecycleAction::None);
    CHECK(transition.sessionRunning);

    transition = state.OnStateChanged(XrLifecycleState::Focused);
    CHECK(transition.action == XrLifecycleAction::None);
    CHECK(transition.sessionRunning);

    transition = state.OnStateChanged(XrLifecycleState::Stopping);
    CHECK(transition.action == XrLifecycleAction::EndSession);
    CHECK(!transition.sessionRunning);

    transition = state.OnStateChanged(XrLifecycleState::Ready);
    CHECK(transition.action == XrLifecycleAction::BeginSession);
    CHECK(transition.sessionRunning);

    transition = state.OnStateChanged(XrLifecycleState::LossPending);
    CHECK(transition.action == XrLifecycleAction::RestartSession);
    CHECK(!transition.sessionRunning);

    transition = state.OnStateChanged(XrLifecycleState::Exiting);
    CHECK(transition.action == XrLifecycleAction::ExitHost);
    CHECK(!transition.sessionRunning);

    fearvr::ReferenceSpaceRecenterState origin;
    CHECK(!fearvr::CommitReferenceSpaceRecenterIfReady(
        origin, 100, true));
    fearvr::ScheduleReferenceSpaceRecenter(origin, 500);
    CHECK(origin.pending);
    CHECK(!fearvr::CommitReferenceSpaceRecenterIfReady(
        origin, 499, true));
    CHECK(!fearvr::CommitReferenceSpaceRecenterIfReady(
        origin, 500, false));
    CHECK(fearvr::CommitReferenceSpaceRecenterIfReady(
        origin, 500, true));
    CHECK(!origin.pending);
    CHECK(origin.generation == 1);

    // Several pending notifications collapse to the final future change.  A
    // pose between them must not become the new body anchor.
    fearvr::ScheduleReferenceSpaceRecenter(origin, 700);
    fearvr::ScheduleReferenceSpaceRecenter(origin, 900);
    CHECK(!fearvr::CommitReferenceSpaceRecenterIfReady(
        origin, 700, true));
    CHECK(fearvr::CommitReferenceSpaceRecenterIfReady(
        origin, 900, true));
    CHECK(origin.generation == 2);

    origin.generation = UINT32_MAX;
    fearvr::ScheduleReferenceSpaceRecenter(origin, 0);
    CHECK(fearvr::CommitReferenceSpaceRecenterIfReady(
        origin, 0, true));
    CHECK(origin.generation == 1);

    if (failed == 0) {
        std::printf("test_xr_session_state: OK\n");
        return 0;
    }
    std::printf("test_xr_session_state: %d Pruefung(en) fehlgeschlagen\n",
                failed);
    return 1;
}
