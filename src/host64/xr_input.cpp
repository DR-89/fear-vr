#include "xr_input.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace fearvr {
namespace {

void CopyName(char* target, std::size_t targetSize,
              const char* source) {
    if (strcpy_s(target, targetSize, source) != 0) {
        throw std::runtime_error("OpenXR action name is too long.");
    }
}

float ClampUnit(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::clamp(value, 0.0F, 1.0F);
}

float ClampAxis(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::clamp(value, -1.0F, 1.0F);
}

} // namespace

XrInput::XrInput(XrInputLogFunction log) : log_(std::move(log)) {}

XrInput::~XrInput() {
    Destroy();
}

void XrInput::Initialize(XrInstance instance) {
    if (instance == XR_NULL_HANDLE || actionSet_ != XR_NULL_HANDLE) {
        throw std::runtime_error("Invalid OpenXR input initialization.");
    }
    instance_ = instance;
    if (XR_FAILED(xrStringToPath(
            instance_, "/user/hand/left",
            &handPath_[FEARVR_HAND_LEFT])) ||
        XR_FAILED(xrStringToPath(
            instance_, "/user/hand/right",
            &handPath_[FEARVR_HAND_RIGHT]))) {
        throw std::runtime_error(
            "OpenXR controller hand paths could not be created.");
    }

    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    CopyName(setInfo.actionSetName,
             sizeof(setInfo.actionSetName), "gameplay");
    CopyName(setInfo.localizedActionSetName,
             sizeof(setInfo.localizedActionSetName), "F.E.A.R. VR");
    setInfo.priority = 0;
    const XrResult setResult =
        xrCreateActionSet(instance_, &setInfo, &actionSet_);
    if (XR_FAILED(setResult)) {
        throw std::runtime_error(
            "xrCreateActionSet(gameplay) failed: " +
            std::to_string(static_cast<std::int32_t>(setResult)));
    }

    CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "move", "Move", 1,
                 &handPath_[FEARVR_HAND_LEFT], moveAction_);
    CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "turn", "Turn", 1,
                 &handPath_[FEARVR_HAND_RIGHT], turnAction_);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger",
                 FEARVR_HAND_COUNT, handPath_, triggerAction_);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "squeeze", "Grip",
                 FEARVR_HAND_COUNT, handPath_, squeezeAction_);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "primary", "Primary Button",
                 FEARVR_HAND_COUNT, handPath_, primaryAction_);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary",
                 "Secondary Button", FEARVR_HAND_COUNT, handPath_,
                 secondaryAction_);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu",
                 FEARVR_HAND_COUNT, handPath_, menuAction_);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", "Stick Click",
                  FEARVR_HAND_COUNT, handPath_, stickClickAction_);
    CreateAction(XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Aim Pose",
                 FEARVR_HAND_COUNT, handPath_, aimPoseAction_);
    CreateAction(XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Grip Pose",
                 FEARVR_HAND_COUNT, handPath_, gripPoseAction_);
    CreateAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Haptic",
                  FEARVR_HAND_COUNT, handPath_, hapticAction_);

    const auto path = [this](const char* text) {
        XrPath value = XR_NULL_PATH;
        if (XR_FAILED(xrStringToPath(instance_, text, &value))) {
            throw std::runtime_error(
                std::string("OpenXR binding path failed: ") + text);
        }
        return value;
    };
    const auto bind = [](XrAction action, XrPath binding) {
        return XrActionSuggestedBinding{action, binding};
    };

    const std::array<XrActionSuggestedBinding, 19> touch{{
        bind(moveAction_, path("/user/hand/left/input/thumbstick")),
        bind(turnAction_, path("/user/hand/right/input/thumbstick")),
        bind(triggerAction_, path("/user/hand/left/input/trigger/value")),
        bind(triggerAction_, path("/user/hand/right/input/trigger/value")),
        bind(squeezeAction_, path("/user/hand/left/input/squeeze/value")),
        bind(squeezeAction_, path("/user/hand/right/input/squeeze/value")),
        bind(primaryAction_, path("/user/hand/left/input/x/click")),
        bind(primaryAction_, path("/user/hand/right/input/a/click")),
        bind(secondaryAction_, path("/user/hand/left/input/y/click")),
        bind(secondaryAction_, path("/user/hand/right/input/b/click")),
        bind(menuAction_, path("/user/hand/left/input/menu/click")),
        bind(stickClickAction_,
             path("/user/hand/left/input/thumbstick/click")),
        bind(stickClickAction_,
             path("/user/hand/right/input/thumbstick/click")),
        bind(aimPoseAction_, path("/user/hand/left/input/aim/pose")),
        bind(aimPoseAction_, path("/user/hand/right/input/aim/pose")),
        bind(gripPoseAction_, path("/user/hand/left/input/grip/pose")),
        bind(gripPoseAction_, path("/user/hand/right/input/grip/pose")),
        bind(hapticAction_, path("/user/hand/left/output/haptic")),
        bind(hapticAction_, path("/user/hand/right/output/haptic")),
    }};
    SuggestBindings("/interaction_profiles/oculus/touch_controller",
                    touch.data(),
                    static_cast<std::uint32_t>(touch.size()));

    const std::array<XrActionSuggestedBinding, 18> index{{
        bind(moveAction_, path("/user/hand/left/input/thumbstick")),
        bind(turnAction_, path("/user/hand/right/input/thumbstick")),
        bind(triggerAction_, path("/user/hand/left/input/trigger/value")),
        bind(triggerAction_, path("/user/hand/right/input/trigger/value")),
        bind(squeezeAction_, path("/user/hand/left/input/squeeze/value")),
        bind(squeezeAction_, path("/user/hand/right/input/squeeze/value")),
        bind(primaryAction_, path("/user/hand/left/input/a/click")),
        bind(primaryAction_, path("/user/hand/right/input/a/click")),
        bind(secondaryAction_, path("/user/hand/left/input/b/click")),
        bind(secondaryAction_, path("/user/hand/right/input/b/click")),
        bind(stickClickAction_,
             path("/user/hand/left/input/thumbstick/click")),
        bind(stickClickAction_,
             path("/user/hand/right/input/thumbstick/click")),
        bind(aimPoseAction_, path("/user/hand/left/input/aim/pose")),
        bind(aimPoseAction_, path("/user/hand/right/input/aim/pose")),
        bind(gripPoseAction_, path("/user/hand/left/input/grip/pose")),
        bind(gripPoseAction_, path("/user/hand/right/input/grip/pose")),
        bind(hapticAction_, path("/user/hand/left/output/haptic")),
        bind(hapticAction_, path("/user/hand/right/output/haptic")),
    }};
    SuggestBindings("/interaction_profiles/valve/index_controller",
                    index.data(),
                    static_cast<std::uint32_t>(index.size()));

    const std::array<XrActionSuggestedBinding, 15> motionController{{
        bind(moveAction_, path("/user/hand/left/input/thumbstick")),
        bind(turnAction_, path("/user/hand/right/input/thumbstick")),
        bind(triggerAction_, path("/user/hand/left/input/trigger/value")),
        bind(triggerAction_, path("/user/hand/right/input/trigger/value")),
        bind(primaryAction_, path("/user/hand/left/input/trackpad/click")),
        bind(primaryAction_, path("/user/hand/right/input/trackpad/click")),
        bind(menuAction_, path("/user/hand/left/input/menu/click")),
        bind(stickClickAction_,
             path("/user/hand/left/input/thumbstick/click")),
        bind(stickClickAction_,
             path("/user/hand/right/input/thumbstick/click")),
        bind(aimPoseAction_, path("/user/hand/left/input/aim/pose")),
        bind(aimPoseAction_, path("/user/hand/right/input/aim/pose")),
        bind(gripPoseAction_, path("/user/hand/left/input/grip/pose")),
        bind(gripPoseAction_, path("/user/hand/right/input/grip/pose")),
        bind(hapticAction_, path("/user/hand/left/output/haptic")),
        bind(hapticAction_, path("/user/hand/right/output/haptic")),
    }};
    SuggestBindings(
        "/interaction_profiles/microsoft/motion_controller",
        motionController.data(),
        static_cast<std::uint32_t>(motionController.size()));

    const std::array<XrActionSuggestedBinding, 13> vive{{
        bind(moveAction_, path("/user/hand/left/input/trackpad")),
        bind(turnAction_, path("/user/hand/right/input/trackpad")),
        bind(triggerAction_, path("/user/hand/left/input/trigger/value")),
        bind(triggerAction_, path("/user/hand/right/input/trigger/value")),
        bind(primaryAction_, path("/user/hand/left/input/trackpad/click")),
        bind(primaryAction_, path("/user/hand/right/input/trackpad/click")),
        bind(menuAction_, path("/user/hand/left/input/menu/click")),
        bind(aimPoseAction_, path("/user/hand/left/input/aim/pose")),
        bind(aimPoseAction_, path("/user/hand/right/input/aim/pose")),
        bind(gripPoseAction_, path("/user/hand/left/input/grip/pose")),
        bind(gripPoseAction_, path("/user/hand/right/input/grip/pose")),
        bind(hapticAction_, path("/user/hand/left/output/haptic")),
        bind(hapticAction_, path("/user/hand/right/output/haptic")),
    }};
    SuggestBindings("/interaction_profiles/htc/vive_controller",
                    vive.data(),
                    static_cast<std::uint32_t>(vive.size()));

    const std::array<XrActionSuggestedBinding, 10> simple{{
        bind(primaryAction_, path("/user/hand/left/input/select/click")),
        bind(primaryAction_, path("/user/hand/right/input/select/click")),
        bind(menuAction_, path("/user/hand/left/input/menu/click")),
        bind(menuAction_, path("/user/hand/right/input/menu/click")),
        bind(aimPoseAction_, path("/user/hand/left/input/aim/pose")),
        bind(aimPoseAction_, path("/user/hand/right/input/aim/pose")),
        bind(gripPoseAction_, path("/user/hand/left/input/grip/pose")),
        bind(gripPoseAction_, path("/user/hand/right/input/grip/pose")),
        bind(hapticAction_, path("/user/hand/left/output/haptic")),
        bind(hapticAction_, path("/user/hand/right/output/haptic")),
    }};
    SuggestBindings("/interaction_profiles/khr/simple_controller",
                    simple.data(),
                    static_cast<std::uint32_t>(simple.size()));

    log_("INFO", "input_actions_created",
         "Gameplay action set and controller bindings are ready.");
}

