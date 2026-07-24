#include <cstdio>

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

    if (failed == 0) {
        std::printf("test_xr_session_state: OK\n");
        return 0;
    }
    std::printf("test_xr_session_state: %d Pruefung(en) fehlgeschlagen\n",
                failed);
    return 1;
}
