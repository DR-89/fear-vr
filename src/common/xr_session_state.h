#pragma once

#include <cstdint>

namespace fearvr {

enum class XrLifecycleState : std::uint8_t {
    Unknown,
    Idle,
    Ready,
    Synchronized,
    Visible,
    Focused,
    Stopping,
    LossPending,
    Exiting,
};

enum class XrLifecycleAction : std::uint8_t {
    None,
    BeginSession,
    EndSession,
    RestartSession,
    ExitHost,
};

struct XrLifecycleTransition {
    XrLifecycleState previous{XrLifecycleState::Unknown};
    XrLifecycleState current{XrLifecycleState::Unknown};
    XrLifecycleAction action{XrLifecycleAction::None};
    bool sessionRunning{false};
};

class XrSessionStateMachine {
public:
    [[nodiscard]] XrLifecycleTransition OnStateChanged(
        XrLifecycleState state) noexcept {
        const XrLifecycleState previous = state_;
        XrLifecycleAction action = XrLifecycleAction::None;

        switch (state) {
        case XrLifecycleState::Ready:
            if (!sessionRunning_) {
                sessionRunning_ = true;
                action = XrLifecycleAction::BeginSession;
            }
            break;
        case XrLifecycleState::Stopping:
            if (sessionRunning_) {
                sessionRunning_ = false;
                action = XrLifecycleAction::EndSession;
            }
            break;
        case XrLifecycleState::LossPending:
            sessionRunning_ = false;
            action = XrLifecycleAction::RestartSession;
            break;
        case XrLifecycleState::Exiting:
            sessionRunning_ = false;
            action = XrLifecycleAction::ExitHost;
            break;
        default:
            break;
        }

        state_ = state;
        return {previous, state_, action, sessionRunning_};
    }

    [[nodiscard]] XrLifecycleState State() const noexcept { return state_; }
    [[nodiscard]] bool IsSessionRunning() const noexcept {
        return sessionRunning_;
    }

private:
    XrLifecycleState state_{XrLifecycleState::Unknown};
    bool sessionRunning_{false};
};

} // namespace fearvr