void XrInput::CreateAction(
    XrActionType type, const char* name, const char* localizedName,
    std::uint32_t subactionCount, const XrPath* subactions,
    XrAction& action) {
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    CopyName(info.actionName, sizeof(info.actionName), name);
    CopyName(info.localizedActionName,
             sizeof(info.localizedActionName), localizedName);
    info.countSubactionPaths = subactionCount;
    info.subactionPaths = subactions;
    const XrResult result = xrCreateAction(actionSet_, &info, &action);
    if (XR_FAILED(result)) {
        throw std::runtime_error(
            std::string("xrCreateAction(") + name + ") failed: " +
            std::to_string(static_cast<std::int32_t>(result)));
    }
}

void XrInput::SuggestBindings(
    const char* profile, const XrActionSuggestedBinding* bindings,
    std::uint32_t bindingCount) noexcept {
    XrPath profilePath = XR_NULL_PATH;
    XrResult result =
        xrStringToPath(instance_, profile, &profilePath);
    if (XR_SUCCEEDED(result)) {
        XrInteractionProfileSuggestedBinding suggestion{
            XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestion.interactionProfile = profilePath;
        suggestion.countSuggestedBindings = bindingCount;
        suggestion.suggestedBindings = bindings;
        result = xrSuggestInteractionProfileBindings(
            instance_, &suggestion);
    }
    log_(XR_SUCCEEDED(result) ? "INFO" : "WARN",
         XR_SUCCEEDED(result) ? "input_profile_bound"
                              : "input_profile_skipped",
         std::string(profile) + " result=" +
             std::to_string(static_cast<std::int32_t>(result)));
}

void XrInput::Attach(XrSession session) {
    if (session == XR_NULL_HANDLE ||
        actionSet_ == XR_NULL_HANDLE) {
        throw std::runtime_error("OpenXR input cannot attach to session.");
    }
    const XrActionSet sets[]{actionSet_};
    XrSessionActionSetsAttachInfo attachInfo{
        XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = sets;
    const XrResult result =
        xrAttachSessionActionSets(session, &attachInfo);
    if (XR_FAILED(result)) {
        throw std::runtime_error(
            "xrAttachSessionActionSets failed: " +
            std::to_string(static_cast<std::int32_t>(result)));
    }
    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        XrActionSpaceCreateInfo spaceInfo{
            XR_TYPE_ACTION_SPACE_CREATE_INFO};
        spaceInfo.action = aimPoseAction_;
        spaceInfo.subactionPath = handPath_[hand];
        spaceInfo.poseInActionSpace.orientation.w = 1.0F;
        const XrResult spaceResult =
            xrCreateActionSpace(session, &spaceInfo, &aimSpace_[hand]);
        if (XR_FAILED(spaceResult)) {
            ResetSession();
            throw std::runtime_error(
                "xrCreateActionSpace(aim_pose) failed: " +
                std::to_string(static_cast<std::int32_t>(spaceResult)));
        }
        spaceInfo.action = gripPoseAction_;
        const XrResult gripSpaceResult =
            xrCreateActionSpace(session, &spaceInfo, &gripSpace_[hand]);
        if (XR_FAILED(gripSpaceResult)) {
            ResetSession();
            throw std::runtime_error(
                "xrCreateActionSpace(grip_pose) failed: " +
                std::to_string(
                    static_cast<std::int32_t>(gripSpaceResult)));
        }
    }
    attached_ = true;
    syncFailureLogged_ = false;
    log_("INFO", "input_actions_attached",
         "Gameplay action set attached to OpenXR session.");
}

bool XrInput::ReadVector2(
    XrSession session, XrAction action, XrPath hand,
    float& x, float& y) noexcept {
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = hand;
    XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
    const XrResult result =
        xrGetActionStateVector2f(session, &info, &state);
    if (XR_FAILED(result) || state.isActive != XR_TRUE) {
        x = 0.0F;
        y = 0.0F;
        return false;
    }
    x = ClampAxis(state.currentState.x);
    y = ClampAxis(state.currentState.y);
    return true;
}

bool XrInput::ReadFloat(
    XrSession session, XrAction action, XrPath hand,
    float& value) noexcept {
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = hand;
    XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
    const XrResult result =
        xrGetActionStateFloat(session, &info, &state);
    if (XR_FAILED(result) || state.isActive != XR_TRUE) {
        value = 0.0F;
        return false;
    }
    value = ClampUnit(state.currentState);
    return true;
}

bool XrInput::ReadBoolean(
    XrSession session, XrAction action, XrPath hand,
    bool& value) noexcept {
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = hand;
    XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
    const XrResult result =
        xrGetActionStateBoolean(session, &info, &state);
    if (XR_FAILED(result) || state.isActive != XR_TRUE) {
        value = false;
        return false;
    }
    value = state.currentState == XR_TRUE;
    return true;
}

bool XrInput::ReadPose(
    XrSession session, XrAction action, XrPath hand,
    XrSpace actionSpace, XrSpace baseSpace, XrTime displayTime,
    FearVrPose& pose) noexcept {
    pose = {};
    if (actionSpace == XR_NULL_HANDLE ||
        baseSpace == XR_NULL_HANDLE || displayTime <= 0) {
        return false;
    }
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = hand;
    XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
    if (XR_FAILED(xrGetActionStatePose(session, &info, &state)) ||
        state.isActive != XR_TRUE) {
        return false;
    }

    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(
            actionSpace, baseSpace, displayTime, &location))) {
        return false;
    }
    constexpr XrSpaceLocationFlags required =
        XR_SPACE_LOCATION_POSITION_VALID_BIT |
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
        XR_SPACE_LOCATION_POSITION_TRACKED_BIT |
        XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    if ((location.locationFlags & required) != required) {
        return false;
    }
    pose.px = location.pose.position.x;
    pose.py = location.pose.position.y;
    pose.pz = location.pose.position.z;
    pose.qx = location.pose.orientation.x;
    pose.qy = location.pose.orientation.y;
    pose.qz = location.pose.orientation.z;
    pose.qw = location.pose.orientation.w;
    const float magnitudeSquared =
        pose.qx * pose.qx + pose.qy * pose.qy +
        pose.qz * pose.qz + pose.qw * pose.qw;
    return std::isfinite(pose.px) && std::isfinite(pose.py) &&
           std::isfinite(pose.pz) && std::isfinite(magnitudeSquared) &&
           magnitudeSquared >= 0.25F && magnitudeSquared <= 4.0F;
}

