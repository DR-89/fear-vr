#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <openxr/openxr.h>

#include "protocol.h"

namespace fearvr {

using XrInputLogFunction =
    std::function<void(const char* level, const char* event,
                       const std::string& message)>;

class XrInput {
public:
    explicit XrInput(XrInputLogFunction log);
    ~XrInput();

    XrInput(const XrInput&) = delete;
    XrInput& operator=(const XrInput&) = delete;

    void Initialize(XrInstance instance,
                    bool genericControllerEnabled);
    void Attach(XrSession session);
    void MarkInteractionProfileChanged() noexcept;
    void LogInteractionProfiles(XrSession session) noexcept;
    void ResetSession() noexcept;

    bool Sync(XrSession session, XrSpace baseSpace, bool focused,
              XrTime predictedDisplayTime,
              FearVrInputState& state) noexcept;
    void ApplyHaptic(XrSession session,
                     const FearVrHapticRequest& request) noexcept;

private:
    void Destroy() noexcept;
    void CreateAction(XrActionType type, const char* name,
                      const char* localizedName,
                      std::uint32_t subactionCount,
                      const XrPath* subactions,
                      XrAction& action);
    void SuggestBindings(const char* profile,
                         const XrActionSuggestedBinding* bindings,
                         std::uint32_t bindingCount) noexcept;
    bool ReadVector2(XrSession session, XrAction action,
                     XrPath hand, float& x, float& y) noexcept;
    bool ReadFloat(XrSession session, XrAction action,
                   XrPath hand, float& value) noexcept;
    bool ReadBoolean(XrSession session, XrAction action,
                     XrPath hand, bool& value) noexcept;
    bool ReadPose(XrSession session, XrAction action,
                  XrPath hand, XrSpace actionSpace,
                  XrSpace baseSpace, XrTime displayTime,
                  FearVrPose& pose) noexcept;
    void LogStateChanges(const FearVrInputState& state) noexcept;

    XrInputLogFunction log_;
    XrInstance instance_{XR_NULL_HANDLE};
    XrActionSet actionSet_{XR_NULL_HANDLE};
    XrPath handPath_[FEARVR_HAND_COUNT]{XR_NULL_PATH, XR_NULL_PATH};
    XrAction moveAction_{XR_NULL_HANDLE};
    XrAction turnAction_{XR_NULL_HANDLE};
    XrAction triggerAction_{XR_NULL_HANDLE};
    XrAction squeezeAction_{XR_NULL_HANDLE};
    XrAction primaryAction_{XR_NULL_HANDLE};
    XrAction secondaryAction_{XR_NULL_HANDLE};
    XrAction menuAction_{XR_NULL_HANDLE};
    XrAction stickClickAction_{XR_NULL_HANDLE};
    XrAction aimPoseAction_{XR_NULL_HANDLE};
    XrAction gripPoseAction_{XR_NULL_HANDLE};
    XrAction hapticAction_{XR_NULL_HANDLE};
    XrSpace aimSpace_[FEARVR_HAND_COUNT]{
        XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrSpace gripSpace_[FEARVR_HAND_COUNT]{
        XR_NULL_HANDLE, XR_NULL_HANDLE};
    std::uint64_t sampleId_{0};
    std::uint32_t lastActiveHands_{0};
    std::uint32_t lastButtons_{0};
    std::uint32_t lastAimPoseValidHands_{0};
    std::uint32_t lastGripPoseValidHands_{0};
    std::uint32_t activeSampleLogCounter_{0};
    XrPath lastInteractionProfile_[FEARVR_HAND_COUNT]{
        XR_NULL_PATH, XR_NULL_PATH};
    bool genericControllerEnabled_{false};
    bool interactionProfilesDirty_{true};
    bool interactionProfilesLogged_{false};
    bool attached_{false};
    bool syncFailureLogged_{false};
};

} // namespace fearvr