bool XrInput::Sync(
    XrSession session, XrSpace baseSpace, bool focused,
    XrTime predictedDisplayTime,
    FearVrInputState& output) noexcept {
    output = {};
    output.sampleId = ++sampleId_;
    output.predictedDisplayTimeNs =
        static_cast<std::uint64_t>(predictedDisplayTime);
    output.flags = FEARVR_IF_VALID;
    if (!attached_ || session == XR_NULL_HANDLE || !focused) {
        LogStateChanges(output);
        return true;
    }

    XrActiveActionSet activeSet{};
    activeSet.actionSet = actionSet_;
    activeSet.subactionPath = XR_NULL_PATH;
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeSet;
    const XrResult syncResult =
        xrSyncActions(session, &syncInfo);
    if (XR_FAILED(syncResult)) {
        if (!syncFailureLogged_) {
            log_("WARN", "input_sync_failed",
                 "result=" +
                     std::to_string(
                         static_cast<std::int32_t>(syncResult)) +
                     "; neutral state published.");
            syncFailureLogged_ = true;
        }
        LogStateChanges(output);
        return false;
    }
    syncFailureLogged_ = false;
    output.flags |= FEARVR_IF_FOCUSED;

    if (ReadVector2(session, moveAction_,
                    handPath_[FEARVR_HAND_LEFT],
                    output.moveX, output.moveY)) {
        output.activeHands |= FEARVR_HAND_MASK_LEFT;
    }
    if (ReadVector2(session, turnAction_,
                    handPath_[FEARVR_HAND_RIGHT],
                    output.turnX, output.turnY)) {
        output.activeHands |= FEARVR_HAND_MASK_RIGHT;
    }

    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        const std::uint32_t handMask =
            hand == FEARVR_HAND_LEFT
                ? FEARVR_HAND_MASK_LEFT
                : FEARVR_HAND_MASK_RIGHT;
        if (ReadFloat(session, triggerAction_, handPath_[hand],
                      output.trigger[hand])) {
            output.activeHands |= handMask;
        }
        if (ReadFloat(session, squeezeAction_, handPath_[hand],
                      output.squeeze[hand])) {
            output.activeHands |= handMask;
        }

        bool value = false;
        if (ReadBoolean(session, primaryAction_, handPath_[hand], value)) {
            output.activeHands |= handMask;
            if (value) {
                output.buttons |=
                    hand == FEARVR_HAND_LEFT
                        ? FEARVR_IB_LEFT_PRIMARY
                        : FEARVR_IB_RIGHT_PRIMARY;
            }
        }
        if (ReadBoolean(session, secondaryAction_, handPath_[hand], value)) {
            output.activeHands |= handMask;
            if (value) {
                output.buttons |=
                    hand == FEARVR_HAND_LEFT
                        ? FEARVR_IB_LEFT_SECONDARY
                        : FEARVR_IB_RIGHT_SECONDARY;
            }
        }
        if (ReadBoolean(session, menuAction_, handPath_[hand], value)) {
            output.activeHands |= handMask;
            if (value) {
                output.buttons |=
                    hand == FEARVR_HAND_LEFT
                        ? FEARVR_IB_LEFT_MENU
                        : FEARVR_IB_RIGHT_MENU;
            }
        }
        if (ReadBoolean(
                session, stickClickAction_, handPath_[hand], value)) {
            output.activeHands |= handMask;
            if (value) {
                output.buttons |=
                    hand == FEARVR_HAND_LEFT
                        ? FEARVR_IB_LEFT_STICK
                        : FEARVR_IB_RIGHT_STICK;
            }
        }
        if (ReadPose(
                session, aimPoseAction_, handPath_[hand],
                aimSpace_[hand], baseSpace, predictedDisplayTime,
                output.handAimPose[hand])) {
            output.aimPoseValidHands |= handMask;
        }
        if (ReadPose(
                session, gripPoseAction_, handPath_[hand],
                gripSpace_[hand], baseSpace, predictedDisplayTime,
                output.handGripPose[hand])) {
            output.gripPoseValidHands |= handMask;
        }
    }

    LogStateChanges(output);
    return true;
}

void XrInput::LogStateChanges(
    const FearVrInputState& state) noexcept {
    if (state.activeHands != lastActiveHands_) {
        std::ostringstream message;
        message << "active_hands=0x" << std::hex << state.activeHands;
        log_("INFO", "input_devices_changed", message.str());
        lastActiveHands_ = state.activeHands;
    }
    if (state.buttons != lastButtons_) {
        std::ostringstream message;
        message << "buttons=0x" << std::hex << state.buttons;
        log_("INFO", "input_buttons_changed", message.str());
        lastButtons_ = state.buttons;
    }
    if (state.aimPoseValidHands != lastAimPoseValidHands_) {
        std::ostringstream message;
        message << "aim_pose_hands=0x" << std::hex
                << state.aimPoseValidHands;
        log_("INFO", "input_aim_pose_changed", message.str());
        lastAimPoseValidHands_ = state.aimPoseValidHands;
    }
    if (state.gripPoseValidHands != lastGripPoseValidHands_) {
        std::ostringstream message;
        message << "grip_pose_hands=0x" << std::hex
                << state.gripPoseValidHands;
        log_("INFO", "input_grip_pose_changed", message.str());
        lastGripPoseValidHands_ = state.gripPoseValidHands;
    }

    const bool analogActive =
        std::fabs(state.moveX) > 0.1F ||
        std::fabs(state.moveY) > 0.1F ||
        std::fabs(state.turnX) > 0.1F ||
        std::fabs(state.turnY) > 0.1F ||
        state.trigger[0] > 0.1F || state.trigger[1] > 0.1F ||
        state.squeeze[0] > 0.1F || state.squeeze[1] > 0.1F;
    if (analogActive && ++activeSampleLogCounter_ >= 180) {
        activeSampleLogCounter_ = 0;
        std::ostringstream message;
        message << std::fixed << std::setprecision(2)
                << "move=" << state.moveX << ',' << state.moveY
                << " turn=" << state.turnX << ',' << state.turnY
                << " trigger=" << state.trigger[0] << ','
                << state.trigger[1] << " squeeze="
                << state.squeeze[0] << ',' << state.squeeze[1];
        log_("INFO", "input_sample", message.str());
    } else if (!analogActive) {
        activeSampleLogCounter_ = 0;
    }
}

void XrInput::ApplyHaptic(
    XrSession session,
    const FearVrHapticRequest& request) noexcept {
    if (!attached_ || session == XR_NULL_HANDLE ||
        (request.flags & FEARVR_HF_VALID) == 0 ||
        request.requestId == 0) {
        return;
    }
    const XrDuration minimum = 1'000'000;
    const XrDuration maximum = 5'000'000'000;
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = std::clamp(
        static_cast<XrDuration>(request.durationNs),
        minimum, maximum);
    vibration.frequency =
        std::isfinite(request.frequency) &&
                request.frequency > 0.0F
            ? request.frequency
            : XR_FREQUENCY_UNSPECIFIED;
    vibration.amplitude = ClampUnit(request.amplitude);

    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        const std::uint32_t mask =
            hand == FEARVR_HAND_LEFT
                ? FEARVR_HAND_MASK_LEFT
                : FEARVR_HAND_MASK_RIGHT;
        if ((request.handMask & mask) == 0) {
            continue;
        }
        XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
        info.action = hapticAction_;
        info.subactionPath = handPath_[hand];
        const XrResult result = xrApplyHapticFeedback(
            session, &info,
            reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
        if (XR_FAILED(result)) {
            log_("WARN", "haptic_apply_failed",
                 "hand=" + std::to_string(hand) + " result=" +
                     std::to_string(static_cast<std::int32_t>(result)));
        }
    }
}

void XrInput::ResetSession() noexcept {
    for (XrSpace& space : aimSpace_) {
        if (space != XR_NULL_HANDLE) {
            xrDestroySpace(space);
            space = XR_NULL_HANDLE;
        }
    }
    for (XrSpace& space : gripSpace_) {
        if (space != XR_NULL_HANDLE) {
            xrDestroySpace(space);
            space = XR_NULL_HANDLE;
        }
    }
    attached_ = false;
    lastActiveHands_ = 0;
    lastButtons_ = 0;
    lastAimPoseValidHands_ = 0;
    lastGripPoseValidHands_ = 0;
    activeSampleLogCounter_ = 0;
}

void XrInput::Destroy() noexcept {
    ResetSession();
    const std::array<XrAction*, 11> actions{{
        &moveAction_, &turnAction_, &triggerAction_, &squeezeAction_,
        &primaryAction_, &secondaryAction_, &menuAction_,
        &stickClickAction_, &aimPoseAction_, &gripPoseAction_,
        &hapticAction_,
    }};
    for (XrAction* action : actions) {
        if (*action != XR_NULL_HANDLE) {
            xrDestroyAction(*action);
            *action = XR_NULL_HANDLE;
        }
    }
    if (actionSet_ != XR_NULL_HANDLE) {
        xrDestroyActionSet(actionSet_);
        actionSet_ = XR_NULL_HANDLE;
    }
    instance_ = XR_NULL_HANDLE;
}

} // namespace fearvr
