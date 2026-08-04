#include "stereo_hook.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <limits>

#include <iltclient.h>
#include <iltcommon.h>
#include <idatabasemgr.h>
#include <ltobjectcreate.h>
#include <iclientshell.h>
#include <iltdrawprim.h>
#include <iltmodel.h>
#include <iltphysics.h>
#include <iltphysicssim.h>
#include <iltrenderer.h>
#include <MinHook.h>

#include "camera_collision.h"
#include "arm_ik.h"
#include "controller_mapping.h"
#include "dev_menu_model.h"
#include "head_tracking_math.h"
#include "input_state.h"
#include "lean_collision.h"
#include "melee_actions.h"
#include "physical_duck.h"
#include "protocol.h"
#include "stereo_math.h"
#include "vertical_camera_height.h"
#include "wrist_hud_visibility.h"
#include "climb_grip.h"
#include "two_handed_grip.h"
#include "two_handed_jump_camera.h"
#include "vr_menu_scroll.h"
#include "weapon_collision.h"
#include "weapon_recoil.h"
#include "weapon_weight.h"
#include "vr_menu_model.h"

namespace fearvr {
namespace {

using IsHostConnectedFunction = BOOL(__cdecl*)();
using IsStereoAvailableFunction = BOOL(__cdecl*)();
using IsStereoEnabledFunction = BOOL(__cdecl*)();
using SetStereoEnabledFunction = void(__cdecl*)(BOOL);
using SetFovScalePercentFunction =
    void(__cdecl*)(std::uint32_t);
using GetRenderScalePercentFunction = std::uint32_t(__cdecl*)();
using SetRenderScalePercentFunction =
    void(__cdecl*)(std::uint32_t);
using GetBooleanOptionFunction = BOOL(__cdecl*)();
using SetBooleanOptionFunction = void(__cdecl*)(BOOL);
using SetMenuActiveFunction = void(__cdecl*)(BOOL);
using RequestRecenterFunction = void(__cdecl*)();
using IsFlatPanelActiveFunction = BOOL(__cdecl*)();
using StereoToggleCallback = void(__cdecl*)(BOOL);
using RegisterStereoToggleFunction =
    void(__cdecl*)(StereoToggleCallback);
using GetRenderRequestFunction =
    BOOL(__cdecl*)(FearVrRenderRequest*);
using WaitForNewRenderRequestFunction =
    BOOL(__cdecl*)(
        std::uint64_t, std::uint32_t, FearVrRenderRequest*);
using GetInputStateFunction =
    BOOL(__cdecl*)(FearVrInputState*);
using SubmitHapticRequestFunction =
    BOOL(__cdecl*)(const FearVrHapticRequest*);
using BeginEyeFunction = void(__cdecl*)(std::uint32_t);
using CaptureEyeFunction = void(__cdecl*)(std::uint32_t);
using EndStereoFrameFunction = void(__cdecl*)(
    std::uint64_t, const FearVrGameCameraSample*);
using ReportHookStatusFunction =
    void(__cdecl*)(const char*, const char*, const char*);
using RenderPlayerCameraFunction =
    LTRESULT(__thiscall*)(ILTRenderer*, HLOCALOBJ);
using RenderCameraWithOverrideFunction =
    LTRESULT(__thiscall*)(ILTRenderer*, HLOCALOBJ, const char*);
using ClientShellUpdateFunction =
    void(__thiscall*)(IClientShell*);
struct RetailBinding;
using RetailGetBindingValueFunction =
    float(__thiscall*)(
        const void*, const RetailBinding*, bool);
using RetailWeaponManagerUpdateFunction =
    int(__thiscall*)(
        void*, const LTRotation&, const LTVector&);
using RetailSetWeaponTransformFunction =
    void(__thiscall*)(void*, const LTTransform&);
using RetailSetWeaponVisibleFunction =
    void(__thiscall*)(void*, bool, bool);
using RetailStartMuzzleFlashFunction =
    void(__thiscall*)(void*);
using RetailGetFireVectorsFunction =
    bool(__thiscall*)(
        const void*, LTVector&, LTVector&, LTVector&, LTVector&);
using RetailSetTrackedTargetFunction =
    void(__thiscall*)(void*, int, const LTVector&);
using RetailCalcNoClipFunction =
    void(__thiscall*)(void*, LTVector&, LTRotation&);
using RetailAccuracyManagerFunction = void*(__cdecl*)();
using RetailMenuInitFunction = bool(__thiscall*)(void*, void*);
using RetailMenuOnCommandFunction =
    std::uint32_t(__thiscall*)(
        void*, std::uint32_t, std::uint32_t, std::uint32_t);
using RetailMenuOnFocusFunction = void(__thiscall*)(void*, bool);
using RetailMenuAddControlFunction =
    std::uint16_t(__thiscall*)(
        void*, const wchar_t*, std::uint32_t, bool);
using RetailListGetControlFunction =
    void*(__thiscall*)(void*, std::uint32_t);
using RetailListSwapItemsFunction =
    void(__thiscall*)(void*, std::uint32_t, std::uint32_t);
using RetailListSetSelectionFunction =
    std::uint32_t(__thiscall*)(void*, std::uint32_t);
using RetailControlShowFunction = void(__thiscall*)(void*, bool);
using RetailMenuNavigateFunction = bool(__thiscall*)(void*);
using RetailGetAmmoCountFunction =
    std::uint32_t(__thiscall*)(const void*, HRECORD);
using RetailGetWeaponDataFunction =
    HRECORD(__thiscall*)(const void*, HRECORD, bool);
using RetailGetPlayerGrenadeFunction =
    HRECORD(__thiscall*)(const void*, std::uint8_t);
using RetailGetGearRecordFunction =
    HRECORD(__thiscall*)(const void*, const char*);
using RetailGetGearCountFunction =
    std::uint8_t(__thiscall*)(const void*, HRECORD);

ILTClient* g_client = nullptr;
ILTRenderer* g_renderer = nullptr;
IsHostConnectedFunction g_isHostConnected = nullptr;
IsStereoAvailableFunction g_isStereoAvailable = nullptr;
IsStereoEnabledFunction g_isStereoEnabled = nullptr;
SetStereoEnabledFunction g_setStereoEnabled = nullptr;
SetFovScalePercentFunction g_setFovScalePercent = nullptr;
GetRenderScalePercentFunction g_getRenderScalePercent = nullptr;
SetRenderScalePercentFunction g_setRenderScalePercent = nullptr;
GetBooleanOptionFunction g_isTranslationEnabled = nullptr;
SetBooleanOptionFunction g_setTranslationEnabled = nullptr;
GetBooleanOptionFunction g_isStereoHudEnabled = nullptr;
SetBooleanOptionFunction g_setStereoHudEnabled = nullptr;
GetBooleanOptionFunction g_isComfortModeEnabled = nullptr;
SetBooleanOptionFunction g_setComfortModeEnabled = nullptr;
SetMenuActiveFunction g_setMenuActive = nullptr;
RequestRecenterFunction g_requestRecenter = nullptr;
IsFlatPanelActiveFunction g_isFlatPanelActive = nullptr;
RegisterStereoToggleFunction g_registerStereoToggle = nullptr;
GetRenderRequestFunction g_getRenderRequest = nullptr;
WaitForNewRenderRequestFunction g_waitForNewRenderRequest = nullptr;
GetInputStateFunction g_getInputState = nullptr;
SubmitHapticRequestFunction g_submitHapticRequest = nullptr;
BeginEyeFunction g_beginEye = nullptr;
CaptureEyeFunction g_captureEye = nullptr;
EndStereoFrameFunction g_endStereoFrame = nullptr;
ReportHookStatusFunction g_reportHookStatus = nullptr;
RenderPlayerCameraFunction g_renderPlayerCamera = nullptr;
RenderCameraWithOverrideFunction g_renderCameraWithOverride = nullptr;
void** g_renderCameraSlot = nullptr;
IClientShell* g_clientShell = nullptr;
ClientShellUpdateFunction g_clientShellUpdate = nullptr;
RetailGetBindingValueFunction g_retailGetBindingValue = nullptr;
void* g_retailGetBindingValueTarget = nullptr;
RetailWeaponManagerUpdateFunction g_retailWeaponManagerUpdate = nullptr;
RetailSetWeaponTransformFunction g_retailSetWeaponTransform = nullptr;
RetailSetWeaponVisibleFunction g_retailSetWeaponVisible = nullptr;
RetailStartMuzzleFlashFunction g_retailStartMuzzleFlash = nullptr;
RetailGetFireVectorsFunction g_retailGetFireVectors = nullptr;
RetailSetTrackedTargetFunction g_retailSetTrackedTarget = nullptr;
ObjectFilterFn g_retailVectorObjectFilter = nullptr;
RetailCalcNoClipFunction g_retailCalcNoClip = nullptr;
void* g_retailCalcNoClipTarget = nullptr;
RetailAccuracyManagerFunction g_retailAccuracyManager = nullptr;
RetailMenuInitFunction g_retailMenuInit = nullptr;
RetailMenuOnCommandFunction g_retailMenuOnCommand = nullptr;
RetailMenuOnFocusFunction g_retailMenuOnFocus = nullptr;
RetailMenuAddControlFunction g_retailMenuAddControl = nullptr;
RetailListGetControlFunction g_retailListGetControl = nullptr;
RetailListSwapItemsFunction g_retailListSwapItems = nullptr;
RetailListSetSelectionFunction g_retailListSetSelection = nullptr;
void* g_retailMenuInitTarget = nullptr;
void* g_retailMenuOnCommandTarget = nullptr;
void* g_retailMenuOnFocusTarget = nullptr;
// Strahlbasierte Interaktion: beides sind thiscall-Ziele, edx bleibt ungenutzt.
using RetailCheckForIntersectFunction =
    void(__fastcall*)(void*, void*, float*);
using RetailObjectDetectorUpdateFunction =
    void(__fastcall*)(void*, void*, float);
RetailCheckForIntersectFunction g_retailCheckForIntersect = nullptr;
RetailObjectDetectorUpdateFunction g_retailObjectDetectorUpdate = nullptr;
void* g_retailCheckForIntersectTarget = nullptr;
void* g_retailObjectDetectorUpdateTarget = nullptr;
void** g_retailPlayerMgrPointer = nullptr;
volatile LONG g_interactionRayActiveLogged = 0;
bool g_interactionReachOriginalKnown = false;
bool g_interactionReachApplied = false;
float g_activationReachOriginal = 0.0F;
float g_pickupReachOriginal = 0.0F;
volatile LONG g_pickupRayActiveLogged = 0;
void* g_retailWeaponManagerUpdateTarget = nullptr;
void* g_retailStartMuzzleFlashTarget = nullptr;
void* g_retailGetFireVectorsTarget = nullptr;
void* g_retailSetTrackedTargetTarget = nullptr;
HOBJECT g_playerBodyObject = nullptr;
struct HandNodeControlState {
    HMODELNODE node{INVALID_MODEL_NODE};
    HMODELNODE upperArmNode{INVALID_MODEL_NODE};
    HMODELNODE forearmNode{INVALID_MODEL_NODE};
    LTTransform socketFromNode;
    LTVector forearmOffsetFromUpperArm;
    LTVector socketOffsetFromForearm;
    LTVector desiredElbowWorld;
    LTVector bendDirectionWorld;
    float elbowSideSign{0.0F};
    bool installed{false};
    bool upperArmInstalled{false};
    bool forearmInstalled{false};
    bool socketFromNodeValid{false};
    bool forearmOffsetFromUpperArmValid{false};
    bool socketOffsetFromForearmValid{false};
    bool desiredElbowValid{false};
    bool bendDirectionValid{false};
};
HandNodeControlState g_rightHandControl;
HandNodeControlState g_leftHandControl;
struct BodyPresentationNodeControlState {
    HMODELNODE node{INVALID_MODEL_NODE};
    bool installed{false};
};
BodyPresentationNodeControlState g_bodyPresentationNodeControl;
thread_local LTVector g_bodyPresentationWorldOffset;
thread_local bool g_bodyPresentationOffsetActive = false;
SRWLOCK g_hookLock = SRWLOCK_INIT;
bool g_hookInstalled = false;
thread_local bool g_inStereoRender = false;
thread_local const char* g_stereoStep = "idle";
struct StereoRecoveryState {
    LTRigidTransform transform;
    float fovX{0.0F};
    float fovY{0.0F};
    bool valid{false};
};
thread_local StereoRecoveryState g_stereoRecovery;
struct HeadTrackingState {
    FearVrPose recenter{};
    FearVrPose currentCenter{};
    float sessionEyeHeightMeters{0.0F};
    float verticalTranslationMeters{0.0F};
    std::uint64_t lastFrameId{0};
    ULONGLONG lastFreshFrameTick{0};
    std::uint32_t recenterGeneration{0};
    LONG resetGeneration{0};
    bool centered{false};
    bool trackingLost{false};
    bool floorSpace{false};
    bool verticalTranslationOverride{false};
};
thread_local HeadTrackingState g_headTracking;
volatile LONG g_firstHookCallLogged = 0;
volatile LONG g_playerHookCallLogged = 0;
volatile LONG g_firstStereoFrameLogged = 0;
volatile LONG g_stereoFallbackLogged = 0;
volatile LONG g_headTrackingActiveLogged = 0;
volatile LONG g_trackingResetGeneration = 0;
volatile LONG g_clientInputHookCallLogged = 0;
volatile LONG g_weaponAimActiveLogged = 0;
volatile LONG g_weaponBodyAimActiveLogged = 0;
volatile LONG g_weaponHandTrackingActiveLogged = 0;
volatile LONG g_leftHandTrackingActiveLogged = 0;
volatile LONG g_rightForearmTrackingActiveLogged = 0;
volatile LONG g_leftForearmTrackingActiveLogged = 0;
volatile LONG g_bulletGuideAlignmentActiveLogged = 0;
volatile LONG g_weaponHandTrackingFailureLogged = 0;
volatile LONG g_weaponAimGuideActiveLogged = 0;
volatile LONG g_twoHandedGripActiveLogged = 0;
volatile LONG g_dualPistolsActiveLogged = 0;
volatile LONG g_handPoseGapBridgedLogged = 0;
volatile LONG g_weaponSocketSyncActiveLogged = 0;
const void* g_retailVisibilityInitializedWeapon = nullptr;
thread_local void* g_retailWeaponUpdateInProgress = nullptr;
volatile LONG g_weaponCameraBaseSyncActiveLogged = 0;
volatile LONG g_directionalCameraHeightBypassLogged = 0;
volatile LONG g_twoHandedCameraHeightStabilizedLogged = 0;
volatile LONG g_twoHandedViewRotationStabilizedLogged = 0;
volatile LONG g_twoHandedCameraPositionStabilizedLogged = 0;
volatile LONG g_armGeometryInspectedLogged = 0;
volatile LONG g_armGeometryEmptyAttempts = 0;
volatile LONG g_armGeometryNeverAvailableLogged = 0;
volatile LONG g_bodyMaterialOverrideLogged = 0;
// Retail player.Model00p exposes four unnamed pieces, so the piece carrying
// the hands can only be identified by isolating them one at a time.
constexpr std::uint32_t kPlayerBodyPieceMaskAll = 0xFU;
// Benutzerbestätigt am 25.07.2026: Piece #1 trägt die Arme. Ohne dieses Piece
// bleiben Hände und Waffe sichtbar, Ober- und Unterarm verschwinden.
// Piece #1 ist tatsaechlich Body_Group: Es enthaelt Arme, Torso und Beine
// gemeinsam. Der alte Wert 2 entfernte dadurch auch alle sichtbaren Kick-
// Animationen. Die Armflaechen werden nun im lokal erzeugten Alpha-Material
// ausgeblendet; alle vier Mesh-Pieces muessen sichtbar bleiben.
constexpr std::uint32_t kLegacyPlayerBodyArmPieceMask = 0x2U;
std::uint32_t g_hiddenBodyPieceMask = 0;
std::uint32_t g_bodyPieceProbeStep = 0;
bool g_bodyPieceProbeKeyWasDown = false;
// Standardmaessig stanzt das lokal erzeugte Alpha-Test-Material nur Ober- und
// Unterarme aus dem gemeinsamen PlayerBody-Atlas. Haende, Torso und Beine
// bleiben sichtbar; der Kopf wird ueber Materialslot 1 separat verborgen.
bool g_showPlayerArms = false;
std::uint64_t g_hapticRequestId = 0;
std::uint64_t g_lastInputSampleId = 0;
ULONGLONG g_lastInputSampleTick = 0;
std::uint32_t g_lastInputButtons = 0;
std::uint32_t g_lastActiveHands = 0;
ULONGLONG g_lastFireHapticTick = 0;
volatile LONG g_fireHapticActiveLogged = 0;
constexpr std::size_t kVrMuzzleFlashRight = 0;
constexpr std::size_t kVrMuzzleFlashLeft = 1;
constexpr std::size_t kVrMuzzleFlashCount = 2;
struct VrMuzzleFlashState {
    HLOCALOBJ flare{nullptr};
    HLOCALOBJ light{nullptr};
    ULONGLONG pulseStartedTick{0};
    ULONGLONG visibleUntilTick{0};
    ULONGLONG lastTriggerTick{0};
    std::uint32_t pulseSerial{0};
};
VrMuzzleFlashState g_vrMuzzleFlash[kVrMuzzleFlashCount];
volatile LONG g_vrMuzzleFlashCreatedLogged = 0;
volatile LONG g_vrMuzzleFlashActiveLogged = 0;
volatile LONG g_vrMuzzleFlashRenderedLogged = 0;
void RenderVrMuzzleFlash(HLOCALOBJ camera) noexcept;
ULONGLONG g_lastWeaponRecoilTick = 0;
volatile LONG g_pendingWeaponRecoilShots = 0;
volatile LONG g_weaponRecoilActiveLogged = 0;
std::uint32_t g_lastMenuButtons = 0;
bool g_lastMenuTriggerDown = false;
bool g_menuAxisDown[4]{};
ULONGLONG g_menuAxisRepeatTick[4]{};
bool g_menuControllerActive = false;
ULONGLONG g_menuActivationHoldUntil = 0;
bool g_menuFocusKnown = false;
bool g_menuFocusActive = false;
bool g_escapeWasDown = false;
constexpr std::size_t kDevMenuTabCount = 8;
constexpr float kDevMenuDistanceMeters = 1.15F;
constexpr float kDevMenuVerticalOffsetMeters = 0.08F;
constexpr float kDevMenuWidthMeters = 0.64F;
constexpr float kDevMenuHeightMeters = 0.66F;
constexpr float kDevMenuHeaderMeters = 0.12F;
constexpr float kDevMenuTitleMeters = 0.055F;
constexpr float kDevMenuTabMeters = 0.055F;
constexpr float kDevMenuRowMeters = 0.055F;
enum class DevMenuTab : std::size_t {
    recoil,
    weight,
    collision,
    weapon,
    ik,
    movement,
    melee,
    vr,
};
constexpr std::array<const wchar_t*, kDevMenuTabCount> kDevMenuTabLabels{
    L"RECOIL", L"WEIGHT", L"COLLIDE", L"WEAPON", L"IK", L"MOVE",
    L"MELEE", L"VR"};
constexpr std::array<std::size_t, kDevMenuTabCount> kDevMenuTabRowCounts{
    5U, 6U, 7U, 3U, 7U, 9U, 5U, 6U};

std::size_t DevMenuRowCount(DevMenuTab tab) noexcept {
    const std::size_t index = static_cast<std::size_t>(tab);
    return index < kDevMenuTabRowCounts.size()
        ? kDevMenuTabRowCounts[index] : 0U;
}
struct DevMenuState {
    bool open{false};
    bool anchorValid{false};
    bool suppressUntilRelease{false};
    std::size_t selectedRow{0};
    std::uint32_t lastButtons{0};
    bool lastTriggerDown{false};
    bool axisUpDown{false};
    bool axisDownDown{false};
    bool axisLeftDown{false};
    bool axisRightDown{false};
    ULONGLONG axisUpRepeatTick{0};
    ULONGLONG axisDownRepeatTick{0};
    ULONGLONG axisLeftRepeatTick{0};
    ULONGLONG axisRightRepeatTick{0};
    DevMenuTab selectedTab{DevMenuTab::recoil};
    LTVector center;
    LTVector right;
    LTVector up;
    LTVector normal;
    LTVector pointerWorld;
    bool pointerValid{false};
    DevMenuHitRegion pointerRegion{DevMenuHitRegion::none};
    std::size_t pointerIndex{0};
};
DevMenuState g_devMenu;
constexpr std::size_t kDevMenuGlyphQuadCapacity = 6144;
std::array<LT_POLYG4, kDevMenuGlyphQuadCapacity> g_devMenuGlyphQuads;

bool DevMenuCapturesControllerInput() noexcept {
    return g_devMenu.open || g_devMenu.suppressUntilRelease;
}
bool g_seenForwardAxisBinding = false;
bool g_seenStrafeAxisBinding = false;
bool g_controllerCommandActive[128]{};
bool g_injectedCommandActive[128]{};
ULONGLONG g_weaponSwitchPulseUntil = 0;
bool g_weaponSwitchTriggered = false;
ULONGLONG g_secondaryHoldStartTick = 0;
ULONGLONG g_reloadPulseUntil = 0;
ULONGLONG g_grenadePulseUntil = 0;
bool g_secondaryWasDown = false;
bool g_grenadeConsumed = false;
ULONGLONG g_meleePulseUntil = 0;
ULONGLONG g_slideDuckPulseUntil = 0;
ULONGLONG g_slideForwardPulseUntil = 0;
ULONGLONG g_meleePeakReportTick = 0;
MeleeActionState g_meleeActions{};
bool g_meleeThrustEnabled = true;
bool g_meleeWeaponStrikeEnabled = true;
bool g_meleeOffHandStrikeEnabled = true;
bool g_meleeJumpKickEnabled = true;
bool g_meleeSlideKickEnabled = true;
struct SlideKickViewStabilization {
    LTRotation localRotation;
    float pitch{0.0F};
    float roll{0.0F};
    ULONGLONG startTick{0};
    bool active{false};
};
SlideKickViewStabilization g_slideKickView{};
struct RetailMovementSnapshot {
    std::uint32_t controlFlags{0};
    float postureDownSeconds{0.0F};
    bool available{false};
    bool onGround{false};
    bool falling{false};
    bool jumped{false};
    bool airborne{false};
    bool postureDownWindow{false};
};
RetailMovementSnapshot g_retailMovement{};
bool g_retailMovementHadSnapshot = false;
bool g_retailDuckWasDown = false;
ULONGLONG g_retailPostureDownUntil = 0;
ULONGLONG g_retailUnsupportedSince = 0;
ULONGLONG g_retailSupportedSince = 0;
bool g_retailPersistentUnsupported = false;
HCONSOLEVAR g_retailPostureDownVariable = nullptr;
thread_local bool g_semanticBitsInjected = false;
FearVrInputState g_currentInput{};
struct WeaponAimState {
    LTRigidTransform fireTransform;
    LTRigidTransform gripTransform;
    // Unfiltered right-hand grip used only for the physical line between
    // both controllers. Weapon weight may lag the visible weapon behind this
    // pose, but must not turn that lag into false two-handed steering.
    LTRigidTransform supportRightGripTransform;
    LTRigidTransform leftAimTransform;
    LTRigidTransform leftGripTransform;
    LTRigidTransform leftWeaponTransform;
    LTRigidTransform muzzleTransform;
    LTRigidTransform leftMuzzleTransform;
    LTRigidTransform trackingBase;
    LTVector muzzleForwardInWeapon;
    // Mündung als starrer Versatz im Waffenraum. Damit lässt sich der
    // Schussursprung aus unserer eigenen Waffentransformation rekonstruieren,
    // statt aus der Welt-Sockettransformation der Engine.
    LTVector muzzleOffsetInWeapon;
    LTRotation muzzleRotationInWeapon;
    LTVector leftMuzzleOffsetInWeapon;
    LTRotation leftMuzzleRotationInWeapon;
    const void* muzzleWeapon{nullptr};
    HOBJECT leftWeaponModel{nullptr};
    void* retailWeapon{nullptr};
    bool valid{false};
    bool gripValid{false};
    bool supportRightGripValid{false};
    bool leftAimValid{false};
    bool leftGripValid{false};
    bool muzzleValid{false};
    bool muzzleDirectionValid{false};
    bool muzzleLocalValid{false};
    bool muzzleDiagnosticLogged{false};
    bool leftWeaponTransformValid{false};
    bool leftMuzzleValid{false};
    bool leftMuzzleLocalValid{false};
    bool trackingBaseValid{false};
};
thread_local WeaponAimState g_weaponAim;

bool DualPistolsVrActive() noexcept {
    return g_weaponAim.leftWeaponModel != nullptr;
}

HLOCALOBJ g_playerCameraObject = nullptr;
VerticalCameraHeightState g_verticalCameraHeight;
TwoHandedJumpCameraState g_twoHandedJumpCamera;
struct TwoHandedCameraPositionState {
    LTVector localCameraOffset;
    bool valid{false};
};
TwoHandedCameraPositionState g_twoHandedCameraPosition;
LTVector g_visualCameraPosition;
ULONGLONG g_visualCameraHeightSampleTick = 0;
bool g_visualCameraHeightValid = false;
// Der Weapon-Manager liefert die gemeinsame Y-Basis von Augen, Händen und
// Waffen normalerweise in jedem Spielbild. Bei einem kurzen Render-Hitch darf
// die Darstellung nicht für genau ein Bild auf die Kameraobjekt-Höhe
// zurückspringen und danach wieder zurück: Das ist besonders am nahen Körper
// sichtbar. Echte Zustandswechsel invalidieren den Wert ausdrücklich; diese
// Frist überbrückt daher nur temporäre Timing-Lücken im laufenden Spiel.
constexpr ULONGLONG kVisualCameraHeightFreshMilliseconds = 500;
thread_local bool g_cutsceneCameraStateKnown = false;
thread_local bool g_cutsceneCameraState = false;
thread_local ULONGLONG g_cutsceneCameraActivationTick = 0;
struct CutsceneBodyPresentationState {
    LTVector bodyOffsetFromCamera;
    HLOCALOBJ bodyObject{nullptr};
    bool valid{false};
};
thread_local CutsceneBodyPresentationState g_cutsceneBodyPresentation;
enum class FlashlightMount : int {
    LeftHand = 0,
    Head,
    Weapon,
    Count,
};
constexpr int kFlashlightMountCount =
    static_cast<int>(FlashlightMount::Count);
FlashlightMount g_flashlightMount = FlashlightMount::Weapon;
HLOCALOBJ g_flashlightModel = nullptr;
HLOCALOBJ g_flashlightLight = nullptr;
const void* g_flashlightModelWeapon = nullptr;
// Retail hat die Waffe abgeschaltet: kein Modell, kein Zielstrahl.
bool g_weaponDisabled = false;
LTRigidTransform g_preRenderCameraRecovery;
float g_preRenderCameraRecoveryFovX = 0.0F;
float g_preRenderCameraRecoveryFovY = 0.0F;
bool g_preRenderCameraOverridePending = false;
// Genau das Objekt, dessen Transform wir entfuehrt haben. Eine
// Zwischensequenz kann die Spielkamera austauschen; dann darf der gemerkte
// Transform nicht in ein fremdes oder totes Objekt zurueckgeschrieben werden.
HLOCALOBJ g_preRenderCameraOverrideObject = nullptr;
volatile LONG g_renderTargetCameraStabilizedLogged = 0;
bool g_flashlightEnabled = true;
bool g_flashlightButtonWasDown = false;
bool g_dualPistolSlowMoActive = false;
struct TrackedPoseCache {
    FearVrPose pose{};
    ULONGLONG lastValidTick{0};
    LONG resetGeneration{-1};
    bool valid{false};
};
thread_local TrackedPoseCache g_aimPoseCache[FEARVR_HAND_COUNT];
thread_local TrackedPoseCache g_gripPoseCache[FEARVR_HAND_COUNT];
thread_local TrackedPoseCache g_supportRightGripPoseCache;
ULONGLONG g_lastWeaponManagerUpdateTick = 0;
// Laeuft das Weapon-Manager-Update, ist ein normaler Spielframe aktiv.
// Zwischensequenzen, Ladebildschirme und Menues halten es an.
constexpr ULONGLONG kPlayingFrameFreshMilliseconds = 500;
// Retails Third-Person-Koerper steht fast mittig unter der Augenkamera. Im
// breiten VR-Sichtfeld ragen Brust und Kragen dadurch schon bei waagerechtem
// Blick ins Bild. Nur das gerenderte Skelett wird etwas hinter und unter den
// HMD-Anker gesetzt; Kollisionskoerper, Haende und Waffen bleiben unveraendert.
constexpr float kPlayerBodyRearwardOffsetUnits = 18.0F;
constexpr float kPlayerBodyDownwardOffsetUnits = 8.0F;
// Retail-Spielzustand (`CInterfaceMgr::m_eGameState`). Er ist die einzige
// verlaessliche Quelle dafuer, ob gerade ein Vollbild-UI laeuft.
const void* const* g_retailInterfaceMgrPointer = nullptr;
bool g_retailGameStateResolveAttempted = false;
bool g_disableRetailGameState = false;
int g_lastReportedRetailGameState = -2;
const void* const* g_retailPlayerStatsPointer = nullptr;
const void* const* g_retailWeaponDatabasePointer = nullptr;
IDatabaseMgr* const* g_retailDatabasePointer = nullptr;
RetailGetAmmoCountFunction g_retailGetAmmoCount = nullptr;
RetailGetWeaponDataFunction g_retailGetWeaponData = nullptr;
RetailGetPlayerGrenadeFunction g_retailGetPlayerGrenade = nullptr;
RetailGetGearRecordFunction g_retailGetGearRecord = nullptr;
RetailGetGearCountFunction g_retailGetGearCount = nullptr;
bool g_retailWristHudResolveAttempted = false;
ULONGLONG g_wristHudVisibleUntil = 0;
ULONGLONG g_wristHudClockRefreshAt = 0;
ULONGLONG g_dualPistolWristCandidateSince = 0;
ULONGLONG g_dualPistolWristDiagnosticAt = 0;
char g_wristHudClockText[6] = "00:00";
bool g_dualPistolWristHudVisible = false;
volatile LONG g_wristHudActiveLogged = 0;
struct RetailHudVariableOverride {
    const char* name;
    float originalValue{1.0F};
    bool originalKnown{false};
};
RetailHudVariableOverride g_retailHudVariables[] = {
    {"XUIUseOldHUD"},
    {"HUDHealthRender"},
    {"HUDArmorRender"},
    {"HUDAmmoRender"},
    {"HUDGrenadeRender"},
    {"HUDGearRender"},
};
bool g_retailHudOverrideApplied = false;
LONG g_commandInjectionSuspendedLogged = 0;
bool g_autoStereoActivationAttempted = false;
bool g_crosshairOverrideApplied = false;
bool g_crosshairOriginalKnown = false;
float g_crosshairOriginalValue = 0.0F;
bool g_recoilOverrideApplied = false;
bool g_recoilOriginalKnown = false;
float g_recoilOriginalValue = -1.0F;
bool g_cameraCollisionRaycastApplied = false;
bool g_cameraCollisionOriginalKnown = false;
float g_cameraCollisionOriginalUseObject = 1.0F;
volatile LONG g_cameraCharacterFilterActiveLogged = 0;
bool g_weaponAimGuideEnabled = true;
bool g_controllerHapticsEnabled = true;
// Mithalten der Waffe mit der linken Hand. `active` wird im
// Weapon-Manager-Update gesetzt und im Bindungshook gelesen; beide laufen auf
// dem Clientthread, deshalb reicht ein einfacher Wert.
bool g_twoHandedGripEnabled = true;
bool g_twoHandedGripActive = false;
// Handklettern an Leitern. Stufe 1: Die Greif- und Zugauswertung laeuft und
// wird protokolliert, greift aber noch nicht in die Fortbewegung ein. Dafuer
// fehlt die Leitererkennung — `LadderMgr::Instance().m_pLadder` in
// GameOrig.dll, deren Adresse noch nicht belegt ist.
// Physisches Lehnen: Der Kopfversatz aus dem Headtracking bewegt den
// Blickpunkt, an Waenden begrenzt. Retails eigenes Lehnen kippt nur die
// Kamera, der Blickpunkt bleibt dabei in der Spielerposition stehen.
bool g_physicalLeanEnabled = true;
bool g_physicalDuckEnabled = true;
bool g_physicalDuckActive = false;
// Verstaerkung des physischen Lehnens, in Prozent. Ein Kopfversatz von zehn
// Zentimetern wirkt bei 200 wie zwanzig — man muss sich also nur halb so weit
// beugen, um hinter einer Deckung hervorzusehen. Der gemessene Versatz bleibt
// vorher auf 25 cm begrenzt, die Wirkung damit auf einen halben Meter.
int g_leanScalePercent = 200;
// Zero keeps a per-session automatic neutral height. A positive value is an
// explicit floor-to-eye height captured by the floating developer menu.
float g_configuredEyeHeightMeters = 0.0F;
LeanCollisionState g_leanCollision;
float g_leanTranslationScale = 1.0F;
// Der Versatz, um den der Blickpunkt gegenueber dem Spielerkoerper steht.
// Die Handposen bekommen ihn ebenfalls: Sie sind relativ zum HMD gerechnet,
// werden aber an die Spielerposition gehaengt — ohne diesen Ausgleich wandert
// die Waffe beim Lehnen aus der Hand. Das sichtbare Koerpermodell folgt dem
// horizontalen Anteil erst beim Rendern; Retails Spielerobjekt und seine
// Kollisionskapsel bleiben stehen, damit keine Rueckkopplung entsteht.
LTVector g_leanViewOffsetUnits;
bool g_climbingEnabled = false;
ClimbGripState g_climbGrip;
bool g_climbWasGripping = false;
// `g_climbOnLadder`: Der Spieler haengt an einer Leiter. Solange das gilt und
// Handklettern eingeschaltet ist, gehoert die Vorwaertsachse den Haenden —
// ohne Griff steht sie still, statt an den Stick zurueckzufallen.
// `g_climbActive`: zusaetzlich greift gerade eine Hand.
bool g_climbOnLadder = false;
bool g_climbActive = false;
float g_climbAxis = 0.0F;
struct TwoHandedGripState {
    // Griffpunkt und Handhaltung im Waffenraum. Damit klebt die sichtbare
    // linke Hand am original animierten Vordergriff der jeweiligen Waffe,
    // statt an der zufaelligen Controllerposition beim Zugreifen.
    LTVector grabOffsetInWeapon;
    LTRotation grabRotationInWeapon;
    // Fuer genau eine unveraenderte Animationsauswertung werden die beiden
    // Retail-Hand-Sockets vor unserem Node-Override abgenommen. Ihr relativer
    // Transform liefert den waffenspezifischen Originalgriff.
    LTRigidTransform animatedRightSocket;
    LTRigidTransform animatedLeftSocket;
    // A valid attachment is retained across grip releases for this exact
    // Retail weapon. Recapturing on every squeeze could sample a reload or
    // weapon-switch animation and turn that transient lowered hand into the
    // permanent weapon axis.
    const void* geometryWeapon{nullptr};
    bool geometryValid{false};
    bool placementValid{false};
    bool animatedRightSocketValid{false};
    bool animatedLeftSocketValid{false};
};
TwoHandedGripState g_twoHandedGrip;
ArmIkTuning g_armIkTuning{};
bool g_devMenuIkLeftHandPage = false;

// Sichtbare Lage der linken Hand. Waehrend des Zweihandgriffs klebt sie am
// waffenspezifischen Originalgriff aus Retails eigener Animation.
//
// Gesteuert wird die Waffe weiterhin von der *echten* Controllerpose. Wuerde
// auch die Steuerung diese Lage benutzen, folgte die Hand der Waffe, die der
// Hand folgt — die Waffe liesse sich dann gar nicht mehr fuehren.
bool LeftHandOnWeapon() noexcept {
    return g_twoHandedGripActive && g_twoHandedGrip.placementValid;
}

LTVector EffectiveLeftHandPosition() noexcept {
    if (LeftHandOnWeapon()) {
        return g_weaponAim.gripTransform.m_vPos +
               g_weaponAim.fireTransform.m_rRot.RotateVector(
                   g_twoHandedGrip.grabOffsetInWeapon);
    }
    if (DualPistolsVrActive() &&
        g_weaponAim.leftWeaponTransformValid) {
        // Retail attaches the left weapon object directly to the LeftHand
        // socket. Keep hand and pistol in exactly the same frame.
        return g_weaponAim.leftWeaponTransform.m_vPos;
    }
    const LTRotation baseRotation =
        g_weaponAim.leftGripTransform.m_rRot;
    const LTVector localOffset(
        g_armIkTuning.leftHandRightMeters * kGameUnitsPerMeter,
        g_armIkTuning.leftHandUpMeters * kGameUnitsPerMeter,
        g_armIkTuning.leftHandForwardMeters * kGameUnitsPerMeter);
    return g_weaponAim.leftGripTransform.m_vPos +
           baseRotation.RotateVector(localOffset);
}

LTRotation EffectiveLeftHandRotation() noexcept {
    if (LeftHandOnWeapon()) {
        // Der waffenspezifische Originalgriff wurde direkt aus Retails
        // Animation abgenommen und ist bereits korrekt. Eine zusätzliche
        // Handkalibrierung würde die Finger aus dem Vordergriff drehen.
        return g_weaponAim.fireTransform.m_rRot *
               g_twoHandedGrip.grabRotationInWeapon;
    }
    if (DualPistolsVrActive()) {
        // CWeaponModelData::UpdateWeaponPosition attaches the weapon object
        // directly to this player-body socket. Reuse the final muzzle-aligned
        // object rotation so hand and handle share exactly the same frame.
        if (g_weaponAim.leftWeaponTransformValid) {
            return g_weaponAim.leftWeaponTransform.m_rRot;
        }
        return g_weaponAim.leftGripTransform.m_rRot;
    }
    // Die freie LeftHand-Skelettachse liegt gegenüber der standardisierten
    // OpenXR-Griffpose weit nach hinten. Die gewünschte Kalibrierung ist 45
    // Grad weniger als die zuletzt getesteten 135 Grad, also lokal 90 Grad
    // über die X-/Querachse nach vorn. Das ist aus Sicht entlang +X eine
    // weitere Drehung im Uhrzeigersinn. Aim-Pose und Schussrichtung bleiben
    // davon ausdrücklich unberührt.
    constexpr float kPi = 3.14159265358979323846F;
    constexpr float kLeftHandForwardCorrection =
        kPi * 0.5F;
    const LTRotation forwardCorrection(
        LTVector(1.0F, 0.0F, 0.0F),
        kLeftHandForwardCorrection);
    const LTRotation baseRotation =
        g_weaponAim.leftGripTransform.m_rRot * forwardCorrection;
    constexpr float kDegreesToRadians =
        3.14159265358979323846F / 180.0F;
    LTRotation correction(
        g_armIkTuning.leftHandPitchDegrees * kDegreesToRadians,
        g_armIkTuning.leftHandYawDegrees * kDegreesToRadians,
        g_armIkTuning.leftHandRollDegrees * kDegreesToRadians);
    correction.Normalize();
    LTRotation result = baseRotation * correction;
    result.Normalize();
    return result;
}

// Die Taschenlampe folgt sonst der Hand. Klebt die Hand an der Waffe, zeigt
// sie aber dorthin, wohin sie im Moment des Zugreifens zufaellig zeigte — und
// der Kegel leuchtet quer. Am Vordergriff gehoert das Licht nach vorn, also
// auf die Waffenachse.
LTRotation EffectiveFlashlightRotation() noexcept {
    return LeftHandOnWeapon()
        ? g_weaponAim.fireTransform.m_rRot
        : g_weaponAim.leftAimTransform.m_rRot;
}
// Linkshaenderbelegung: gespiegelt wird ausschliesslich der eingehende
// Controllerzustand, nie eine einzelne Zuordnung.
bool g_leftHandedBindings = false;
// Keep locomotion independent from free HMD look by default. Head-relative
// steering remains available as an explicit preference.
bool g_headRelativeMovement = false;
bool g_headBobEnabled = false;
bool g_forceHeadBobDisabled = false;
// Diagnoseschalter zum Eingrenzen des Absturzes an einer bestimmten
// Zwischensequenz. Jeder schaltet genau eine Gruppe unserer Schreibzugriffe
// auf Retail-Weltobjekte ab. -fearvr-safe schaltet alle vier zusammen.
bool g_disableFlashlight = false;
bool g_disableHandNodes = false;
bool g_disableWeaponTransform = false;
bool g_disableBodyPieceHiding = false;
// Schaltet den Stereo-Doppelrender ab. Damit laeuft der Weltrender wieder
// genau einmal pro Frame, wie in Retail. Pruefschalter fuer die Frage, ob der
// zweite Durchlauf mehr als nur Geometrie erneut ausloest.
bool g_disableStereoRender = false;
std::uint64_t g_lastStereoRenderRequestId = 0;
// Laesst den Client-Input-Hook installiert, schreibt aber keine Kommandobits
// mehr in Retails CBindMgr. Trennt "Schreibzugriff auf den Bind-Manager" von
// "Hook auf die Bindungsabfrage und synthetische Tastendruecke".
bool g_disableCommandInjection = false;
// Laesst den Bindungs-Hook weg. Zusammen mit -fearvr-no-inject trennt das
// "Eingabe erreicht das Spiel" von "IClientShell::Update-Arbeit pro Frame".
bool g_disableBindingHook = false;
// Laesst die Arbeit im IClientShell::Update-Hook weg: Menueabfrage,
// synthetische Tastendruecke, Fadenkreuz-Override, Waffenwechsel.
bool g_disableClientUpdateWork = false;
// Laesst Weapon-Manager-, AimAt- und Fire-Vector-Hook ungesetzt. Diese Gruppe
// war in jedem abgestuerzten Lauf aktiv und im einzigen ueberlebenden nicht.
bool g_disableAimHooks = false;
bool g_disableInteractionHooks = false;
// Nur der AimAt-Node-Tracker. Er laeuft fuer jeden Charakter, nicht nur fuer
// den Spieler, und ist damit der Kandidat fuer den NPC in der Problemszene.
bool g_disableAimAtHook = false;
// Hook bleibt gesetzt, ueberschreibt das Ziel aber nie. Trennt "der Detour
// selbst stoert" von "unser Zielwert stoert".
bool g_aimAtPassthrough = false;
bool g_stableWeaponMotionConfigured = false;
bool g_headBobOriginalKnown = false;
float g_headBobOriginalScale = 1.0F;
float g_headBobOriginalDebugMode = 0.0F;
float g_headBobOriginalAmplitudes[12]{};
int g_turnSpeedPreset = 1;
constexpr std::array<std::uint32_t, 4> kFovScalePercents{
    100U, 110U, 120U, 130U};
int g_fovScalePreset = 3;
constexpr std::array<std::uint32_t, 5> kRenderScalePercents{
    100U, 125U, 150U, 175U, 200U};
std::uint32_t g_renderScalePercent = 100U;
wchar_t g_vrSettingsPath[MAX_PATH]{};
bool g_vrSettingsFilePresent = false;
// Optional simulated weapon mass. Disabled is the compatibility default.
bool g_weaponWeightEnabled = false;
WeaponWeightPreset g_weaponWeightPreset = WeaponWeightPreset::none;
bool g_weaponWeightDiagnosticsEnabled = false;
WeaponWeightProfile g_defaultWeaponWeightProfile{};
bool g_weaponRecoilEnabled = true;
WeaponRecoilProfile g_weaponRecoilProfile{};
struct WeaponCollisionProfile {
    float lengthScale{1.0F};
    float widthMeters{0.10F};
    float heightMeters{0.18F};
    float forwardOffsetMeters{0.0F};
};

WeaponCollisionProfile SanitizeWeaponCollisionProfile(
    WeaponCollisionProfile profile) noexcept {
    const auto sane = [](float value, float fallback,
                         float minimum, float maximum) noexcept {
        return std::isfinite(value)
            ? std::clamp(value, minimum, maximum) : fallback;
    };
    profile.lengthScale = sane(profile.lengthScale, 1.0F, 0.25F, 2.50F);
    profile.widthMeters = sane(profile.widthMeters, 0.10F, 0.02F, 0.40F);
    profile.heightMeters = sane(profile.heightMeters, 0.18F, 0.02F, 0.40F);
    profile.forwardOffsetMeters = sane(
        profile.forwardOffsetMeters, 0.0F, -0.30F, 0.30F);
    return profile;
}

bool g_weaponCollisionEnabled = true;
bool g_weaponCollisionDebugEnabled = true;
WeaponCollisionProfile g_defaultWeaponCollisionProfile{};
struct WeaponCollisionRuntimeState {
    LTVector correction;
    std::uint64_t lastUpdateNs{0};
    std::uint64_t holdUntilNs{0};
    bool haveUpdate{false};
};

void ResetWeaponCollisionRuntime(
    WeaponCollisionRuntimeState& state) noexcept {
    state = WeaponCollisionRuntimeState{};
}

struct WeightedWeaponInputState {
    WeaponWeightPairState filters;
    WeaponRecoilState recoil;
    WeaponCollisionRuntimeState collision;
    WeaponRecoilOffset recoilOffset;
    FearVrPose aimPose{};
    FearVrPose gripPose{};
    WeaponWeightDiagnostics aimDiagnostics;
    WeaponWeightDiagnostics gripDiagnostics;
    std::uint64_t lastSampleId{0};
    ULONGLONG lastAimValidTick{0};
    ULONGLONG lastGripValidTick{0};
    ULONGLONG lastDiagnosticTick{0};
    ULONGLONG lastCollisionDiagnosticTick{0};
    const void* weapon{nullptr};
    LONG resetGeneration{-1};
    WeaponWeightProfile profile{};
    WeaponRecoilProfile recoilProfile{};
    WeaponCollisionProfile collisionProfile{};
    char profileName[96]{};
    bool aimValid{false};
    bool gripValid{false};
    bool enabledOnLastUpdate{false};
    bool recoilProfileExplicit{false};
    bool collisionProfileExplicit{false};
    bool collisionObstructed{false};
};
thread_local WeightedWeaponInputState g_weightedWeaponInput;

struct VrMenuControl {
    void* object{nullptr};
    std::uint32_t index{0};
};

struct VrMenuToggle {
    VrMenuControl enabled;
    VrMenuControl disabled;
};

constexpr std::size_t kRetailSystemMenuOriginalControlCount = 16;
void* g_vrMenuOwner = nullptr;
void* g_vrNormalControls[
    kRetailSystemMenuOriginalControlCount + 1]{};
std::size_t g_vrNormalControlCount = 0;
bool g_vrMenuControlsBuilt = false;
bool g_vrSettingsPageActive = false;
constexpr std::size_t kRetailVrMenuVisibleControlCount = 17;
std::size_t g_vrMenuFirstVisibleRow = 0;
// CMenuSystem::OnFocus shrinks the list to the stock controls visible in the
// current game state. Learn the real capacity from m_nLastShown instead of
// assuming the default layout height. Eight is only the safe first-frame
// fallback for the single-player pause menu.
std::size_t g_vrMenuPageRows = 8;
VrSettingsPage g_vrSettingsPage = VrSettingsPage::None;
bool g_vrWeaponProfileEditsCurrent = true;
bool g_vrRecoilProfileEditsCurrent = true;
bool g_vrCollisionProfileEditsCurrent = true;
int g_vrOriginalItemSpacing = 0;
bool g_vrOriginalItemSpacingKnown = false;

constexpr std::array<int, 7> kLeanScalePresets{
    100, 150, 200, 250, 300, 350, 400};
constexpr std::array<float, 8> kWeaponWeightPresets{
    0.25F, 0.50F, 0.75F, 1.00F, 1.50F, 2.00F, 3.00F, 4.00F};
constexpr std::array<float, 7> kWeaponPositionFollowPresets{
    6.0F, 10.0F, 14.0F, 18.0F, 24.0F, 32.0F, 40.0F};
constexpr std::array<float, 7> kWeaponRotationFollowPresets{
    6.0F, 10.0F, 14.0F, 20.0F, 26.0F, 32.0F, 40.0F};
constexpr std::array<float, 7> kWeaponCatchUpPresets{
    0.0F, 0.5F, 1.0F, 1.5F, 2.0F, 3.0F, 4.0F};
constexpr std::array<float, 6> kWeaponRecoilStrengthPresets{
    0.50F, 1.00F, 1.50F, 2.00F, 3.00F, 5.00F};
constexpr std::array<float, 6> kWeaponRecoilRisePresets{
    0.00F, 0.50F, 1.00F, 1.50F, 2.00F, 3.00F};
constexpr std::array<float, 6> kWeaponRecoilRecoveryPresets{
    0.50F, 0.75F, 1.00F, 1.50F, 2.00F, 3.00F};
constexpr std::array<float, 8> kWeaponCollisionLengthPresets{
    0.50F, 0.75F, 1.00F, 1.25F, 1.50F, 1.75F, 2.00F, 2.50F};
constexpr std::array<float, 8> kWeaponCollisionWidthPresets{
    0.04F, 0.06F, 0.08F, 0.10F, 0.12F, 0.16F, 0.20F, 0.30F};
constexpr std::array<float, 8> kWeaponCollisionHeightPresets{
    0.06F, 0.08F, 0.10F, 0.14F, 0.18F, 0.22F, 0.28F, 0.36F};
constexpr std::array<float, 9> kWeaponCollisionOffsetPresets{
    -0.20F, -0.15F, -0.10F, -0.05F, 0.00F,
     0.05F,  0.10F,  0.15F, 0.20F};
constexpr float kIkElbowStep = 0.05F;
constexpr float kIkHandPositionStepMeters = 0.005F;
constexpr float kIkHandRotationStepDegrees = 5.0F;

VrMenuControl g_vrMenuEntry;
VrMenuControl g_vrMenuTitle;
VrMenuControl g_vrMenuDisplayTitle;
VrMenuControl g_vrMenuComfortTitle;
VrMenuControl g_vrMenuControlsTitle;
VrMenuControl g_vrMenuWeaponsTitle;
VrMenuControl g_vrMenuWeaponHandlingTitle;
VrMenuControl g_vrMenuWeaponWeightTitle;
VrMenuControl g_vrMenuWeaponRecoilTitle;
VrMenuControl g_vrMenuMeleeTitle;
VrMenuControl g_vrMenuAdvancedTitle;
VrMenuControl g_vrMenuOpenDisplay;
VrMenuControl g_vrMenuOpenComfort;
VrMenuControl g_vrMenuOpenControls;
VrMenuControl g_vrMenuOpenWeapons;
VrMenuControl g_vrMenuOpenWeaponHandling;
VrMenuControl g_vrMenuOpenWeaponWeight;
VrMenuControl g_vrMenuOpenWeaponRecoil;
VrMenuControl g_vrMenuOpenMelee;
VrMenuControl g_vrMenuOpenAdvanced;
VrMenuToggle g_vrMenuStereo;
VrMenuToggle g_vrMenuTranslation;
VrMenuToggle g_vrMenuStereoHud;
VrMenuToggle g_vrMenuHeadBob;
VrMenuToggle g_vrMenuComfort;
VrMenuControl g_vrMenuWeaponWeightPreset[4];
VrMenuToggle g_vrMenuAimGuide;
VrMenuToggle g_vrMenuHaptics;
VrMenuControl g_vrMenuFlashlightMount[kFlashlightMountCount];
// `enabled` zeigt die Rechtshaenderbelegung, `disabled` die gespiegelte.
VrMenuToggle g_vrMenuHandedness;
VrMenuToggle g_vrMenuMovementDirection;
// `enabled` zeigt das Klettern mit den Haenden, `disabled` die
// Retail-Steuerung ueber den Stick.
VrMenuToggle g_vrMenuClimbing;
// `enabled` = physisches Lehnen, `disabled` = nur Retails Kameraneigung.
VrMenuToggle g_vrMenuPhysicalLean;
VrMenuToggle g_vrMenuPhysicalDuck;
VrMenuControl g_vrMenuLeanScale[kLeanScalePresets.size()];
VrMenuToggle g_vrMenuMelee;
VrMenuToggle g_vrMenuMeleeWeaponStrike;
VrMenuToggle g_vrMenuMeleeOffHandStrike;
VrMenuToggle g_vrMenuMeleeJumpKick;
VrMenuToggle g_vrMenuMeleeSlideKick;
VrMenuToggle g_vrMenuArms;
VrMenuToggle g_vrMenuTwoHandGrip;
VrMenuToggle g_vrMenuWeaponWeight;
VrMenuToggle g_vrMenuWeaponRecoil;
VrMenuToggle g_vrMenuWeaponWeightDiagnostics;
VrMenuToggle g_vrMenuWeaponProfileTarget;
VrMenuControl g_vrMenuFovScale[kFovScalePercents.size()];
VrMenuControl g_vrMenuTurnSpeed[3];
VrMenuControl g_vrMenuWeaponWeightValue[kWeaponWeightPresets.size()];
VrMenuControl g_vrMenuWeaponPositionFollow[
    kWeaponPositionFollowPresets.size()];
VrMenuControl g_vrMenuWeaponRotationFollow[
    kWeaponRotationFollowPresets.size()];
VrMenuControl g_vrMenuWeaponCatchUp[kWeaponCatchUpPresets.size()];
VrMenuControl g_vrMenuWeaponRecoilStrength[
    kWeaponRecoilStrengthPresets.size()];
VrMenuControl g_vrMenuWeaponRecoilRise[
    kWeaponRecoilRisePresets.size()];
VrMenuControl g_vrMenuWeaponRecoilRecovery[
    kWeaponRecoilRecoveryPresets.size()];
VrMenuControl g_vrMenuResetWeaponProfile;
VrMenuControl g_vrMenuRecenter;
VrMenuControl g_vrMenuDefaults;
VrMenuControl g_vrMenuBack;

struct RetailBinding {
    std::uint32_t device;
    std::uint32_t object;
    std::uint32_t command;
    float defaultValue;
    float offset;
    float scale;
    float deadzoneMin;
    float deadzoneMax;
    float deadzoneValue;
    float commandMin;
    float commandMax;
};
static_assert(
    sizeof(RetailBinding) == 44,
    "Retail CBindMgr::SBinding layout changed.");

// The retail VC7.1 VTable groups the RenderCamera overloads differently
// from their declaration order in the public header. The one-argument
// player-camera alias in slot 17 forwards to the exact two-argument
// RenderCamera implementation in slot 19 (call [vtable + 0x4c]).
constexpr std::size_t kRenderPlayerCameraSlot = 17;
constexpr std::size_t kRenderCameraWithOverrideSlot = 19;
// IBase::_InterfaceImplementation is slot 0. IClientShell version 5 places
// OnConsolePrint, key/model and world callbacks before Update; the public
// header therefore puts Update in slot 20.
constexpr std::size_t kClientShellUpdateSlot = 20;
constexpr std::uint32_t kRetailGameClientTimeDateStamp = 0x44EF6B56U;
constexpr std::uint32_t kRetailGameClientSizeOfImage = 0x00315000U;
constexpr std::uintptr_t kRetailMenuSystemInitRva = 0x0010CA90U;
constexpr std::uintptr_t kRetailMenuSystemOnCommandRva = 0x0010D480U;
constexpr std::uintptr_t kRetailMenuSystemOnFocusRva = 0x0010CE00U;
constexpr std::uintptr_t kRetailBaseMenuAddWideControlThunkRva =
    0x00008A58U;
constexpr std::uintptr_t kRetailListGetControlRva = 0x00251440U;
constexpr std::uintptr_t kRetailListSwapItemsRva = 0x00252CE0U;
constexpr std::uintptr_t kRetailListSetSelectionRva = 0x002527E0U;
constexpr std::size_t kRetailMenuListOffset = 0x6E8;
// Retail CBaseMenu::Init writes FontSize / 4 here before marking the list
// dirty. This is CLTGUIListCtrl::m_nItemSpacing in the verified 1.08 build.
constexpr std::size_t kRetailListItemSpacingOffset = 0x54;
constexpr std::size_t kRetailListCurrentIndexOffset = 0x58;
constexpr std::size_t kRetailListFirstShownOffset = 0x5C;
constexpr std::size_t kRetailListLastShownOffset = 0x60;
constexpr std::size_t kRetailListNeedsRecalculationOffset = 0x648;
// CLTGUICtrl::IsEnabled() ist `m_bEnabled && IsVisible()`. Verstecken genügt
// deshalb, damit CLTGUIListCtrl::NextSelection einen Eintrag überspringt; ein
// zusätzliches Enable(false) wäre wirkungslos und würde beim Wiedereinblenden
// statische Controls fälschlich auswählbar machen.
constexpr std::size_t kRetailControlShowVtableSlot = 41;
constexpr std::size_t kRetailMenuOnDownVtableSlot = 23;
constexpr ULONGLONG kTrackedPoseGapGraceMilliseconds = 150;
constexpr std::uintptr_t kRetailWeaponManagerUpdateRva = 0x00078140U;
constexpr std::uintptr_t kRetailSetWeaponTransformRva = 0x00066F90U;
constexpr std::uintptr_t kRetailSetWeaponVisibleRva = 0x00069320U;
constexpr std::uintptr_t kRetailStartMuzzleFlashRva = 0x00069950U;
constexpr std::uintptr_t kRetailGetFireVectorsRva = 0x0006B4A0U;
// ClientWeapon_VectorObjFilterFn first rejects the local player, then replaces
// the broad character proxy with Retails per-skeleton-node intersection.
constexpr std::uintptr_t kRetailVectorObjectFilterRva = 0x00068120U;
constexpr std::uintptr_t kRetailSetTrackedTargetRva = 0x0021EAA0U;
// CAccuracyMgr::Instance jump thunk. The first member is m_fCurrentPerturb.
constexpr std::uintptr_t kRetailAccuracyManagerRva = 0x00009007U;
// Verified Retail CPlayerBodyMgr::Instance returns GameOrig+0x2D7380.
// Its first four members are animation-context pointers, followed by
// m_hPlayerBody at +0x10.
constexpr std::uintptr_t kRetailPlayerBodyManagerRva = 0x002D7380U;
constexpr std::size_t kRetailPlayerBodyObjectOffset = 0x10;
// Strahlbasierte Interaktion. Herleitung und Gegenproben stehen in
// docs/RETAIL-ACTIVATION.md.
constexpr std::uintptr_t kRetailCheckForIntersectRva = 0x001CC150U;
constexpr std::uintptr_t kRetailObjectDetectorUpdateRva = 0x001205A0U;
constexpr std::uintptr_t kRetailPlayerMgrPointerRva = 0x002E2C3CU;
// CPlayerCamera::CalcNoClipPos_WithoutObject. Native VR selects this Retail
// path through CameraCollisionUseObject=0; the hook below retains its six
// anti-clipping rays while excluding character and hitbox objects.
constexpr std::uintptr_t kRetailCalcNoClipRva = 0x00137340U;
constexpr std::size_t kRetailPlayerMgrCameraOffset = 0x28;
// CPlayerMgr::AllowPlayerMovement is a verified Retail function at
// GameOrig+0x146F90. It copies the old byte from +0x88 to +0x89 and stores
// its boolean argument at +0x88.
constexpr std::size_t kRetailPlayerMgrAllowMovementOffset = 0x88;
constexpr std::size_t kRetailPlayerMgrPickupDetectorOffset = 0x3CC;
constexpr std::size_t kRetailCameraObjectOffset = 0x0C;
constexpr std::size_t kRetailCameraPositionOffset = 0x10;
// CheckForIntersect bildet die Blickdrehung als Produkt dieser beiden
// Teilrotationen. Retail 1.08 haelt keinen fertigen Member dafuer bereit.
constexpr std::size_t kRetailCameraRotationSecondOffset = 0x1C;
constexpr std::size_t kRetailCameraRotationFirstOffset = 0xB8;
// m_tfWorld starts at +0xAC and occupies 28 bytes on x86. The following
// member is the animated camera-socket rotation which Retail copies back into
// m_rLocalRotation when an attached camera animation ends.
constexpr std::size_t kRetailCameraTargetAttachRotationOffset = 0xC8;
// `CClientWeapon::GetFireVectors` reads CPlayerCamera::m_eCameraMode at
// +0x114 and compares it with kCM_FirstPerson before choosing its firing
// path. The public 1.08 enum assigns kCM_Cinematic the value 2.
constexpr std::size_t kRetailCameraModeOffset = 0x114;
constexpr std::int32_t kRetailCameraModeCinematic = 2;
// CPlayerCamera::UpdateFirstPerson writes the current PlayerBody camera
// descriptor to +0x190 after comparing it against kAD_CAM_Rotation (0x1D)
// and kAD_CAM_RotationAim (0x1E). Seated/attached sequences such as the
// helicopter can use these descriptors while the camera mode and player
// movement both remain normal first person.
constexpr std::size_t kRetailCameraLastDescriptorOffset = 0x190;
constexpr std::int32_t kRetailCameraDescriptorRotation = 0x1D;
constexpr std::int32_t kRetailCameraDescriptorRotationAim = 0x1E;
constexpr std::size_t kRetailCurrentWeaponOffset = 0x0C;
constexpr std::size_t kRetailRightWeaponModelDataOffset = 0x10;
constexpr std::size_t kRetailLeftWeaponModelDataOffset = 0xC8;
// Retail CClientWeapon starts m_RightHandWeapon at +0x10. Its first LTObjRef
// occupies 16 bytes on x86 (vptr, list links, HOBJECT), putting the model
// HOBJECT at +0x1c and m_hMuzzleSocket at +0x50.
constexpr std::size_t kRetailRightWeaponModelObjectOffset = 0x1C;
constexpr std::size_t kRetailRightWeaponMuzzleSocketOffset = 0x50;
// `CClientWeapon::m_LeftHandWeapon.m_hObject`. Bei Dual Pistols ist dies das
// LDPistol-Modell; normale Einhandwaffen haben kein linkes HHModel.
// SetVisible liest das Objekt im geprueften Code aus `[esi+0xD4]`.
constexpr std::size_t kRetailLeftWeaponModelObjectOffset = 0xD4;
// `CClientWeapon::m_bFireLeftHand`. Retails FIRE-Modellkey setzt dieses Byte
// auf LEFT oder RIGHT; GetFireVectors liest es vor der Modellwahl.
constexpr std::size_t kRetailWeaponFireLeftHandOffset = 0x1E3;
constexpr std::size_t kRetailGetFireVectorsFireHandProbeOffset = 0x3D8;
// `CClientWeapon::m_bDisabled`. Retail schaltet die Waffe darueber ab, sobald
// sie nicht in der Hand liegt — an der Leiter (`LadderMgr` ruft beim Aufstieg
// `CClientWeaponMgr::DisableWeapons`), in Zwischensequenzen und am Geschuetz.
// Das Modell ist dann unsichtbar, und der Zielstrahl darf es auch nicht mehr
// geben. Belegt aus `SetVisible`, das an dieser Stelle `m_bVisible` setzt und
// bei gesetztem Flag sofort zurueckkehrt.
constexpr std::size_t kRetailWeaponDisabledOffset = 0x223;
constexpr std::size_t kRetailSetWeaponVisibleDisabledProbeOffset = 0x18;
// Leiterzustand. `LadderMgr::Instance()` ist ein Magic-Static-Accessor: Er
// gibt die Adresse eines statischen Objekts zurueck, dessen erster Member
// `m_pLadder` ist — `IsClimbing()` prueft nur, ob dieser Zeiger gesetzt ist.
// Belegt an zwei Stellen in GameOrig.dll: Der Accessor endet mit
// `mov eax, <Objekt>` / `ret`, und die Aufrufstelle in CMoveMgr, die
// „%s - jump from ladder" protokolliert, liest unmittelbar danach
// `cmp dword ptr [eax], 0`. `CanReleaseLadder` beginnt mit demselben
// `cmp dword ptr [ecx], 0`, womit der Offset 0 doppelt belegt ist.
constexpr std::uintptr_t kRetailLadderInstanceRva = 0x00027B50U;
constexpr std::uintptr_t kRetailLadderObjectRva = 0x002D7AA8U;
// mov cl, byte ptr [guard] — die vier Adressbytes werden beim Laden
// relokiert und deshalb nicht mitgeprueft.
constexpr unsigned char kRetailLadderInstancePrefix[] = {0x8A, 0x0D};
// mov eax, 1 / test al, cl / jne
constexpr unsigned char kRetailLadderInstanceGuard[] = {
    0xB8, 0x01, 0x00, 0x00, 0x00, 0x84, 0xC8, 0x75};
// Offset des abschliessenden `mov eax, <Objekt>` / `ret` im Accessor.
constexpr std::size_t kRetailLadderInstanceReturnOffset = 0x34;
// CMoveMgr. Der globale Zeiger wird im Konstruktor gesetzt und im
// registrierten `PlayerLeash`-Konsolenprogramm zweimal geladen. Weitere,
// unabhaengige Proben belegen die hier gelesenen Zustandsfelder.
constexpr std::uintptr_t kRetailMoveManagerPointerRva = 0x002D8D8CU;
constexpr std::uintptr_t kRetailMoveManagerPointerProbeRva = 0x0007AB00U;
constexpr std::uintptr_t kRetailMoveFallingProbeRva = 0x00100179U;
constexpr std::uintptr_t kRetailMoveOnGroundProbeRva = 0x000230BCU;
constexpr std::uintptr_t kRetailMoveJumpedProbeRva = 0x001320F6U;
constexpr std::uintptr_t kRetailPostureDownProbeRva = 0x0007FDE6U;
constexpr std::uintptr_t kRetailPostureDownVarTrackRva = 0x002D8F04U;
constexpr std::size_t kRetailMoveControlFlagsOffset = 0x28;
constexpr std::size_t kRetailMoveOnGroundOffset = 0x64;
constexpr std::size_t kRetailMoveFallingOffset = 0x66;
constexpr std::size_t kRetailMoveJumpedOffset = 0x78;
// CMoveMgr's constructor stores the newly allocated CVehicleMgr at +0x3D8.
// CVehicleMgr::m_ePPhysicsModel is the dword at +0x1C; PPM_LURE (1) binds
// the player to a scripted moving lure such as a vehicle/passenger seat.
constexpr std::size_t kRetailMoveVehicleManagerOffset = 0x3D8;
constexpr std::size_t kRetailVehiclePhysicsModelOffset = 0x1C;
constexpr std::int32_t kRetailPlayerPhysicsModelLure = 1;
constexpr std::uint32_t kRetailControlFlagForward = 1U << 0;
constexpr std::uint32_t kRetailControlFlagDuck = 1U << 5;
constexpr std::uint32_t kRetailControlFlagRun = 1U << 9;
constexpr int kRetailTrackerGroupAimAt = 1;
// UI-Zustand fuer den Flachbildmodus. Herleitung in
// docs/RETAIL-ACTIVATION.md: `g_pInterfaceMgr` liegt als Zeiger bei
// GameOrig+0x2E1BAC, `m_eGameState` als dword bei +0x08.
constexpr std::uintptr_t kRetailInterfaceMgrPointerRva = 0x002E1BACU;
constexpr std::size_t kRetailInterfaceMgrGameStateOffset = 0x08;
// CPlayerStats/WeaponDB/IDatabaseMgr-Zugriffe fuer das Wrist-HUD. Alle
// Adressen und Layouts sind gegen Retail 1.08 (GameOrig.dll) geprueft:
// CPlayerStats besitzt einen vptr bei +0, seine Statusfelder beginnen bei +4.
constexpr std::uintptr_t kRetailPlayerStatsPointerRva = 0x002E31B8U;
constexpr std::uintptr_t kRetailWeaponDatabasePointerRva = 0x002E66CCU;
constexpr std::uintptr_t kRetailDatabasePointerRva = 0x002E66A8U;
constexpr std::uintptr_t kRetailGetAmmoCountRva = 0x0000A60AU;
constexpr std::uintptr_t kRetailGetWeaponDataRva = 0x0020FFB0U;
constexpr std::uintptr_t kRetailGetPlayerGrenadeRva = 0x0020FA80U;
constexpr std::uintptr_t kRetailGetGearRecordRva = 0x0020F650U;
constexpr std::uintptr_t kRetailGetGearCountRva = 0x0001252BU;
constexpr std::size_t kRetailPlayerStatsHealthOffset = 0x04;
constexpr std::size_t kRetailPlayerStatsArmorOffset = 0x08;
constexpr std::size_t kRetailPlayerStatsMaxHealthOffset = 0x0C;
constexpr std::size_t kRetailPlayerStatsMaxArmorOffset = 0x10;
constexpr std::size_t kRetailPlayerStatsAirPercentOffset = 0x14;
constexpr std::size_t kRetailPlayerStatsCurrentAmmoOffset = 0x2C;
// Ladestelle des Zeigers in CInterfaceResMgr::DrawScreen. Belegt, dass die
// RVA in dieser Binary wirklich der Interface-Manager ist, und liefert die
// relozierte Adresse.
constexpr std::uintptr_t kRetailInterfaceMgrLoadSiteRva = 0x000FE51CU;
// `cmp dword ptr [ecx+8], GS_MENU; setne al; ret` — der kleinste Beweis
// dafuer, dass der Spielzustand als dword bei +0x08 steht.
constexpr std::uintptr_t kRetailGameStateMenuTestRva = 0x000EF900U;
// `mov eax,[ecx+8]; cmp eax, GS_MOVIE; ja ...` — Sprungtabelle ueber genau
// zehn Zustaende, also dieselbe Enum-Reihenfolge wie im SDK.
constexpr std::uintptr_t kRetailGameStateSwitchRva = 0x000F1F20U;
// GS_UNDEFINED=0, GS_PLAYING=1, GS_EXITINGLEVEL=2, GS_LOADINGLEVEL=3,
// GS_SPLASHSCREEN=4, GS_MENU=5, GS_SCREEN=6, GS_PAUSED=7, GS_DEMOSCREEN=8,
// GS_MOVIE=9. Nur GS_PLAYING rendert die Welt; alles andere ist Flachbild.
constexpr int kRetailGameStatePlaying = 1;
constexpr int kRetailGameStateExitingLevel = 2;
constexpr int kRetailGameStateLoadingLevel = 3;
constexpr int kRetailGameStateCount = 10;
constexpr unsigned char kRetailPlayerCameraForwarder[] = {
    0x8B, 0x54, 0x24, 0x04, // mov edx,[esp+4]
    0x8B, 0x01,             // mov eax,[ecx]
    0x6A, 0x00,             // push 0 (technique override)
    0x52,                   // push edx (camera)
    0xFF, 0x50, 0x4C,       // call [eax+0x4c] (slot 19)
    0xC2, 0x04, 0x00        // ret 4
};
constexpr unsigned char kRetailMenuInitPrefix[] = {
    0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xF1, 0x50,
    0xC7, 0x86, 0xF0, 0x05, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00};
constexpr unsigned char kRetailMenuOnCommandPrefix[] = {
    0x83, 0xEC, 0x2C, 0x53, 0x56, 0x8B, 0xF1, 0x8B,
    0x4C, 0x24, 0x38, 0x8D, 0x41, 0xFB, 0x83, 0xF8,
    0x0E};
constexpr unsigned char kRetailMenuOnFocusPrefix[] = {
    0x81, 0xEC, 0x1C, 0x02, 0x00, 0x00, 0x55, 0x56,
    0x8B, 0xF1};
constexpr unsigned char kRetailListGetControlPrefix[] = {
    0x8B, 0x91, 0x50, 0x06, 0x00, 0x00, 0x85, 0xD2,
    0x74, 0x13};
constexpr unsigned char kRetailListSwapItemsPrefix[] = {
    0x8B, 0x91, 0x50, 0x06, 0x00, 0x00, 0x85, 0xD2,
    0x74, 0x71, 0x8B, 0x81, 0x54, 0x06, 0x00, 0x00};

enum VrMenuCommand : std::uint32_t {
    kVrMenuOpen = 0x56520001U,
    kVrMenuOpenDisplay,
    kVrMenuOpenComfort,
    kVrMenuOpenControls,
    kVrMenuOpenWeapons,
    kVrMenuOpenWeaponHandling,
    kVrMenuOpenWeaponWeight,
    kVrMenuOpenWeaponRecoil,
    kVrMenuOpenMelee,
    kVrMenuOpenAdvanced,
    kVrMenuToggleStereo,
    kVrMenuToggleTranslation,
    kVrMenuToggleStereoHud,
    kVrMenuToggleHeadBob,
    kVrMenuToggleComfort,
    kVrMenuCycleWeaponWeightPreset,
    kVrMenuToggleAimGuide,
    kVrMenuToggleHaptics,
    kVrMenuCycleFlashlightMount,
    kVrMenuToggleHandedness,
    kVrMenuToggleHeadRelativeMovement,
    kVrMenuToggleClimbing,
    kVrMenuTogglePhysicalLean,
    kVrMenuTogglePhysicalDuck,
    kVrMenuCycleLeanScale,
    kVrMenuToggleMelee,
    kVrMenuToggleMeleeWeaponStrike,
    kVrMenuToggleMeleeOffHandStrike,
    kVrMenuToggleMeleeJumpKick,
    kVrMenuToggleMeleeSlideKick,
    kVrMenuToggleArms,
    kVrMenuToggleTwoHandGrip,
    kVrMenuToggleWeaponWeight,
    kVrMenuToggleWeaponRecoil,
    kVrMenuToggleWeaponWeightDiagnostics,
    kVrMenuToggleWeaponProfileTarget,
    kVrMenuCycleFovScale,
    kVrMenuCycleTurnSpeed,
    kVrMenuCycleWeaponWeight,
    kVrMenuCycleWeaponPositionFollow,
    kVrMenuCycleWeaponRotationFollow,
    kVrMenuCycleWeaponCatchUp,
    kVrMenuCycleWeaponRecoilStrength,
    kVrMenuCycleWeaponRecoilRise,
    kVrMenuCycleWeaponRecoilRecovery,
    kVrMenuResetWeaponProfile,
    kVrMenuRecenter,
    kVrMenuDefaults,
    kVrMenuBack,
};
static_assert(
    0x4C / sizeof(void*) == kRenderCameraWithOverrideSlot,
    "Retail RenderCamera forwarding slot changed.");

void Report(const char* level, const char* event,
            const char* message) noexcept {
    if (g_reportHookStatus != nullptr) {
        g_reportHookStatus(level, event, message);
    }
    OutputDebugStringA("F.E.A.R. VR: ");
    OutputDebugStringA(event);
    OutputDebugStringA(": ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

bool IsExecutableAddress(const void* address) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr ||
        VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT) {
        return false;
    }
    const DWORD protection =
        info.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    return protection == PAGE_EXECUTE ||
           protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

// HDTextures4FEAR/XP v2.0.2 maps the patched FEAR.exe image as
// PAGE_WRITECOPY and runs the process with DEP disabled. Its renderer
// functions are therefore callable even though VirtualQuery does not report
// a PAGE_EXECUTE_* flag. Accept that legacy mapping only inside the main image
// and only while DEP is actually disabled; every DLL and DEP-enabled process
// still requires normal executable protection.
bool IsCallableMainImageAddress(const void* address) noexcept {
    if (IsExecutableAddress(address)) {
        return true;
    }
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr ||
        VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT ||
        info.Type != MEM_IMAGE ||
        info.AllocationBase != GetModuleHandleW(nullptr)) {
        return false;
    }
    DWORD depFlags = 0;
    BOOL depPermanent = FALSE;
    if (!GetProcessDEPPolicy(
            GetCurrentProcess(), &depFlags, &depPermanent) ||
        (depFlags & PROCESS_DEP_ENABLE) != 0) {
        return false;
    }
    const DWORD protection =
        info.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    return protection == PAGE_READONLY ||
           protection == PAGE_READWRITE ||
           protection == PAGE_WRITECOPY;
}

const unsigned char* ResolveRelativeBranch(
    const unsigned char* instruction) noexcept {
    if (!IsExecutableAddress(instruction) ||
        (instruction[0] != 0xE8 &&
         instruction[0] != 0xE9)) {
        return nullptr;
    }
    std::int32_t displacement = 0;
    std::memcpy(
        &displacement, instruction + 1, sizeof(displacement));
    const unsigned char* const resolved =
        instruction + 5 + displacement;
    return IsExecutableAddress(resolved) ? resolved : nullptr;
}

const unsigned char* ResolveCodeTarget(
    const unsigned char* target) noexcept {
    const unsigned char* current = target;
    for (int depth = 0; depth < 4; ++depth) {
        if (current == nullptr || current[0] != 0xE9) {
            break;
        }
        const unsigned char* const resolved =
            ResolveRelativeBranch(current);
        if (resolved == nullptr || resolved == current) {
            break;
        }
        current = resolved;
    }
    return current;
}

bool MatchesCode(
    const unsigned char* target,
    const unsigned char* expected,
    std::size_t size) noexcept {
    if (!IsExecutableAddress(target) || expected == nullptr) {
        return false;
    }
    __try {
        return std::memcmp(target, expected, size) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

const unsigned char* FindRetailGetBindingValue(
    const void* target) noexcept {
    const auto* updateThunk =
        static_cast<const unsigned char*>(target);
    const unsigned char* const update =
        ResolveCodeTarget(updateThunk);

    constexpr unsigned char kBindMgrCalls[] = {
        0xE8, 0x00, 0x00, 0x00, 0x00,
        0x8B, 0xC8, 0xE8
    };
    if (!IsExecutableAddress(update)) {
        return nullptr;
    }
    __try {
        if (update[0x7B] != kBindMgrCalls[0] ||
            update[0x80] != kBindMgrCalls[5] ||
            update[0x81] != kBindMgrCalls[6] ||
            update[0x82] != kBindMgrCalls[7]) {
            return nullptr;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }

    const unsigned char* const bindUpdateBranch =
        ResolveRelativeBranch(update + 0x82);
    const unsigned char* const bindUpdate =
        ResolveCodeTarget(bindUpdateBranch);
    constexpr unsigned char kBindUpdatePrefix[] = {
        0x83, 0xEC, 0x1C, 0x56, 0x8B, 0xF1,
        0x8A, 0x46, 0x4C, 0x84, 0xC0, 0x0F, 0x84
    };
    constexpr unsigned char kGetBindingCallSite[] = {
        0x6A, 0x00, 0x53, 0x8B, 0xCE, 0xE8
    };
    if (!MatchesCode(
            bindUpdate, kBindUpdatePrefix,
            sizeof(kBindUpdatePrefix)) ||
        !MatchesCode(
            bindUpdate + 0xD4, kGetBindingCallSite,
            sizeof(kGetBindingCallSite))) {
        return nullptr;
    }

    return ResolveCodeTarget(
        ResolveRelativeBranch(bindUpdate + 0xD9));
}

bool MatchesRetailPlayerCameraForwarder(const void* target) noexcept {
    if (!IsCallableMainImageAddress(target)) {
        return false;
    }
    __try {
        return std::memcmp(
                   target, kRetailPlayerCameraForwarder,
                   sizeof(kRetailPlayerCameraForwarder)) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void ReportRendererHookLayout(
    const void* playerTarget,
    const void* overrideTarget,
    bool playerMatches,
    bool overrideExecutable) noexcept {
    unsigned char bytes[24]{};
    char byteText[sizeof(bytes) * 3 + 1]{};
    char modulePath[MAX_PATH]{"<unknown>"};
    MEMORY_BASIC_INFORMATION info{};
    MEMORY_BASIC_INFORMATION overrideInfo{};
    bool readable = false;
    __try {
        std::memcpy(bytes, playerTarget, sizeof(bytes));
        readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    if (readable) {
        std::size_t offset = 0;
        for (unsigned char byte : bytes) {
            const int written = std::snprintf(
                byteText + offset, sizeof(byteText) - offset,
                offset == 0 ? "%02X" : " %02X",
                static_cast<unsigned>(byte));
            if (written <= 0) {
                break;
            }
            offset += static_cast<std::size_t>(written);
        }
    } else {
        std::snprintf(byteText, sizeof(byteText), "<unreadable>");
    }
    if (playerTarget != nullptr &&
        VirtualQuery(playerTarget, &info, sizeof(info)) == sizeof(info) &&
        info.AllocationBase != nullptr) {
        GetModuleFileNameA(
            static_cast<HMODULE>(info.AllocationBase),
            modulePath, static_cast<DWORD>(std::size(modulePath)));
    }
    VirtualQuery(
        overrideTarget, &overrideInfo, sizeof(overrideInfo));
    char message[768]{};
    std::snprintf(
        message, sizeof(message),
        "slot17=%p match=%u protect=0x%lX slot19=%p executable=%u "
        "protect=0x%lX module=%s bytes=%s",
        playerTarget, playerMatches ? 1U : 0U,
        static_cast<unsigned long>(info.Protect),
        overrideTarget, overrideExecutable ? 1U : 0U,
        static_cast<unsigned long>(overrideInfo.Protect),
        modulePath, byteText);
    Report("ERROR", "stereo_hook_layout_probe", message);
}

bool CommandLineContains(const wchar_t* option) noexcept {
    const wchar_t* const commandLine = GetCommandLineW();
    return commandLine != nullptr && option != nullptr &&
           std::wcsstr(commandLine, option) != nullptr;
}

constexpr const char* kHeadBobAmplitudeVariables[] = {
    "HeadBobCameraOffsetXAmp",
    "HeadBobCameraOffsetYAmp",
    "HeadBobCameraOffsetZAmp",
    "HeadBobCameraRotationXAmp",
    "HeadBobCameraRotationYAmp",
    "HeadBobCameraRotationZAmp",
    "HeadBobWeaponOffsetXAmp",
    "HeadBobWeaponOffsetYAmp",
    "HeadBobWeaponOffsetZAmp",
    "HeadBobWeaponRotationXAmp",
    "HeadBobWeaponRotationYAmp",
    "HeadBobWeaponRotationZAmp",
};
constexpr std::size_t kHeadBobCameraAmplitudeCount = 6;

bool CaptureHeadBobOriginals() noexcept {
    if (g_headBobOriginalKnown) {
        return true;
    }
    if (g_client == nullptr) {
        return false;
    }
    __try {
        const HCONSOLEVAR scale =
            g_client->GetConsoleVariable("HeadBob");
        const HCONSOLEVAR debug =
            g_client->GetConsoleVariable("HeadBobDebugMode");
        if (scale == nullptr || debug == nullptr) {
            return false;
        }
        g_headBobOriginalScale =
            g_client->GetConsoleVariableFloat(scale);
        g_headBobOriginalDebugMode =
            g_client->GetConsoleVariableFloat(debug);
        for (std::size_t index = 0;
             index < std::size(kHeadBobAmplitudeVariables);
             ++index) {
            const HCONSOLEVAR variable = g_client->GetConsoleVariable(
                kHeadBobAmplitudeVariables[index]);
            if (variable == nullptr) {
                return false;
            }
            g_headBobOriginalAmplitudes[index] =
                g_client->GetConsoleVariableFloat(variable);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    g_headBobOriginalKnown = true;
    return true;
}

// Setzt eine Retail-Konsolenvariable und merkt sich den ersten Fehlschlag.
// Ohne den Namen ist im Log nicht erkennbar, welche Variable fehlte.
void SetRetailFloatVariable(
    const char* name, float value, bool& configured,
    const char*& firstFailure) noexcept {
    if (g_client->SetConsoleVariableFloat(name, value) != LT_OK) {
        configured = false;
        if (firstFailure == nullptr) {
            firstFailure = name;
        }
    }
}

bool ApplyHeadBobEnabled(bool enabled) noexcept {
    const bool originalsKnown = CaptureHeadBobOriginals();
    if (enabled && !originalsKnown) {
        return false;
    }
    bool configured = true;
    const char* firstFailure = nullptr;
    __try {
        // Retail's Walk/Run profiles come from the client database. Their
        // amplitudes ignore the debug CVars unless HeadBobDebugMode is active,
        // but every profile is multiplied by this global HeadBob scale.
        SetRetailFloatVariable(
            "HeadBob", enabled ? g_headBobOriginalScale : 0.0F,
            configured, firstFailure);
        // WeaponLagEnabled adds another artificial node rotation derived from
        // camera motion. The OpenXR controller is already the authoritative
        // weapon pose, so the Retail lag must never be layered on top.
        SetRetailFloatVariable(
            "WeaponLagEnabled", 0.0F, configured, firstFailure);
        // The VR flashlight is permanently available and follows the left
        // controller, so Retail battery drain and locomotion-driven waver are
        // not useful here.
        SetRetailFloatVariable(
            "FlashlightBattery", 0.0F, configured, firstFailure);
        SetRetailFloatVariable(
            "FlashlightWaverSpeedScale", 0.0F, configured, firstFailure);
        // Frueher ueberschrieb diese Zuweisung `configured`, statt sie zu
        // verknuepfen. Ein fehlgeschlagenes WeaponLagEnabled=0 blieb dadurch
        // unbemerkt: Die Funktion meldete Erfolg, setzte
        // g_stableWeaponMotionConfigured und versuchte es nie wieder. Die
        // Retail-Waffenverzoegerung blieb dann die ganze Sitzung aktiv.
        SetRetailFloatVariable(
            "HeadBobDebugMode",
            enabled ? g_headBobOriginalDebugMode : 1.0F,
            configured, firstFailure);
        for (std::size_t index = 0;
             index < std::size(kHeadBobAmplitudeVariables);
             ++index) {
            // In VR the visible weapon follows the tracked controller. Retail
            // weapon bob would add a second, artificial motion on top and make
            // aiming while walking unnecessarily unstable. Camera bob remains
            // optional, but weapon offsets and rotations are always suppressed.
            const bool restoreCameraAmplitude =
                enabled && index < kHeadBobCameraAmplitudeCount;
            SetRetailFloatVariable(
                kHeadBobAmplitudeVariables[index],
                restoreCameraAmplitude
                    ? g_headBobOriginalAmplitudes[index]
                    : 0.0F,
                configured, firstFailure);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        configured = false;
    }
    if (configured) {
        g_headBobEnabled = enabled;
        g_stableWeaponMotionConfigured = true;
    }
    if (configured) {
        Report(
            "INFO", enabled ? "headbob_enabled" : "headbob_disabled",
            enabled
                ? "Retail camera head bob was restored; weapon head bob "
                  "remains disabled for stable VR aiming."
                : "Retail Walk/Run head bob and weapon lag are disabled; "
                  "the OpenXR controller is the sole weapon motion source.");
    } else {
        char message[160];
        _snprintf_s(
            message, sizeof(message), _TRUNCATE,
            "The Retail head-bob variables could not be changed; "
            "first rejected variable: %s",
            firstFailure != nullptr ? firstFailure : "<unknown>");
        Report("WARN", "headbob_configuration_failed", message);
    }
    return configured;
}

void ConfigureComfortOptions() noexcept {
    // A steady view and weapon are the safe VR default. A persisted HeadBob=1
    // may still opt into camera motion; weapon bob remains disabled separately.
    g_forceHeadBobDisabled =
        CommandLineContains(L"-fearvr-no-headbob");
    const bool safeMode = CommandLineContains(L"-fearvr-safe");
    g_disableFlashlight =
        safeMode || CommandLineContains(L"-fearvr-no-flashlight");
    g_disableHandNodes =
        safeMode || CommandLineContains(L"-fearvr-no-handnodes");
    g_disableWeaponTransform =
        safeMode || CommandLineContains(L"-fearvr-no-weapon-transform");
    g_disableBodyPieceHiding =
        safeMode || CommandLineContains(L"-fearvr-no-body-hide");
    g_disableStereoRender =
        CommandLineContains(L"-fearvr-no-stereo");
    g_disableCommandInjection =
        CommandLineContains(L"-fearvr-no-inject");
    // Der Bindungs-Hook ist als Absturzursache ausgeschlossen: Ohne ihn ist
    // das Spiel an derselben Stelle weiterhin abgestuerzt. Er bleibt aktiv,
    // zumal die Kommando-Injektion aus ihm heraus aufgerufen wird und ohne
    // ihn gar keine Controllereingabe mehr ankaeme.
    g_disableBindingHook =
        CommandLineContains(L"-fearvr-no-binding-hook");
    g_disableClientUpdateWork =
        CommandLineContains(L"-fearvr-no-client-update");
    g_disableAimHooks =
        CommandLineContains(L"-fearvr-no-aim-hooks");
    g_disableInteractionHooks =
        safeMode || CommandLineContains(L"-fearvr-no-interaction");
    // Notausstieg zurueck auf die alte Heuristik, falls das Lesen des
    // Retail-Spielzustands je Aerger macht.
    g_disableRetailGameState =
        CommandLineContains(L"-fearvr-no-gamestate");
    // Der AimAt-Hook wird NICHT mehr installiert.
    //
    // Belegt am 25.07.2026: An einer geskripteten Szene, in der ein NPC
    // spawnt, stuerzt das Spiel reproduzierbar mit einem Sprung auf Adresse 0
    // ab, sobald dieser Detour gesetzt ist. Entscheidend war der Lauf mit
    // -fearvr-aimat-passthrough: Der Hook war installiert, reichte aber jeden
    // Aufruf unveraendert durch, und es stuerzte trotzdem ab. Es liegt also
    // am Patchen dieser Funktion selbst, nicht an unserem Zielwert. Ohne den
    // Hook laeuft dieselbe Szene durch.
    //
    // Kosten: Oberkoerper und Kopf des Spielerkoerpers drehen nicht mehr in
    // die Zielrichtung nach. Waffenhaltung, roter Zielstrahl, Fire-Vectors und
    // Trefferpunkt sind nicht betroffen.
    g_disableAimAtHook =
        !CommandLineContains(L"-fearvr-aimat");
    g_aimAtPassthrough =
        CommandLineContains(L"-fearvr-aimat-passthrough");
    if (g_disableFlashlight || g_disableHandNodes ||
        g_disableWeaponTransform || g_disableBodyPieceHiding ||
        g_disableStereoRender || g_disableCommandInjection ||
        g_disableBindingHook || g_disableClientUpdateWork ||
        g_disableAimHooks || g_disableAimAtHook || g_aimAtPassthrough) {
        char message[320];
        _snprintf_s(
            message, sizeof(message), _TRUNCATE,
            "flashlight=%d hand_nodes=%d weapon_transform=%d body_hide=%d "
            "stereo=%d inject=%d binding_hook=%d client_update=%d "
            "aim_hooks=%d aimat=%d aimat_passthrough=%d",
            g_disableFlashlight ? 0 : 1, g_disableHandNodes ? 0 : 1,
            g_disableWeaponTransform ? 0 : 1,
            g_disableBodyPieceHiding ? 0 : 1,
            g_disableStereoRender ? 0 : 1,
            g_disableCommandInjection ? 0 : 1,
            g_disableBindingHook ? 0 : 1,
            g_disableClientUpdateWork ? 0 : 1,
            g_disableAimHooks ? 0 : 1,
            g_disableAimAtHook ? 0 : 1,
            g_aimAtPassthrough ? 1 : 0);
        Report("WARN", "vr_features_disabled", message);
    }
    g_headBobEnabled = false;
    g_stableWeaponMotionConfigured = false;
    CaptureHeadBobOriginals();
    ApplyHeadBobEnabled(false);
}

bool ReadCommandLineValue(
    const wchar_t* option, wchar_t* output,
    std::size_t outputCount) noexcept {
    if (option == nullptr || output == nullptr || outputCount == 0) {
        return false;
    }
    output[0] = L'\0';
    const wchar_t* commandLine = GetCommandLineW();
    const wchar_t* cursor =
        commandLine == nullptr ? nullptr : std::wcsstr(commandLine, option);
    if (cursor == nullptr) {
        return false;
    }
    cursor += std::wcslen(option);
    while (*cursor == L' ' || *cursor == L'\t') {
        ++cursor;
    }
    const bool quoted = *cursor == L'"';
    if (quoted) {
        ++cursor;
    }
    std::size_t length = 0;
    while (*cursor != L'\0' &&
           ((quoted && *cursor != L'"') ||
            (!quoted && *cursor != L' ' && *cursor != L'\t')) &&
           length + 1 < outputCount) {
        output[length++] = *cursor++;
    }
    output[length] = L'\0';
    return length != 0;
}

void LocateVrSettingsFile() noexcept {
    wchar_t directory[MAX_PATH]{};
    if (!ReadCommandLineValue(
            L"-userdirectory", directory, std::size(directory))) {
        HMODULE self = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&LocateVrSettingsFile),
                &self)) {
            const DWORD length =
                GetModuleFileNameW(self, directory, MAX_PATH);
            if (length != 0 && length < MAX_PATH) {
                wchar_t* separator = std::wcsrchr(directory, L'\\');
                if (separator != nullptr) {
                    *separator = L'\0';
                }
            }
        }
    }
    if (directory[0] == L'\0') {
        return;
    }
    _snwprintf_s(
        g_vrSettingsPath, std::size(g_vrSettingsPath),
        _TRUNCATE, L"%s\\fearvr.ini", directory);
    g_vrSettingsFilePresent =
        GetFileAttributesW(g_vrSettingsPath) != INVALID_FILE_ATTRIBUTES;
}

int ReadVrSetting(const wchar_t* name, int fallback) noexcept {
    if (!g_vrSettingsFilePresent ||
        g_vrSettingsPath[0] == L'\0') {
        return fallback;
    }
    return GetPrivateProfileIntW(
        L"VR", name, fallback, g_vrSettingsPath);
}

float ReadVrFloat(
    const wchar_t* section, const wchar_t* name, float fallback) noexcept {
    if (!g_vrSettingsFilePresent || g_vrSettingsPath[0] == L'\0') {
        return fallback;
    }
    wchar_t fallbackText[32]{};
    wchar_t valueText[32]{};
    _snwprintf_s(
        fallbackText, std::size(fallbackText), _TRUNCATE,
        L"%.6g", static_cast<double>(fallback));
    GetPrivateProfileStringW(
        section, name, fallbackText, valueText,
        static_cast<DWORD>(std::size(valueText)), g_vrSettingsPath);
    wchar_t* end = nullptr;
    const float value = std::wcstof(valueText, &end);
    return end != valueText && std::isfinite(value) ? value : fallback;
}

void WriteVrSetting(const wchar_t* name, int value) noexcept;
void WriteVrFloat(
    const wchar_t* section, const wchar_t* name, float value) noexcept;
void SaveActiveWeaponWeightProfile() noexcept;
void SaveActiveWeaponRecoilProfile() noexcept;
void SaveActiveWeaponCollisionProfile() noexcept;

bool QueryBooleanOption(
    GetBooleanOptionFunction getter, bool fallback) noexcept {
    if (getter == nullptr) {
        return fallback;
    }
    __try {
        return getter() != FALSE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return fallback;
    }
}

void SetBooleanOption(
    SetBooleanOptionFunction setter, bool enabled) noexcept {
    if (setter == nullptr) {
        return;
    }
    __try {
        setter(enabled ? TRUE : FALSE);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

int FovScalePresetForPercent(int percent) noexcept {
    const int clamped = std::clamp(
        percent,
        static_cast<int>(FEARVR_FOV_SCALE_MIN_PERCENT),
        static_cast<int>(FEARVR_FOV_SCALE_MAX_PERCENT));
    return std::clamp((clamped - 100 + 5) / 10, 0, 3);
}

std::uint32_t CurrentFovScalePercent() noexcept {
    const int preset = std::clamp(g_fovScalePreset, 0, 3);
    return kFovScalePercents[static_cast<std::size_t>(preset)];
}

void ApplyFovScalePreset() noexcept {
    if (g_setFovScalePercent == nullptr) {
        return;
    }
    __try {
        g_setFovScalePercent(CurrentFovScalePercent());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

std::uint32_t QueryRenderScalePercent() noexcept {
    if (g_getRenderScalePercent == nullptr) {
        return 100U;
    }
    __try {
        return std::clamp(
            g_getRenderScalePercent(), 100U, 200U);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 100U;
    }
}

void ApplyRenderScalePercent() noexcept {
    if (g_setRenderScalePercent == nullptr) {
        return;
    }
    __try {
        g_setRenderScalePercent(g_renderScalePercent);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void InitializeVrSettings() noexcept {
    LocateVrSettingsFile();
    g_renderScalePercent = QueryRenderScalePercent();
    if (!g_vrSettingsFilePresent) {
        ApplyFovScalePreset();
        return;
    }

    g_weaponWeightDiagnosticsEnabled =
        ReadVrSetting(L"WeaponWeightDiagnostics", 0) != 0;
    g_weaponRecoilEnabled =
        ReadVrSetting(L"WeaponRecoilEnabled", 1) != 0;
    g_weaponRecoilProfile = SanitizeWeaponRecoilProfile({
        ReadVrFloat(L"VR", L"WeaponRecoilStrength", 1.5F),
        ReadVrFloat(L"VR", L"WeaponRecoilMuzzleRise", 1.0F),
        ReadVrFloat(L"VR", L"WeaponRecoilRecovery", 1.0F)});
    g_weaponCollisionEnabled =
        ReadVrSetting(L"WeaponCollisionEnabled", 1) != 0;
    g_weaponCollisionDebugEnabled =
        ReadVrSetting(L"WeaponCollisionDebug", 1) != 0;
    g_defaultWeaponCollisionProfile = SanitizeWeaponCollisionProfile({
        ReadVrFloat(L"VR", L"WeaponCollisionLengthScale", 1.0F),
        ReadVrFloat(L"VR", L"WeaponCollisionWidth", 0.10F),
        ReadVrFloat(L"VR", L"WeaponCollisionHeight", 0.18F),
        ReadVrFloat(L"VR", L"WeaponCollisionForwardOffset", 0.0F)});
    g_defaultWeaponWeightProfile = SanitizeWeaponWeightProfile({
        ReadVrFloat(L"VR", L"WeaponWeight", 1.0F),
        ReadVrFloat(L"VR", L"WeaponPositionalFollow", 18.0F),
        ReadVrFloat(L"VR", L"WeaponRotationalFollow", 20.0F),
        ReadVrFloat(L"VR", L"WeaponCatchUpStrength", 1.5F)});
    const bool legacyWeaponWeightEnabled =
        ReadVrSetting(L"WeaponWeightEnabled", 0) != 0;
    const int configuredWeaponWeightPreset =
        ReadVrSetting(L"WeaponWeightPreset", -1);
    g_weaponWeightPreset = configuredWeaponWeightPreset < 0
        ? (legacyWeaponWeightEnabled
               ? WeaponWeightPreset::medium
               : WeaponWeightPreset::none)
        : SanitizeWeaponWeightPreset(configuredWeaponWeightPreset);
    g_weaponWeightEnabled =
        g_weaponWeightPreset != WeaponWeightPreset::none;
    g_armIkTuning = SanitizeArmIkTuning({
        ReadVrFloat(L"VR", L"IkElbowOutward", 1.0F),
        ReadVrFloat(L"VR", L"IkElbowDown", 0.45F),
        ReadVrFloat(L"VR", L"IkElbowBack", 0.15F),
        ReadVrSetting(L"IkElbowContinuity", 1) != 0,
        ReadVrFloat(L"VR", L"IkLeftHandRight", 0.0F),
        ReadVrFloat(L"VR", L"IkLeftHandUp", 0.0F),
        ReadVrFloat(L"VR", L"IkLeftHandForward", 0.0F),
        ReadVrFloat(L"VR", L"IkLeftHandPitch", 0.0F),
        ReadVrFloat(L"VR", L"IkLeftHandYaw", 0.0F),
        ReadVrFloat(L"VR", L"IkLeftHandRoll", 0.0F)});
    SetBooleanOption(
        g_setStereoEnabled,
        ReadVrSetting(
            L"Stereo",
            QueryBooleanOption(g_isStereoEnabled, true) ? 1 : 0) != 0);
    SetBooleanOption(
        g_setTranslationEnabled,
        ReadVrSetting(
            L"Translation",
            QueryBooleanOption(g_isTranslationEnabled, false) ? 1 : 0) != 0);
    SetBooleanOption(
        g_setStereoHudEnabled,
        ReadVrSetting(
            L"StereoHUD",
            QueryBooleanOption(g_isStereoHudEnabled, true) ? 1 : 0) != 0);
    SetBooleanOption(
        g_setComfortModeEnabled,
        ReadVrSetting(
            L"ComfortMode",
            QueryBooleanOption(g_isComfortModeEnabled, false) ? 1 : 0) != 0);
    ApplyHeadBobEnabled(
        !g_forceHeadBobDisabled &&
        ReadVrSetting(L"HeadBob", g_headBobEnabled ? 1 : 0) != 0);
    g_weaponAimGuideEnabled =
        ReadVrSetting(L"AimGuide", 1) != 0;
    g_controllerHapticsEnabled =
        ReadVrSetting(L"Haptics", 1) != 0;
    g_flashlightMount = static_cast<FlashlightMount>(
        std::clamp(
            ReadVrSetting(
                L"FlashlightMount",
                static_cast<int>(FlashlightMount::Weapon)),
            0, kFlashlightMountCount - 1));
    g_twoHandedGripEnabled =
        ReadVrSetting(L"TwoHandGrip", 1) != 0;
    g_meleeThrustEnabled =
        ReadVrSetting(L"MeleeGestures", 1) != 0;
    g_meleeWeaponStrikeEnabled =
        ReadVrSetting(L"MeleeWeaponStrike", 1) != 0;
    g_meleeOffHandStrikeEnabled =
        ReadVrSetting(L"MeleeOffHandStrike", 1) != 0;
    g_meleeJumpKickEnabled =
        ReadVrSetting(L"MeleeJumpKick", 1) != 0;
    g_meleeSlideKickEnabled =
        ReadVrSetting(L"MeleeSlideKick", 1) != 0;
    // Klettern greift in die Fortbewegung ein und bleibt deshalb
    // ausdruecklich abschaltbar; Standard ist aus, solange es nicht im Spiel
    // bestaetigt ist.
    g_climbingEnabled = ReadVrSetting(L"Climbing", 0) != 0;
    g_physicalLeanEnabled = ReadVrSetting(L"PhysicalLean", 1) != 0;
    g_physicalDuckEnabled = ReadVrSetting(L"PhysicalDuck", 1) != 0;
    g_physicalDuckActive = false;
    g_leanScalePercent =
        std::clamp(ReadVrSetting(L"LeanScale", 200), 100, 400);
    g_configuredEyeHeightMeters = SanitizeEyeHeightMeters(
        ReadVrFloat(L"VR", L"EyeHeightMeters", 0.0F));
    g_leftHandedBindings =
        ReadVrSetting(L"LeftHanded", 0) != 0;
    g_headRelativeMovement =
        ReadVrSetting(L"HeadRelativeMovement", 0) != 0;
    g_showPlayerArms =
        ReadVrSetting(L"ShowArms", 0) != 0;
    g_fovScalePreset = FovScalePresetForPercent(
        ReadVrSetting(L"FovScale", 130));
    ApplyFovScalePreset();
    // An explicit launcher value is a one-run override. Otherwise the live
    // tool-menu choice persists with the remaining VR settings.
    if (!CommandLineContains(L"-fearvr-render-scale")) {
        g_renderScalePercent = static_cast<std::uint32_t>(
            std::clamp(ReadVrSetting(L"RenderScale", 100), 100, 200));
        ApplyRenderScalePercent();
    }
    g_turnSpeedPreset =
        std::clamp(ReadVrSetting(L"TurnSpeed", 1), 0, 2);
    g_hiddenBodyPieceMask = static_cast<std::uint32_t>(
        std::clamp(
            ReadVrSetting(L"HiddenBodyPieces", 0),
            0, 0xF));
    if (g_hiddenBodyPieceMask ==
        kLegacyPlayerBodyArmPieceMask) {
        g_hiddenBodyPieceMask = 0;
        WriteVrSetting(L"HiddenBodyPieces", 0);
        Report(
            "INFO", "vr_body_piece_mask_migrated",
            "Legacy HiddenBodyPieces=2 hid Body_Group including torso and "
            "legs. All body pieces are now visible; the locally generated "
            "alpha material removes only the arm UV islands.");
    }
    // Keep the F11 probe in step with a persisted isolation mask so the next
    // press continues the walk instead of jumping back to the start.
    g_bodyPieceProbeStep =
        g_hiddenBodyPieceMask == kPlayerBodyPieceMaskAll ? 5U : 0U;
    for (std::uint32_t index = 0; index < 4U; ++index) {
        if (g_hiddenBodyPieceMask ==
            (kPlayerBodyPieceMaskAll & ~(1U << index))) {
            g_bodyPieceProbeStep = index + 1U;
            break;
        }
    }
    // A persisted stereo choice must not be overwritten by the automatic
    // first-playing-frame activation.
    g_autoStereoActivationAttempted = true;
    Report(
        "INFO", "vr_settings_loaded",
        "Persistent in-game VR settings were loaded from fearvr.ini.");
}

void WriteVrSetting(const wchar_t* name, int value) noexcept {
    if (g_vrSettingsPath[0] == L'\0') {
        return;
    }
    wchar_t text[16]{};
    _snwprintf_s(
        text, std::size(text), _TRUNCATE, L"%d", value);
    WritePrivateProfileStringW(
        L"VR", name, text, g_vrSettingsPath);
}

void WriteVrFloat(
    const wchar_t* section, const wchar_t* name, float value) noexcept {
    if (g_vrSettingsPath[0] == L'\0' || !std::isfinite(value)) {
        return;
    }
    wchar_t text[32]{};
    _snwprintf_s(
        text, std::size(text), _TRUNCATE,
        L"%.6g", static_cast<double>(value));
    WritePrivateProfileStringW(section, name, text, g_vrSettingsPath);
}
void SaveVrSettings() noexcept {
    if (g_vrSettingsPath[0] == L'\0') {
        LocateVrSettingsFile();
    }
    WriteVrSetting(
        L"Stereo",
        QueryBooleanOption(g_isStereoEnabled, true) ? 1 : 0);
    WriteVrSetting(
        L"Translation",
        QueryBooleanOption(g_isTranslationEnabled, false) ? 1 : 0);
    WriteVrSetting(
        L"StereoHUD",
        QueryBooleanOption(g_isStereoHudEnabled, true) ? 1 : 0);
    WriteVrSetting(
        L"ComfortMode",
        QueryBooleanOption(g_isComfortModeEnabled, false) ? 1 : 0);
    WriteVrSetting(
        L"WeaponWeightEnabled", g_weaponWeightEnabled ? 1 : 0);
    WriteVrSetting(
        L"WeaponWeightPreset",
        static_cast<int>(g_weaponWeightPreset));
    WriteVrSetting(
        L"WeaponWeightDiagnostics",
        g_weaponWeightDiagnosticsEnabled ? 1 : 0);
    WriteVrSetting(
        L"WeaponRecoilEnabled", g_weaponRecoilEnabled ? 1 : 0);
    WriteVrFloat(
        L"VR", L"WeaponRecoilStrength",
        g_weaponRecoilProfile.strength);
    WriteVrFloat(
        L"VR", L"WeaponRecoilMuzzleRise",
        g_weaponRecoilProfile.muzzleRise);
    WriteVrFloat(
        L"VR", L"WeaponRecoilRecovery",
        g_weaponRecoilProfile.recovery);
    WriteVrSetting(
        L"WeaponCollisionEnabled", g_weaponCollisionEnabled ? 1 : 0);
    WriteVrSetting(
        L"WeaponCollisionDebug", g_weaponCollisionDebugEnabled ? 1 : 0);
    WriteVrFloat(
        L"VR", L"WeaponCollisionLengthScale",
        g_defaultWeaponCollisionProfile.lengthScale);
    WriteVrFloat(
        L"VR", L"WeaponCollisionWidth",
        g_defaultWeaponCollisionProfile.widthMeters);
    WriteVrFloat(
        L"VR", L"WeaponCollisionHeight",
        g_defaultWeaponCollisionProfile.heightMeters);
    WriteVrFloat(
        L"VR", L"WeaponCollisionForwardOffset",
        g_defaultWeaponCollisionProfile.forwardOffsetMeters);
    WriteVrFloat(
        L"VR", L"WeaponWeight",
        g_defaultWeaponWeightProfile.weight);
    WriteVrFloat(
        L"VR", L"WeaponPositionalFollow",
        g_defaultWeaponWeightProfile.positionalFollow);
    WriteVrFloat(
        L"VR", L"WeaponRotationalFollow",
        g_defaultWeaponWeightProfile.rotationalFollow);
    WriteVrFloat(
        L"VR", L"WeaponCatchUpStrength",
        g_defaultWeaponWeightProfile.catchUpStrength);
    WriteVrFloat(
        L"VR", L"IkElbowOutward", g_armIkTuning.elbowOutward);
    WriteVrFloat(
        L"VR", L"IkElbowDown", g_armIkTuning.elbowDown);
    WriteVrFloat(
        L"VR", L"IkElbowBack", g_armIkTuning.elbowBack);
    WriteVrSetting(
        L"IkElbowContinuity",
        g_armIkTuning.preserveElbowContinuity ? 1 : 0);
    WriteVrFloat(
        L"VR", L"IkLeftHandRight",
        g_armIkTuning.leftHandRightMeters);
    WriteVrFloat(
        L"VR", L"IkLeftHandUp",
        g_armIkTuning.leftHandUpMeters);
    WriteVrFloat(
        L"VR", L"IkLeftHandForward",
        g_armIkTuning.leftHandForwardMeters);
    WriteVrFloat(
        L"VR", L"IkLeftHandPitch",
        g_armIkTuning.leftHandPitchDegrees);
    WriteVrFloat(
        L"VR", L"IkLeftHandYaw",
        g_armIkTuning.leftHandYawDegrees);
    WriteVrFloat(
        L"VR", L"IkLeftHandRoll",
        g_armIkTuning.leftHandRollDegrees);
    WriteVrSetting(L"HeadBob", g_headBobEnabled ? 1 : 0);
    WriteVrSetting(
        L"AimGuide", g_weaponAimGuideEnabled ? 1 : 0);
    WriteVrSetting(
        L"Haptics", g_controllerHapticsEnabled ? 1 : 0);
    WriteVrSetting(
        L"FlashlightMount",
        static_cast<int>(g_flashlightMount));
    WriteVrSetting(
        L"TwoHandGrip", g_twoHandedGripEnabled ? 1 : 0);
    WriteVrSetting(
        L"MeleeGestures", g_meleeThrustEnabled ? 1 : 0);
    WriteVrSetting(
        L"MeleeWeaponStrike",
        g_meleeWeaponStrikeEnabled ? 1 : 0);
    WriteVrSetting(
        L"MeleeOffHandStrike",
        g_meleeOffHandStrikeEnabled ? 1 : 0);
    WriteVrSetting(
        L"MeleeJumpKick", g_meleeJumpKickEnabled ? 1 : 0);
    WriteVrSetting(
        L"MeleeSlideKick", g_meleeSlideKickEnabled ? 1 : 0);
    WriteVrSetting(L"Climbing", g_climbingEnabled ? 1 : 0);
    WriteVrSetting(
        L"PhysicalLean", g_physicalLeanEnabled ? 1 : 0);
    WriteVrSetting(
        L"PhysicalDuck", g_physicalDuckEnabled ? 1 : 0);
    WriteVrSetting(L"LeanScale", g_leanScalePercent);
    WriteVrFloat(
        L"VR", L"EyeHeightMeters", g_configuredEyeHeightMeters);
    WriteVrSetting(
        L"LeftHanded", g_leftHandedBindings ? 1 : 0);
    WriteVrSetting(
        L"HeadRelativeMovement", g_headRelativeMovement ? 1 : 0);
    WriteVrSetting(L"ShowArms", g_showPlayerArms ? 1 : 0);
    WriteVrSetting(
        L"FovScale",
        static_cast<int>(CurrentFovScalePercent()));
    WriteVrSetting(
        L"RenderScale",
        static_cast<int>(g_renderScalePercent));
    WriteVrSetting(L"TurnSpeed", g_turnSpeedPreset);
    SaveActiveWeaponWeightProfile();
    SaveActiveWeaponRecoilProfile();
    SaveActiveWeaponCollisionProfile();
    g_vrSettingsFilePresent = true;
}

void* RetailVrMenuList(void* menu) noexcept {
    return menu == nullptr
        ? nullptr
        : static_cast<void*>(
              static_cast<unsigned char*>(menu) +
              kRetailMenuListOffset);
}

void SetRetailControlVisible(
    const VrMenuControl& control, bool visible) noexcept {
    if (control.object == nullptr) {
        return;
    }
    __try {
        void** const vtable =
            *reinterpret_cast<void***>(control.object);
        const auto show =
            reinterpret_cast<RetailControlShowFunction>(
                vtable[kRetailControlShowVtableSlot]);
        if (IsExecutableAddress(
                reinterpret_cast<const void*>(show))) {
            show(control.object, visible);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void SetRetailControlVisible(
    void* control, bool visible) noexcept {
    SetRetailControlVisible(
        VrMenuControl{control, 0}, visible);
}

void MarkRetailVrMenuForLayout() noexcept {
    void* const list = RetailVrMenuList(g_vrMenuOwner);
    if (list == nullptr) {
        return;
    }
    __try {
        *(static_cast<unsigned char*>(list) +
          kRetailListNeedsRecalculationOffset) = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// CLTGUIListCtrl::SetSelection bestimmt beim Herunterscrollen den neuen
// Listenanfang, indem es rückwärts die `GetBaseHeight()` aller Controls
// aufsummiert — ohne `IsVisible()` zu prüfen. `CalculatePositions()`
// überspringt unsichtbare Controls dagegen. Auf der VR-Seite liegen zu jedem
// sichtbaren Umschalter versteckte Geschwister-Controls, weshalb
// `m_nFirstShown` falsch gesetzt wird und die Liste springt.
//
// Beim Verlassen der VR-Seite muss der Listenanfang wieder auf Retails erste
// physische Zeile zeigen. Der Schreibzugriff erfolgt nur bei Bedarf, damit
// nicht jedes Frame eine Neuberechnung erzwungen wird.
void ResetRetailVrMenuScroll() noexcept {
    void* const list = RetailVrMenuList(g_vrMenuOwner);
    if (list == nullptr) {
        return;
    }
    __try {
        auto* const firstShown = reinterpret_cast<std::uint32_t*>(
            static_cast<unsigned char*>(list) +
            kRetailListFirstShownOffset);
        if (*firstShown != 0) {
            *firstShown = 0;
            *(static_cast<unsigned char*>(list) +
              kRetailListNeedsRecalculationOffset) = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void SetRetailVrMenuCompactSpacing(bool compact) noexcept {
    void* const list = RetailVrMenuList(g_vrMenuOwner);
    if (list == nullptr) {
        return;
    }
    __try {
        auto* const spacing = reinterpret_cast<int*>(
            static_cast<unsigned char*>(list) +
            kRetailListItemSpacingOffset);
        if (compact) {
            const int current = *spacing;
            if (!g_vrOriginalItemSpacingKnown) {
                // Retail uses FontSize / 4. Reject an unexpected layout
                // instead of writing through an unverified object.
                if (current < 0 || current > 32) {
                    return;
                }
                g_vrOriginalItemSpacing = current;
                g_vrOriginalItemSpacingKnown = true;
            }
            *spacing = 0;
        } else if (g_vrOriginalItemSpacingKnown) {
            *spacing = g_vrOriginalItemSpacing;
        }
        *(static_cast<unsigned char*>(list) +
          kRetailListNeedsRecalculationOffset) = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

VrMenuControl AddRetailVrMenuControl(
    const wchar_t* text, std::uint32_t command,
    bool isStatic = false) noexcept {
    VrMenuControl result;
    if (g_vrMenuOwner == nullptr ||
        g_retailMenuAddControl == nullptr ||
        g_retailListGetControl == nullptr) {
        return result;
    }
    __try {
        result.index = g_retailMenuAddControl(
            g_vrMenuOwner, text, command, isStatic);
        result.object = g_retailListGetControl(
            RetailVrMenuList(g_vrMenuOwner), result.index);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = VrMenuControl{};
    }
    return result;
}

void HideRetailVrSettingsControls() noexcept {
    SetRetailControlVisible(g_vrMenuTitle, false);
    SetRetailControlVisible(g_vrMenuDisplayTitle, false);
    SetRetailControlVisible(g_vrMenuComfortTitle, false);
    SetRetailControlVisible(g_vrMenuControlsTitle, false);
    SetRetailControlVisible(g_vrMenuWeaponsTitle, false);
    SetRetailControlVisible(g_vrMenuWeaponHandlingTitle, false);
    SetRetailControlVisible(g_vrMenuWeaponWeightTitle, false);
    SetRetailControlVisible(g_vrMenuWeaponRecoilTitle, false);
    SetRetailControlVisible(g_vrMenuMeleeTitle, false);
    SetRetailControlVisible(g_vrMenuAdvancedTitle, false);
    const VrMenuControl categoryControls[] = {
        g_vrMenuOpenDisplay,
        g_vrMenuOpenComfort,
        g_vrMenuOpenControls,
        g_vrMenuOpenWeapons,
        g_vrMenuOpenWeaponHandling,
        g_vrMenuOpenWeaponWeight,
        g_vrMenuOpenWeaponRecoil,
        g_vrMenuOpenMelee,
        g_vrMenuOpenAdvanced,
    };
    for (const VrMenuControl& control : categoryControls) {
        SetRetailControlVisible(control, false);
    }
    const VrMenuToggle toggles[] = {
        g_vrMenuStereo,
        g_vrMenuTranslation,
        g_vrMenuStereoHud,
        g_vrMenuHeadBob,
        g_vrMenuComfort,
        g_vrMenuAimGuide,
        g_vrMenuHaptics,
        g_vrMenuHandedness,
        g_vrMenuMovementDirection,
        g_vrMenuClimbing,
        g_vrMenuPhysicalLean,
        g_vrMenuPhysicalDuck,
        g_vrMenuMelee,
        g_vrMenuMeleeWeaponStrike,
        g_vrMenuMeleeOffHandStrike,
        g_vrMenuMeleeJumpKick,
        g_vrMenuMeleeSlideKick,
        g_vrMenuArms,
        g_vrMenuTwoHandGrip,
        g_vrMenuWeaponWeight,
        g_vrMenuWeaponRecoil,
        g_vrMenuWeaponWeightDiagnostics,
        g_vrMenuWeaponProfileTarget,
    };
    for (const VrMenuToggle& toggle : toggles) {
        SetRetailControlVisible(toggle.enabled, false);
        SetRetailControlVisible(toggle.disabled, false);
    }
    for (const VrMenuControl& turnSpeed : g_vrMenuTurnSpeed) {
        SetRetailControlVisible(turnSpeed, false);
    }
    for (const VrMenuControl& weaponWeight : g_vrMenuWeaponWeightPreset) {
        SetRetailControlVisible(weaponWeight, false);
    }
    for (const VrMenuControl& mount : g_vrMenuFlashlightMount) {
        SetRetailControlVisible(mount, false);
    }
    for (const VrMenuControl& fovScale : g_vrMenuFovScale) {
        SetRetailControlVisible(fovScale, false);
    }
    for (const VrMenuControl& leanScale : g_vrMenuLeanScale) {
        SetRetailControlVisible(leanScale, false);
    }
    for (const VrMenuControl& value : g_vrMenuWeaponWeightValue) {
        SetRetailControlVisible(value, false);
    }
    for (const VrMenuControl& value : g_vrMenuWeaponPositionFollow) {
        SetRetailControlVisible(value, false);
    }
    for (const VrMenuControl& value : g_vrMenuWeaponRotationFollow) {
        SetRetailControlVisible(value, false);
    }
    for (const VrMenuControl& value : g_vrMenuWeaponCatchUp) {
        SetRetailControlVisible(value, false);
    }
    for (const VrMenuControl& value : g_vrMenuWeaponRecoilStrength) {
        SetRetailControlVisible(value, false);
    }
    for (const VrMenuControl& value : g_vrMenuWeaponRecoilRise) {
        SetRetailControlVisible(value, false);
    }
    for (const VrMenuControl& value : g_vrMenuWeaponRecoilRecovery) {
        SetRetailControlVisible(value, false);
    }
    SetRetailControlVisible(g_vrMenuResetWeaponProfile, false);
    SetRetailControlVisible(g_vrMenuRecenter, false);
    SetRetailControlVisible(g_vrMenuDefaults, false);
    SetRetailControlVisible(g_vrMenuBack, false);
}

VrMenuControl SetRetailVrToggleVisible(
    const VrMenuToggle& toggle, bool enabled) noexcept {
    SetRetailControlVisible(toggle.enabled, enabled);
    SetRetailControlVisible(toggle.disabled, !enabled);
    return enabled ? toggle.enabled : toggle.disabled;
}

template <std::size_t Size>
VrMenuControl SetRetailVrPresetVisible(
    VrMenuControl (&controls)[Size], std::size_t selected) noexcept {
    selected = (std::min)(selected, Size - 1U);
    for (std::size_t index = 0; index < Size; ++index) {
        SetRetailControlVisible(controls[index], index == selected);
    }
    return controls[selected];
}

bool HasCurrentWeaponWeightProfile() noexcept {
    return g_weightedWeaponInput.weapon != nullptr &&
           g_weightedWeaponInput.profileName[0] != '\0' &&
           std::strcmp(
               g_weightedWeaponInput.profileName, "default") != 0;
}

bool EditingCurrentWeaponWeightProfile() noexcept {
    return g_vrWeaponProfileEditsCurrent &&
           HasCurrentWeaponWeightProfile();
}

WeaponWeightProfile& EditableWeaponWeightProfile() noexcept {
    return EditingCurrentWeaponWeightProfile()
        ? g_weightedWeaponInput.profile
        : g_defaultWeaponWeightProfile;
}

bool EditingCurrentWeaponRecoilProfile() noexcept {
    return g_vrRecoilProfileEditsCurrent &&
           HasCurrentWeaponWeightProfile();
}

const WeaponRecoilProfile& DisplayedWeaponRecoilProfile() noexcept {
    return EditingCurrentWeaponRecoilProfile()
        ? g_weightedWeaponInput.recoilProfile
        : g_weaponRecoilProfile;
}

WeaponRecoilProfile& EditableWeaponRecoilProfile() noexcept {
    if (!EditingCurrentWeaponRecoilProfile()) {
        return g_weaponRecoilProfile;
    }
    if (!g_weightedWeaponInput.recoilProfileExplicit) {
        g_weightedWeaponInput.recoilProfile = g_weaponRecoilProfile;
        g_weightedWeaponInput.recoilProfileExplicit = true;
    }
    return g_weightedWeaponInput.recoilProfile;
}

const WeaponRecoilProfile& ActiveWeaponRecoilProfile() noexcept {
    return g_weightedWeaponInput.recoilProfileExplicit
        ? g_weightedWeaponInput.recoilProfile
        : g_weaponRecoilProfile;
}

bool EditingCurrentWeaponCollisionProfile() noexcept {
    return g_vrCollisionProfileEditsCurrent &&
           HasCurrentWeaponWeightProfile();
}

const WeaponCollisionProfile& DisplayedWeaponCollisionProfile() noexcept {
    return EditingCurrentWeaponCollisionProfile() &&
           g_weightedWeaponInput.collisionProfileExplicit
        ? g_weightedWeaponInput.collisionProfile
        : g_defaultWeaponCollisionProfile;
}

WeaponCollisionProfile& EditableWeaponCollisionProfile() noexcept {
    if (!EditingCurrentWeaponCollisionProfile()) {
        return g_defaultWeaponCollisionProfile;
    }
    if (!g_weightedWeaponInput.collisionProfileExplicit) {
        g_weightedWeaponInput.collisionProfile =
            g_defaultWeaponCollisionProfile;
        g_weightedWeaponInput.collisionProfileExplicit = true;
    }
    return g_weightedWeaponInput.collisionProfile;
}

const WeaponCollisionProfile& ActiveWeaponCollisionProfile() noexcept {
    return g_weightedWeaponInput.collisionProfileExplicit
        ? g_weightedWeaponInput.collisionProfile
        : g_defaultWeaponCollisionProfile;
}

void ResetEditableWeaponWeightProfile() noexcept {
    EditableWeaponWeightProfile() = WeaponWeightProfile{};
    ResetWeaponWeightPair(
        g_weightedWeaponInput.filters,
        WeaponWeightResetReason::enabledChanged);
}

VrMenuControl RefreshRetailVrSettingsControls() noexcept {
    HideRetailVrSettingsControls();
    VrMenuControl first = g_vrMenuBack;

    switch (g_vrSettingsPage) {
    case VrSettingsPage::Root:
        SetRetailControlVisible(g_vrMenuTitle, true);
        SetRetailControlVisible(g_vrMenuOpenDisplay, true);
        SetRetailControlVisible(g_vrMenuOpenComfort, true);
        SetRetailControlVisible(g_vrMenuOpenControls, true);
        SetRetailControlVisible(g_vrMenuOpenWeapons, true);
        SetRetailControlVisible(g_vrMenuOpenMelee, true);
        SetRetailControlVisible(g_vrMenuOpenAdvanced, true);
        first = g_vrMenuOpenDisplay;
        break;
    case VrSettingsPage::Display:
        SetRetailControlVisible(g_vrMenuDisplayTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuStereo,
            QueryBooleanOption(g_isStereoEnabled, true));
        SetRetailVrToggleVisible(
            g_vrMenuStereoHud,
            QueryBooleanOption(g_isStereoHudEnabled, true));
        SetRetailVrPresetVisible(
            g_vrMenuFovScale,
            static_cast<std::size_t>(g_fovScalePreset));
        break;
    case VrSettingsPage::Comfort:
        SetRetailControlVisible(g_vrMenuComfortTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuTranslation,
            QueryBooleanOption(g_isTranslationEnabled, false));
        SetRetailVrToggleVisible(g_vrMenuHeadBob, g_headBobEnabled);
        SetRetailVrToggleVisible(
            g_vrMenuComfort,
            QueryBooleanOption(g_isComfortModeEnabled, false));
        SetRetailVrPresetVisible(
            g_vrMenuTurnSpeed,
            static_cast<std::size_t>(g_turnSpeedPreset));
        SetRetailVrToggleVisible(
            g_vrMenuMovementDirection, g_headRelativeMovement);
        SetRetailVrToggleVisible(
            g_vrMenuPhysicalLean, g_physicalLeanEnabled);
        SetRetailVrPresetVisible(
            g_vrMenuLeanScale,
            ClosestVrPresetIndex(g_leanScalePercent, kLeanScalePresets));
        break;
    case VrSettingsPage::Controls:
        SetRetailControlVisible(g_vrMenuControlsTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuHandedness, !g_leftHandedBindings);
        SetRetailVrToggleVisible(
            g_vrMenuHaptics, g_controllerHapticsEnabled);
        {
            const int flashlightMount = std::clamp(
                static_cast<int>(g_flashlightMount),
                0, kFlashlightMountCount - 1);
            for (int index = 0; index < kFlashlightMountCount; ++index) {
                SetRetailControlVisible(
                    g_vrMenuFlashlightMount[index],
                    index == flashlightMount);
            }
        }
        SetRetailVrToggleVisible(g_vrMenuClimbing, g_climbingEnabled);
        SetRetailVrToggleVisible(
            g_vrMenuPhysicalDuck, g_physicalDuckEnabled);
        SetRetailControlVisible(g_vrMenuRecenter, true);
        break;
    case VrSettingsPage::Weapons:
        SetRetailControlVisible(g_vrMenuWeaponsTitle, true);
        SetRetailControlVisible(g_vrMenuOpenWeaponHandling, true);
        SetRetailControlVisible(g_vrMenuOpenWeaponWeight, true);
        SetRetailControlVisible(g_vrMenuOpenWeaponRecoil, true);
        first = g_vrMenuOpenWeaponHandling;
        break;
    case VrSettingsPage::WeaponHandling:
        SetRetailControlVisible(g_vrMenuWeaponHandlingTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuAimGuide, g_weaponAimGuideEnabled);
        SetRetailVrToggleVisible(g_vrMenuArms, g_showPlayerArms);
        SetRetailVrToggleVisible(
            g_vrMenuTwoHandGrip, g_twoHandedGripEnabled);
        break;
    case VrSettingsPage::WeaponWeight: {
        SetRetailControlVisible(g_vrMenuWeaponWeightTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuWeaponProfileTarget,
            EditingCurrentWeaponWeightProfile());
        // The four presets include NONE, so the older independent on/off row
        // would be redundant and would push this page beyond Retail's eight
        // visible selectable rows.
        const int weaponWeightPreset = std::clamp(
            static_cast<int>(g_weaponWeightPreset), 0, 3);
        for (int index = 0; index < 4; ++index) {
            SetRetailControlVisible(
                g_vrMenuWeaponWeightPreset[index],
                index == weaponWeightPreset);
        }
        const WeaponWeightProfile& profile = EditableWeaponWeightProfile();
        SetRetailVrPresetVisible(
            g_vrMenuWeaponWeightValue,
            ClosestVrPresetIndex(profile.weight, kWeaponWeightPresets));
        SetRetailVrPresetVisible(
            g_vrMenuWeaponPositionFollow,
            ClosestVrPresetIndex(
                profile.positionalFollow,
                kWeaponPositionFollowPresets));
        SetRetailVrPresetVisible(
            g_vrMenuWeaponRotationFollow,
            ClosestVrPresetIndex(
                profile.rotationalFollow,
                kWeaponRotationFollowPresets));
        SetRetailVrPresetVisible(
            g_vrMenuWeaponCatchUp,
            ClosestVrPresetIndex(
                profile.catchUpStrength, kWeaponCatchUpPresets));
        SetRetailControlVisible(g_vrMenuResetWeaponProfile, true);
        break;
    }
    case VrSettingsPage::WeaponRecoil: {
        SetRetailControlVisible(g_vrMenuWeaponRecoilTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuWeaponProfileTarget,
            EditingCurrentWeaponRecoilProfile());
        SetRetailVrToggleVisible(
            g_vrMenuWeaponRecoil, g_weaponRecoilEnabled);
        const WeaponRecoilProfile& profile =
            DisplayedWeaponRecoilProfile();
        SetRetailVrPresetVisible(
            g_vrMenuWeaponRecoilStrength,
            ClosestVrPresetIndex(
                profile.strength,
                kWeaponRecoilStrengthPresets));
        SetRetailVrPresetVisible(
            g_vrMenuWeaponRecoilRise,
            ClosestVrPresetIndex(
                profile.muzzleRise,
                kWeaponRecoilRisePresets));
        SetRetailVrPresetVisible(
            g_vrMenuWeaponRecoilRecovery,
            ClosestVrPresetIndex(
                profile.recovery,
                kWeaponRecoilRecoveryPresets));
        break;
    }
    case VrSettingsPage::Melee:
        SetRetailControlVisible(g_vrMenuMeleeTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuMelee, g_meleeThrustEnabled);
        if (g_meleeThrustEnabled) {
            SetRetailVrToggleVisible(
                g_vrMenuMeleeWeaponStrike,
                g_meleeWeaponStrikeEnabled);
            SetRetailVrToggleVisible(
                g_vrMenuMeleeOffHandStrike,
                g_meleeOffHandStrikeEnabled);
            SetRetailVrToggleVisible(
                g_vrMenuMeleeJumpKick,
                g_meleeJumpKickEnabled);
            SetRetailVrToggleVisible(
                g_vrMenuMeleeSlideKick,
                g_meleeSlideKickEnabled);
        }
        break;
    case VrSettingsPage::Advanced:
        SetRetailControlVisible(g_vrMenuAdvancedTitle, true);
        first = SetRetailVrToggleVisible(
            g_vrMenuWeaponWeightDiagnostics,
            g_weaponWeightDiagnosticsEnabled);
        SetRetailControlVisible(g_vrMenuDefaults, true);
        break;
    case VrSettingsPage::None:
    default:
        break;
    }
    SetRetailControlVisible(g_vrMenuBack, true);
    MarkRetailVrMenuForLayout();
    return first;
}

std::array<VrMenuControl, kRetailVrMenuVisibleControlCount>
RetailVrMenuVisibleControls() noexcept {
    const auto toggleControl = [](
        const VrMenuToggle& toggle, bool enabled) noexcept {
        return enabled ? toggle.enabled : toggle.disabled;
    };
    const int turnSpeed = std::clamp(g_turnSpeedPreset, 0, 2);
    const int fovScale = std::clamp(g_fovScalePreset, 0, 3);
    const int weaponWeightPreset = std::clamp(
        static_cast<int>(g_weaponWeightPreset), 0, 3);
    const int flashlightMount = std::clamp(
        static_cast<int>(g_flashlightMount),
        0, kFlashlightMountCount - 1);

    return {{
        toggleControl(
            g_vrMenuStereo,
            QueryBooleanOption(g_isStereoEnabled, true)),
        toggleControl(
            g_vrMenuStereoHud,
            QueryBooleanOption(g_isStereoHudEnabled, true)),
        g_vrMenuWeaponWeightPreset[weaponWeightPreset],
        toggleControl(g_vrMenuAimGuide, g_weaponAimGuideEnabled),
        toggleControl(g_vrMenuHaptics, g_controllerHapticsEnabled),
        g_vrMenuFlashlightMount[flashlightMount],
        toggleControl(g_vrMenuHandedness, !g_leftHandedBindings),
        toggleControl(g_vrMenuClimbing, g_climbingEnabled),
        toggleControl(g_vrMenuPhysicalLean, g_physicalLeanEnabled),
        toggleControl(g_vrMenuPhysicalDuck, g_physicalDuckEnabled),
        toggleControl(g_vrMenuMelee, g_meleeThrustEnabled),
        toggleControl(g_vrMenuArms, g_showPlayerArms),
        g_vrMenuFovScale[fovScale],
        g_vrMenuTurnSpeed[turnSpeed],
        g_vrMenuRecenter,
        g_vrMenuDefaults,
        g_vrMenuBack,
    }};
}

void MaintainRetailVrMenuScroll() noexcept {
    if (!g_vrSettingsPageActive) {
        return;
    }
    // Categorized pages are deliberately shorter than Retail's viewport.
    // The legacy flat-page row map below does not describe their controls and
    // must not rewrite m_nFirstShown while navigating between categories.
    if (IsVrSettingsPage(g_vrSettingsPage)) {
        return;
    }
    void* const list = RetailVrMenuList(g_vrMenuOwner);
    if (list == nullptr) {
        return;
    }

    __try {
        const auto controls = RetailVrMenuVisibleControls();
        const std::uint32_t selection =
            *reinterpret_cast<const std::uint32_t*>(
                static_cast<const unsigned char*>(list) +
                kRetailListCurrentIndexOffset);
        std::size_t selectedRow = controls.size();
        for (std::size_t row = 0; row < controls.size(); ++row) {
            if (controls[row].object != nullptr &&
                controls[row].index == selection) {
                selectedRow = row;
                break;
            }
        }
        if (selectedRow == controls.size()) {
            return;
        }

        std::array<std::uint32_t, kRetailVrMenuVisibleControlCount>
            controlIndices{};
        for (std::size_t row = 0; row < controls.size(); ++row) {
            controlIndices[row] = controls[row].index;
        }
        if (!VrMenuControlOrderIsStrictlyIncreasing(
                controlIndices.data(), controlIndices.size())) {
            Report(
                "ERROR", "vr_settings_menu_order_invalid",
                "The logical VR rows do not match Retail's physical control "
                "order; scrolling was left unchanged.");
            return;
        }
        const std::uint32_t lastShown =
            *reinterpret_cast<const std::uint32_t*>(
                static_cast<const unsigned char*>(list) +
                kRetailListLastShownOffset);
        const std::size_t observedPageRows =
            VrMenuVisibleRowCapacity(
                controlIndices.data(), controlIndices.size(),
                g_vrMenuFirstVisibleRow, lastShown);
        if (observedPageRows != 0) {
            g_vrMenuPageRows = VrMenuSafePageRows(observedPageRows);
        }

        g_vrMenuFirstVisibleRow = VrMenuScrollStart(
            g_vrMenuFirstVisibleRow,
            selectedRow,
            controls.size(),
            g_vrMenuPageRows);
        const std::uint32_t desiredFirstShown =
            controls[g_vrMenuFirstVisibleRow].index;
        auto* const firstShown = reinterpret_cast<std::uint32_t*>(
            static_cast<unsigned char*>(list) +
            kRetailListFirstShownOffset);
        if (*firstShown != desiredFirstShown) {
            *firstShown = desiredFirstShown;
            *(static_cast<unsigned char*>(list) +
              kRetailListNeedsRecalculationOffset) = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void SelectRetailVrMenuControl(
    const VrMenuControl& control) noexcept {
    if (g_retailListSetSelection == nullptr ||
        control.object == nullptr) {
        return;
    }
    __try {
        g_retailListSetSelection(
            RetailVrMenuList(g_vrMenuOwner), control.index);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void RestoreRetailSystemMenuControls() noexcept {
    g_vrSettingsPage = VrSettingsPage::None;
    g_vrSettingsPageActive = false;
    SetRetailVrMenuCompactSpacing(false);
    for (std::size_t index = 0;
         index < g_vrNormalControlCount; ++index) {
        SetRetailControlVisible(g_vrNormalControls[index], true);
    }
    HideRetailVrSettingsControls();
    MarkRetailVrMenuForLayout();
}

void ShowRetailVrSettingsPage(VrSettingsPage page) noexcept {
    if (!g_vrMenuControlsBuilt || page == VrSettingsPage::None) {
        return;
    }
    g_vrSettingsPage = page;
    g_vrSettingsPageActive = true;
    SetRetailVrMenuCompactSpacing(true);
    const VrMenuControl first = RefreshRetailVrSettingsControls();
    SelectRetailVrMenuControl(first);
    // SetSelection counts hidden sibling controls while CalculatePositions
    // does not. Every categorized page fits completely, so physical row zero
    // is the only stable list origin after changing pages.
    ResetRetailVrMenuScroll();
}

void EnterRetailVrSettingsPage() noexcept {
    if (!g_vrMenuControlsBuilt) {
        return;
    }
    for (std::size_t index = 0;
         index < g_vrNormalControlCount; ++index) {
        SetRetailControlVisible(g_vrNormalControls[index], false);
    }
    ShowRetailVrSettingsPage(VrSettingsPage::Root);
    Report(
        "INFO", "vr_settings_menu_opened",
        "The native Retail pause menu is showing categorized VR settings.");
}

void LeaveRetailVrSettingsPage() noexcept {
    if (!g_vrSettingsPageActive) {
        return;
    }
    ResetRetailVrMenuScroll();
    g_vrMenuFirstVisibleRow = 0;
    g_vrMenuPageRows = 8;
    g_vrSettingsPageActive = false;
    RestoreRetailSystemMenuControls();
    SaveVrSettings();
    if (g_retailMenuOnFocus != nullptr &&
        g_vrMenuOwner != nullptr) {
        __try {
            g_retailMenuOnFocus(g_vrMenuOwner, true);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    Report(
        "INFO", "vr_settings_menu_closed",
        "The native Retail system-menu controls were restored.");
}

void NavigateBackRetailVrSettingsPage() noexcept {
    if (!g_vrSettingsPageActive) {
        return;
    }
    const VrSettingsPage parent = ParentVrSettingsPage(g_vrSettingsPage);
    if (parent == VrSettingsPage::None) {
        LeaveRetailVrSettingsPage();
    } else {
        ShowRetailVrSettingsPage(parent);
    }
}

void ResetVrTrackingBasis() noexcept {
    g_physicalDuckActive = false;
    InterlockedIncrement(&g_trackingResetGeneration);
}

void RequestVrOriginRecenter() noexcept {
    if (g_requestRecenter != nullptr) {
        __try {
            g_requestRecenter();
            return;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    ResetVrTrackingBasis();
}

void ResetArmIkBendMemory() noexcept {
    g_rightHandControl.bendDirectionValid = false;
    g_leftHandControl.bendDirectionValid = false;
}

void ResetArmIkTuning() noexcept {
    g_armIkTuning = ArmIkTuning{};
    ResetArmIkBendMemory();
    g_twoHandedGrip = TwoHandedGripState{};
}

void ApplyVrDefaults() noexcept {
    SetBooleanOption(g_setStereoEnabled, true);
    SetBooleanOption(g_setTranslationEnabled, false);
    SetBooleanOption(g_setStereoHudEnabled, true);
    SetBooleanOption(g_setComfortModeEnabled, false);
    ApplyHeadBobEnabled(false);
    g_weaponWeightEnabled = false;
    g_weaponWeightPreset = WeaponWeightPreset::none;
    g_defaultWeaponWeightProfile = {};
    g_weightedWeaponInput = {};
    ResetWeaponWeightPair(
        g_weightedWeaponInput.filters,
        WeaponWeightResetReason::enabledChanged);
    g_weaponWeightDiagnosticsEnabled = false;
    g_weaponRecoilEnabled = true;
    g_weaponRecoilProfile = {};
    ResetWeaponRecoil(g_weightedWeaponInput.recoil);
    g_weaponCollisionEnabled = true;
    g_weaponCollisionDebugEnabled = true;
    g_defaultWeaponCollisionProfile = {};
    g_weightedWeaponInput.collisionProfile = {};
    g_weightedWeaponInput.collisionProfileExplicit = false;
    g_weightedWeaponInput.collisionObstructed = false;
    ResetWeaponCollisionRuntime(g_weightedWeaponInput.collision);
    InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
    g_lastWeaponRecoilTick = 0;
    g_weightedWeaponInput.profile = WeaponWeightProfile{};
    g_vrWeaponProfileEditsCurrent = true;
    g_vrRecoilProfileEditsCurrent = true;
    g_vrCollisionProfileEditsCurrent = true;
    g_weaponAimGuideEnabled = true;
    g_controllerHapticsEnabled = true;
    g_flashlightMount = FlashlightMount::Weapon;
    g_twoHandedGripEnabled = true;
    g_leftHandedBindings = false;
    g_headRelativeMovement = false;
    g_turnSpeedPreset = 1;
    g_fovScalePreset = 3;
    ApplyFovScalePreset();
    g_renderScalePercent = 100U;
    ApplyRenderScalePercent();
    g_climbingEnabled = false;
    g_physicalLeanEnabled = true;
    g_physicalDuckEnabled = true;
    g_physicalDuckActive = false;
    g_leanScalePercent = 200;
    ResetLeanCollision(g_leanCollision);
    g_meleeThrustEnabled = true;
    g_meleeWeaponStrikeEnabled = true;
    g_meleeOffHandStrikeEnabled = true;
    g_meleeJumpKickEnabled = true;
    g_meleeSlideKickEnabled = true;
    g_showPlayerArms = false;
    ResetArmIkTuning();
    ResetMeleeActions(g_meleeActions);
    g_meleePulseUntil = 0;
    g_slideDuckPulseUntil = 0;
    g_slideForwardPulseUntil = 0;
    ResetClimbGrip(g_climbGrip);
    g_climbActive = false;
    g_climbOnLadder = false;
    g_climbAxis = 0.0F;
    g_climbWasGripping = false;
    RequestVrOriginRecenter();
}

bool BuildRetailVrMenuControls(void* menu) noexcept {
    g_vrMenuOwner = menu;
    g_vrNormalControlCount = 0;
    g_vrOriginalItemSpacingKnown = false;
    void* const list = RetailVrMenuList(menu);
    if (list == nullptr) {
        return false;
    }

    __try {
        for (std::uint32_t index = 0;
             index < kRetailSystemMenuOriginalControlCount;
             ++index) {
            void* const control =
                g_retailListGetControl(list, index);
            if (control == nullptr) {
                return false;
            }
            g_vrNormalControls[g_vrNormalControlCount++] =
                control;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    g_vrMenuEntry = AddRetailVrMenuControl(
        L"VR Settings", kVrMenuOpen);
    if (g_vrMenuEntry.object == nullptr ||
        g_vrMenuEntry.index !=
            kRetailSystemMenuOriginalControlCount) {
        return false;
    }
    g_vrNormalControls[g_vrNormalControlCount++] =
        g_vrMenuEntry.object;

    // Preserve every original item's relative order while moving the new
    // entry directly behind "Optionen" (original index 2).
    __try {
        for (std::uint32_t index = g_vrMenuEntry.index;
             index > 3; --index) {
            g_retailListSwapItems(list, index, index - 1);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    g_vrMenuEntry.index = 3;

    g_vrMenuTitle = AddRetailVrMenuControl(
        L"VR Settings", 0, true);
    g_vrMenuDisplayTitle = AddRetailVrMenuControl(
        L"Display & HUD", 0, true);
    g_vrMenuComfortTitle = AddRetailVrMenuControl(
        L"Movement & Comfort", 0, true);
    g_vrMenuControlsTitle = AddRetailVrMenuControl(
        L"Controls", 0, true);
    g_vrMenuWeaponsTitle = AddRetailVrMenuControl(
        L"Weapons", 0, true);
    g_vrMenuWeaponHandlingTitle = AddRetailVrMenuControl(
        L"Weapon Handling", 0, true);
    g_vrMenuWeaponWeightTitle = AddRetailVrMenuControl(
        L"Simulated Weapon Weight", 0, true);
    g_vrMenuWeaponRecoilTitle = AddRetailVrMenuControl(
        L"Weapon Recoil", 0, true);
    g_vrMenuMeleeTitle = AddRetailVrMenuControl(
        L"Melee", 0, true);
    g_vrMenuAdvancedTitle = AddRetailVrMenuControl(
        L"Advanced", 0, true);
    g_vrMenuOpenDisplay = AddRetailVrMenuControl(
        L"Display & HUD", kVrMenuOpenDisplay);
    g_vrMenuOpenComfort = AddRetailVrMenuControl(
        L"Movement & Comfort", kVrMenuOpenComfort);
    g_vrMenuOpenControls = AddRetailVrMenuControl(
        L"Controls", kVrMenuOpenControls);
    g_vrMenuOpenWeapons = AddRetailVrMenuControl(
        L"Weapons", kVrMenuOpenWeapons);
    g_vrMenuOpenWeaponHandling = AddRetailVrMenuControl(
        L"Handling & appearance", kVrMenuOpenWeaponHandling);
    g_vrMenuOpenWeaponWeight = AddRetailVrMenuControl(
        L"Simulated weight", kVrMenuOpenWeaponWeight);
    g_vrMenuOpenWeaponRecoil = AddRetailVrMenuControl(
        L"Recoil", kVrMenuOpenWeaponRecoil);
    g_vrMenuOpenMelee = AddRetailVrMenuControl(
        L"Melee", kVrMenuOpenMelee);
    g_vrMenuOpenAdvanced = AddRetailVrMenuControl(
        L"Advanced", kVrMenuOpenAdvanced);
    g_vrMenuStereo.enabled = AddRetailVrMenuControl(
        L"Stereo rendering: On", kVrMenuToggleStereo);
    g_vrMenuStereo.disabled = AddRetailVrMenuControl(
        L"Stereo rendering: Off", kVrMenuToggleStereo);
    g_vrMenuTranslation.enabled = AddRetailVrMenuControl(
        L"HMD translation: On", kVrMenuToggleTranslation);
    g_vrMenuTranslation.disabled = AddRetailVrMenuControl(
        L"HMD translation: Off", kVrMenuToggleTranslation);
    g_vrMenuStereoHud.enabled = AddRetailVrMenuControl(
        L"Stereo HUD: On", kVrMenuToggleStereoHud);
    g_vrMenuStereoHud.disabled = AddRetailVrMenuControl(
        L"Stereo HUD: Off", kVrMenuToggleStereoHud);
    g_vrMenuHeadBob.enabled = AddRetailVrMenuControl(
        L"Head bob: On", kVrMenuToggleHeadBob);
    g_vrMenuHeadBob.disabled = AddRetailVrMenuControl(
        L"Head bob: Off", kVrMenuToggleHeadBob);
    g_vrMenuComfort.enabled = AddRetailVrMenuControl(
        L"Comfort screen: On", kVrMenuToggleComfort);
    g_vrMenuComfort.disabled = AddRetailVrMenuControl(
        L"Comfort screen: Off", kVrMenuToggleComfort);
    g_vrMenuWeaponWeightPreset[0] = AddRetailVrMenuControl(
        L"Weapon weight: None", kVrMenuCycleWeaponWeightPreset);
    g_vrMenuWeaponWeightPreset[1] = AddRetailVrMenuControl(
        L"Weapon weight: Light", kVrMenuCycleWeaponWeightPreset);
    g_vrMenuWeaponWeightPreset[2] = AddRetailVrMenuControl(
        L"Weapon weight: Medium", kVrMenuCycleWeaponWeightPreset);
    g_vrMenuWeaponWeightPreset[3] = AddRetailVrMenuControl(
        L"Weapon weight: Heavy", kVrMenuCycleWeaponWeightPreset);
    g_vrMenuAimGuide.enabled = AddRetailVrMenuControl(
        L"Red aim guide: On", kVrMenuToggleAimGuide);
    g_vrMenuAimGuide.disabled = AddRetailVrMenuControl(
        L"Red aim guide: Off", kVrMenuToggleAimGuide);
    g_vrMenuHaptics.enabled = AddRetailVrMenuControl(
        L"Controller vibration: On", kVrMenuToggleHaptics);
    g_vrMenuHaptics.disabled = AddRetailVrMenuControl(
        L"Controller vibration: Off", kVrMenuToggleHaptics);
    g_vrMenuFlashlightMount[
        static_cast<int>(FlashlightMount::LeftHand)] =
        AddRetailVrMenuControl(
            L"Flashlight mount: LEFT HAND",
            kVrMenuCycleFlashlightMount);
    g_vrMenuFlashlightMount[
        static_cast<int>(FlashlightMount::Head)] =
        AddRetailVrMenuControl(
            L"Flashlight mount: HEAD",
            kVrMenuCycleFlashlightMount);
    g_vrMenuFlashlightMount[
        static_cast<int>(FlashlightMount::Weapon)] =
        AddRetailVrMenuControl(
            L"Flashlight mount: WEAPON",
            kVrMenuCycleFlashlightMount);
    g_vrMenuHandedness.enabled = AddRetailVrMenuControl(
        L"Controls: Right-handed", kVrMenuToggleHandedness);
    g_vrMenuHandedness.disabled = AddRetailVrMenuControl(
        L"Controls: Left-handed", kVrMenuToggleHandedness);
    g_vrMenuMovementDirection.enabled = AddRetailVrMenuControl(
        L"Move direction: Head", kVrMenuToggleHeadRelativeMovement);
    g_vrMenuMovementDirection.disabled = AddRetailVrMenuControl(
        L"Move direction: Body", kVrMenuToggleHeadRelativeMovement);
    g_vrMenuClimbing.enabled = AddRetailVrMenuControl(
        L"Ladder climbing: Hands", kVrMenuToggleClimbing);
    g_vrMenuClimbing.disabled = AddRetailVrMenuControl(
        L"Ladder climbing: Classic", kVrMenuToggleClimbing);
    g_vrMenuPhysicalLean.enabled = AddRetailVrMenuControl(
        L"Physical lean: On", kVrMenuTogglePhysicalLean);
    g_vrMenuPhysicalLean.disabled = AddRetailVrMenuControl(
        L"Physical lean: Off", kVrMenuTogglePhysicalLean);
    g_vrMenuPhysicalDuck.enabled = AddRetailVrMenuControl(
        L"Physical duck: On", kVrMenuTogglePhysicalDuck);
    g_vrMenuPhysicalDuck.disabled = AddRetailVrMenuControl(
        L"Physical duck: Off", kVrMenuTogglePhysicalDuck);
    g_vrMenuLeanScale[0] = AddRetailVrMenuControl(
        L"Lean strength: 100%", kVrMenuCycleLeanScale);
    g_vrMenuLeanScale[1] = AddRetailVrMenuControl(
        L"Lean strength: 150%", kVrMenuCycleLeanScale);
    g_vrMenuLeanScale[2] = AddRetailVrMenuControl(
        L"Lean strength: 200%", kVrMenuCycleLeanScale);
    g_vrMenuLeanScale[3] = AddRetailVrMenuControl(
        L"Lean strength: 250%", kVrMenuCycleLeanScale);
    g_vrMenuLeanScale[4] = AddRetailVrMenuControl(
        L"Lean strength: 300%", kVrMenuCycleLeanScale);
    g_vrMenuLeanScale[5] = AddRetailVrMenuControl(
        L"Lean strength: 350%", kVrMenuCycleLeanScale);
    g_vrMenuLeanScale[6] = AddRetailVrMenuControl(
        L"Lean strength: 400%", kVrMenuCycleLeanScale);
    g_vrMenuMelee.enabled = AddRetailVrMenuControl(
        L"Melee: Gestures", kVrMenuToggleMelee);
    g_vrMenuMelee.disabled = AddRetailVrMenuControl(
        L"Melee: Classic", kVrMenuToggleMelee);
    g_vrMenuMeleeWeaponStrike.enabled = AddRetailVrMenuControl(
        L"Weapon-hand strike: On", kVrMenuToggleMeleeWeaponStrike);
    g_vrMenuMeleeWeaponStrike.disabled = AddRetailVrMenuControl(
        L"Weapon-hand strike: Off", kVrMenuToggleMeleeWeaponStrike);
    g_vrMenuMeleeOffHandStrike.enabled = AddRetailVrMenuControl(
        L"Off-hand strike: On", kVrMenuToggleMeleeOffHandStrike);
    g_vrMenuMeleeOffHandStrike.disabled = AddRetailVrMenuControl(
        L"Off-hand strike: Off", kVrMenuToggleMeleeOffHandStrike);
    g_vrMenuMeleeJumpKick.enabled = AddRetailVrMenuControl(
        L"Jump kick gesture: On", kVrMenuToggleMeleeJumpKick);
    g_vrMenuMeleeJumpKick.disabled = AddRetailVrMenuControl(
        L"Jump kick gesture: Off", kVrMenuToggleMeleeJumpKick);
    g_vrMenuMeleeSlideKick.enabled = AddRetailVrMenuControl(
        L"Slide kick gesture: On", kVrMenuToggleMeleeSlideKick);
    g_vrMenuMeleeSlideKick.disabled = AddRetailVrMenuControl(
        L"Slide kick gesture: Off", kVrMenuToggleMeleeSlideKick);
    g_vrMenuArms.enabled = AddRetailVrMenuControl(
        L"Show arms: On", kVrMenuToggleArms);
    g_vrMenuArms.disabled = AddRetailVrMenuControl(
        L"Show arms: Off", kVrMenuToggleArms);
    g_vrMenuTwoHandGrip.enabled = AddRetailVrMenuControl(
        L"Two-handed grip: On", kVrMenuToggleTwoHandGrip);
    g_vrMenuTwoHandGrip.disabled = AddRetailVrMenuControl(
        L"Two-handed grip: Off", kVrMenuToggleTwoHandGrip);
    g_vrMenuWeaponWeight.enabled = AddRetailVrMenuControl(
        L"Simulated weapon weight: On", kVrMenuToggleWeaponWeight);
    g_vrMenuWeaponWeight.disabled = AddRetailVrMenuControl(
        L"Simulated weapon weight: Off", kVrMenuToggleWeaponWeight);
    g_vrMenuWeaponRecoil.enabled = AddRetailVrMenuControl(
        L"Weapon recoil: On", kVrMenuToggleWeaponRecoil);
    g_vrMenuWeaponRecoil.disabled = AddRetailVrMenuControl(
        L"Weapon recoil: Off", kVrMenuToggleWeaponRecoil);
    g_vrMenuWeaponWeightDiagnostics.enabled = AddRetailVrMenuControl(
        L"Weapon diagnostics: On", kVrMenuToggleWeaponWeightDiagnostics);
    g_vrMenuWeaponWeightDiagnostics.disabled = AddRetailVrMenuControl(
        L"Weapon diagnostics: Off", kVrMenuToggleWeaponWeightDiagnostics);
    g_vrMenuWeaponProfileTarget.enabled = AddRetailVrMenuControl(
        L"Tuning profile: Current weapon", kVrMenuToggleWeaponProfileTarget);
    g_vrMenuWeaponProfileTarget.disabled = AddRetailVrMenuControl(
        L"Tuning profile: Default", kVrMenuToggleWeaponProfileTarget);
    g_vrMenuFovScale[0] = AddRetailVrMenuControl(
        L"FOV scale: 100%", kVrMenuCycleFovScale);
    g_vrMenuFovScale[1] = AddRetailVrMenuControl(
        L"FOV scale: 110%", kVrMenuCycleFovScale);
    g_vrMenuFovScale[2] = AddRetailVrMenuControl(
        L"FOV scale: 120%", kVrMenuCycleFovScale);
    g_vrMenuFovScale[3] = AddRetailVrMenuControl(
        L"FOV scale: 130%", kVrMenuCycleFovScale);
    g_vrMenuTurnSpeed[0] = AddRetailVrMenuControl(
        L"Smooth turn: Slow",
        kVrMenuCycleTurnSpeed);
    g_vrMenuTurnSpeed[1] = AddRetailVrMenuControl(
        L"Smooth turn: Normal",
        kVrMenuCycleTurnSpeed);
    g_vrMenuTurnSpeed[2] = AddRetailVrMenuControl(
        L"Smooth turn: Fast",
        kVrMenuCycleTurnSpeed);
    g_vrMenuWeaponWeightValue[0] = AddRetailVrMenuControl(
        L"Weight: 0.25x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponWeightValue[1] = AddRetailVrMenuControl(
        L"Weight: 0.50x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponWeightValue[2] = AddRetailVrMenuControl(
        L"Weight: 0.75x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponWeightValue[3] = AddRetailVrMenuControl(
        L"Weight: 1.00x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponWeightValue[4] = AddRetailVrMenuControl(
        L"Weight: 1.50x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponWeightValue[5] = AddRetailVrMenuControl(
        L"Weight: 2.00x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponWeightValue[6] = AddRetailVrMenuControl(
        L"Weight: 3.00x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponWeightValue[7] = AddRetailVrMenuControl(
        L"Weight: 4.00x", kVrMenuCycleWeaponWeight);
    g_vrMenuWeaponPositionFollow[0] = AddRetailVrMenuControl(
        L"Position follow: 6", kVrMenuCycleWeaponPositionFollow);
    g_vrMenuWeaponPositionFollow[1] = AddRetailVrMenuControl(
        L"Position follow: 10", kVrMenuCycleWeaponPositionFollow);
    g_vrMenuWeaponPositionFollow[2] = AddRetailVrMenuControl(
        L"Position follow: 14", kVrMenuCycleWeaponPositionFollow);
    g_vrMenuWeaponPositionFollow[3] = AddRetailVrMenuControl(
        L"Position follow: 18", kVrMenuCycleWeaponPositionFollow);
    g_vrMenuWeaponPositionFollow[4] = AddRetailVrMenuControl(
        L"Position follow: 24", kVrMenuCycleWeaponPositionFollow);
    g_vrMenuWeaponPositionFollow[5] = AddRetailVrMenuControl(
        L"Position follow: 32", kVrMenuCycleWeaponPositionFollow);
    g_vrMenuWeaponPositionFollow[6] = AddRetailVrMenuControl(
        L"Position follow: 40", kVrMenuCycleWeaponPositionFollow);
    g_vrMenuWeaponRotationFollow[0] = AddRetailVrMenuControl(
        L"Rotation follow: 6", kVrMenuCycleWeaponRotationFollow);
    g_vrMenuWeaponRotationFollow[1] = AddRetailVrMenuControl(
        L"Rotation follow: 10", kVrMenuCycleWeaponRotationFollow);
    g_vrMenuWeaponRotationFollow[2] = AddRetailVrMenuControl(
        L"Rotation follow: 14", kVrMenuCycleWeaponRotationFollow);
    g_vrMenuWeaponRotationFollow[3] = AddRetailVrMenuControl(
        L"Rotation follow: 20", kVrMenuCycleWeaponRotationFollow);
    g_vrMenuWeaponRotationFollow[4] = AddRetailVrMenuControl(
        L"Rotation follow: 26", kVrMenuCycleWeaponRotationFollow);
    g_vrMenuWeaponRotationFollow[5] = AddRetailVrMenuControl(
        L"Rotation follow: 32", kVrMenuCycleWeaponRotationFollow);
    g_vrMenuWeaponRotationFollow[6] = AddRetailVrMenuControl(
        L"Rotation follow: 40", kVrMenuCycleWeaponRotationFollow);
    g_vrMenuWeaponCatchUp[0] = AddRetailVrMenuControl(
        L"Catch-up strength: 0.0", kVrMenuCycleWeaponCatchUp);
    g_vrMenuWeaponCatchUp[1] = AddRetailVrMenuControl(
        L"Catch-up strength: 0.5", kVrMenuCycleWeaponCatchUp);
    g_vrMenuWeaponCatchUp[2] = AddRetailVrMenuControl(
        L"Catch-up strength: 1.0", kVrMenuCycleWeaponCatchUp);
    g_vrMenuWeaponCatchUp[3] = AddRetailVrMenuControl(
        L"Catch-up strength: 1.5", kVrMenuCycleWeaponCatchUp);
    g_vrMenuWeaponCatchUp[4] = AddRetailVrMenuControl(
        L"Catch-up strength: 2.0", kVrMenuCycleWeaponCatchUp);
    g_vrMenuWeaponCatchUp[5] = AddRetailVrMenuControl(
        L"Catch-up strength: 3.0", kVrMenuCycleWeaponCatchUp);
    g_vrMenuWeaponCatchUp[6] = AddRetailVrMenuControl(
        L"Catch-up strength: 4.0", kVrMenuCycleWeaponCatchUp);
    g_vrMenuWeaponRecoilStrength[0] = AddRetailVrMenuControl(
        L"Recoil strength: 50%", kVrMenuCycleWeaponRecoilStrength);
    g_vrMenuWeaponRecoilStrength[1] = AddRetailVrMenuControl(
        L"Recoil strength: 100%", kVrMenuCycleWeaponRecoilStrength);
    g_vrMenuWeaponRecoilStrength[2] = AddRetailVrMenuControl(
        L"Recoil strength: 150%", kVrMenuCycleWeaponRecoilStrength);
    g_vrMenuWeaponRecoilStrength[3] = AddRetailVrMenuControl(
        L"Recoil strength: 200%", kVrMenuCycleWeaponRecoilStrength);
    g_vrMenuWeaponRecoilStrength[4] = AddRetailVrMenuControl(
        L"Recoil strength: 300%", kVrMenuCycleWeaponRecoilStrength);
    g_vrMenuWeaponRecoilStrength[5] = AddRetailVrMenuControl(
        L"Recoil strength: 500%", kVrMenuCycleWeaponRecoilStrength);
    g_vrMenuWeaponRecoilRise[0] = AddRetailVrMenuControl(
        L"Muzzle rise: None", kVrMenuCycleWeaponRecoilRise);
    g_vrMenuWeaponRecoilRise[1] = AddRetailVrMenuControl(
        L"Muzzle rise: 50%", kVrMenuCycleWeaponRecoilRise);
    g_vrMenuWeaponRecoilRise[2] = AddRetailVrMenuControl(
        L"Muzzle rise: 100%", kVrMenuCycleWeaponRecoilRise);
    g_vrMenuWeaponRecoilRise[3] = AddRetailVrMenuControl(
        L"Muzzle rise: 150%", kVrMenuCycleWeaponRecoilRise);
    g_vrMenuWeaponRecoilRise[4] = AddRetailVrMenuControl(
        L"Muzzle rise: 200%", kVrMenuCycleWeaponRecoilRise);
    g_vrMenuWeaponRecoilRise[5] = AddRetailVrMenuControl(
        L"Muzzle rise: 300%", kVrMenuCycleWeaponRecoilRise);
    g_vrMenuWeaponRecoilRecovery[0] = AddRetailVrMenuControl(
        L"Recovery speed: 50%", kVrMenuCycleWeaponRecoilRecovery);
    g_vrMenuWeaponRecoilRecovery[1] = AddRetailVrMenuControl(
        L"Recovery speed: 75%", kVrMenuCycleWeaponRecoilRecovery);
    g_vrMenuWeaponRecoilRecovery[2] = AddRetailVrMenuControl(
        L"Recovery speed: 100%", kVrMenuCycleWeaponRecoilRecovery);
    g_vrMenuWeaponRecoilRecovery[3] = AddRetailVrMenuControl(
        L"Recovery speed: 150%", kVrMenuCycleWeaponRecoilRecovery);
    g_vrMenuWeaponRecoilRecovery[4] = AddRetailVrMenuControl(
        L"Recovery speed: 200%", kVrMenuCycleWeaponRecoilRecovery);
    g_vrMenuWeaponRecoilRecovery[5] = AddRetailVrMenuControl(
        L"Recovery speed: 300%", kVrMenuCycleWeaponRecoilRecovery);
    g_vrMenuResetWeaponProfile = AddRetailVrMenuControl(
        L"Reset selected weapon profile", kVrMenuResetWeaponProfile);
    g_vrMenuRecenter = AddRetailVrMenuControl(
        L"Recenter VR origin", kVrMenuRecenter);
    g_vrMenuDefaults = AddRetailVrMenuControl(
        L"Reset VR defaults", kVrMenuDefaults);
    g_vrMenuBack = AddRetailVrMenuControl(
        L"Back", kVrMenuBack);

    if (g_vrMenuOpenDisplay.object == nullptr ||
        g_vrMenuOpenAdvanced.object == nullptr ||
        g_vrMenuOpenWeaponRecoil.object == nullptr ||
        g_vrMenuWeaponRecoilRecovery[0].object == nullptr ||
        g_vrMenuResetWeaponProfile.object == nullptr ||
        g_vrMenuBack.object == nullptr) {
        return false;
    }
    HideRetailVrSettingsControls();
    MarkRetailVrMenuForLayout();
    g_vrMenuControlsBuilt = true;
    Report(
        "INFO", "vr_settings_menu_built",
        "VR SETTINGS was inserted after Options in the "
        "Retail pause menu.");
    return true;
}

bool __fastcall HookRetailMenuInit(
    void* menu, void* ignoredEdx, void* menuManager) {
    (void)ignoredEdx;
    const bool initialized =
        g_retailMenuInit(menu, menuManager);
    if (!initialized) {
        return false;
    }
    if (!BuildRetailVrMenuControls(menu)) {
        Report(
            "WARN", "vr_settings_menu_build_failed",
            "The verified Retail menu initialized, but its control "
            "list rejected the VR settings entries.");
    }
    return true;
}

std::uint32_t __fastcall HookRetailMenuOnCommand(
    void* menu, void* ignoredEdx, std::uint32_t command,
    std::uint32_t parameter1, std::uint32_t parameter2) {
    (void)ignoredEdx;
    if (menu != g_vrMenuOwner || !g_vrMenuControlsBuilt) {
        return g_retailMenuOnCommand(
            menu, command, parameter1, parameter2);
    }

    VrMenuControl selection;
    switch (command) {
    case kVrMenuOpen:
        EnterRetailVrSettingsPage();
        return 1;
    case kVrMenuOpenDisplay:
        ShowRetailVrSettingsPage(VrSettingsPage::Display);
        return 1;
    case kVrMenuOpenComfort:
        ShowRetailVrSettingsPage(VrSettingsPage::Comfort);
        return 1;
    case kVrMenuOpenControls:
        ShowRetailVrSettingsPage(VrSettingsPage::Controls);
        return 1;
    case kVrMenuOpenWeapons:
        ShowRetailVrSettingsPage(VrSettingsPage::Weapons);
        return 1;
    case kVrMenuOpenWeaponHandling:
        ShowRetailVrSettingsPage(VrSettingsPage::WeaponHandling);
        return 1;
    case kVrMenuOpenWeaponWeight:
        ShowRetailVrSettingsPage(VrSettingsPage::WeaponWeight);
        return 1;
    case kVrMenuOpenWeaponRecoil:
        ShowRetailVrSettingsPage(VrSettingsPage::WeaponRecoil);
        return 1;
    case kVrMenuOpenMelee:
        ShowRetailVrSettingsPage(VrSettingsPage::Melee);
        return 1;
    case kVrMenuOpenAdvanced:
        ShowRetailVrSettingsPage(VrSettingsPage::Advanced);
        return 1;
    case kVrMenuToggleStereo:
        SetBooleanOption(
            g_setStereoEnabled,
            !QueryBooleanOption(g_isStereoEnabled, true));
        selection = SetRetailVrToggleVisible(
            g_vrMenuStereo,
            QueryBooleanOption(g_isStereoEnabled, true));
        break;
    case kVrMenuToggleTranslation:
        SetBooleanOption(
            g_setTranslationEnabled,
            !QueryBooleanOption(g_isTranslationEnabled, false));
        selection = SetRetailVrToggleVisible(
            g_vrMenuTranslation,
            QueryBooleanOption(g_isTranslationEnabled, false));
        break;
    case kVrMenuToggleStereoHud:
        SetBooleanOption(
            g_setStereoHudEnabled,
            !QueryBooleanOption(g_isStereoHudEnabled, true));
        selection = SetRetailVrToggleVisible(
            g_vrMenuStereoHud,
            QueryBooleanOption(g_isStereoHudEnabled, true));
        break;
    case kVrMenuToggleHeadBob:
        if (g_headBobEnabled || !g_forceHeadBobDisabled) {
            ApplyHeadBobEnabled(!g_headBobEnabled);
        }
        selection = SetRetailVrToggleVisible(
            g_vrMenuHeadBob, g_headBobEnabled);
        break;
    case kVrMenuToggleComfort:
        SetBooleanOption(
            g_setComfortModeEnabled,
            !QueryBooleanOption(g_isComfortModeEnabled, false));
        selection = SetRetailVrToggleVisible(
            g_vrMenuComfort,
            QueryBooleanOption(g_isComfortModeEnabled, false));
        break;
    case kVrMenuCycleWeaponWeightPreset: {
        const int next =
            (static_cast<int>(g_weaponWeightPreset) + 1) % 4;
        g_weaponWeightPreset = SanitizeWeaponWeightPreset(next);
        g_weaponWeightEnabled =
            g_weaponWeightPreset != WeaponWeightPreset::none;
        for (int index = 0; index < 4; ++index) {
            SetRetailControlVisible(
                g_vrMenuWeaponWeightPreset[index], index == next);
        }
        selection = g_vrMenuWeaponWeightPreset[next];
        g_weightedWeaponInput = {};
        ResetWeaponWeightPair(
            g_weightedWeaponInput.filters,
            WeaponWeightResetReason::enabledChanged);
        static constexpr const char* kPresetNames[]{
            "none", "light", "medium", "heavy"};
        char message[96]{};
        std::snprintf(
            message, sizeof(message),
            "Simulated weapon weight preset changed to %s.",
            kPresetNames[next]);
        Report("INFO", "vr_weapon_weight_changed", message);
        break;
    }
    case kVrMenuToggleAimGuide:
        g_weaponAimGuideEnabled = !g_weaponAimGuideEnabled;
        selection = SetRetailVrToggleVisible(
            g_vrMenuAimGuide, g_weaponAimGuideEnabled);
        break;
    case kVrMenuToggleHaptics:
        g_controllerHapticsEnabled =
            !g_controllerHapticsEnabled;
        selection = SetRetailVrToggleVisible(
            g_vrMenuHaptics, g_controllerHapticsEnabled);
        break;
    case kVrMenuCycleFlashlightMount: {
        const int next =
            (static_cast<int>(g_flashlightMount) + 1) %
            kFlashlightMountCount;
        g_flashlightMount = static_cast<FlashlightMount>(next);
        for (int index = 0;
             index < kFlashlightMountCount; ++index) {
            SetRetailControlVisible(
                g_vrMenuFlashlightMount[index],
                index == next);
        }
        selection = g_vrMenuFlashlightMount[next];
        const char* description = "left hand";
        if (g_flashlightMount == FlashlightMount::Head) {
            description = "head";
        } else if (
            g_flashlightMount == FlashlightMount::Weapon) {
            description = "weapon";
        }
        char message[112]{};
        std::snprintf(
            message, sizeof(message),
            "The VR flashlight is now mounted on the %s.",
            description);
        Report(
            "INFO", "vr_flashlight_mount_changed", message);
        break;
    }
    case kVrMenuToggleTwoHandGrip:
        g_twoHandedGripEnabled = !g_twoHandedGripEnabled;
        if (!g_twoHandedGripEnabled) {
            g_twoHandedGripActive = false;
            g_twoHandedGrip = TwoHandedGripState{};
        }
        selection = SetRetailVrToggleVisible(
            g_vrMenuTwoHandGrip, g_twoHandedGripEnabled);
        break;
    case kVrMenuToggleHandedness:
        g_leftHandedBindings = !g_leftHandedBindings;
        // Der gespiegelte Zustand betrifft auch die Handkalibrierung und die
        // gepufferten Posen: beide gehoeren jetzt der jeweils anderen Hand.
        ResetVrTrackingBasis();
        selection = SetRetailVrToggleVisible(
            g_vrMenuHandedness, !g_leftHandedBindings);
        Report(
            "INFO", "vr_handedness_changed",
            g_leftHandedBindings
                ? "Controller bindings are mirrored for left-handed play."
                : "Controller bindings use the right-handed default.");
        break;
    case kVrMenuToggleHeadRelativeMovement:
        g_headRelativeMovement = !g_headRelativeMovement;
        selection = SetRetailVrToggleVisible(
            g_vrMenuMovementDirection, g_headRelativeMovement);
        Report(
            "INFO", "vr_movement_direction_changed",
            g_headRelativeMovement
                ? "Forward movement follows horizontal HMD direction."
                : "Forward movement follows the Retail player body.");
        break;
    case kVrMenuToggleClimbing:
        g_climbingEnabled = !g_climbingEnabled;
        // Ein Griff, der beim Umschalten noch stand, gehoert der alten
        // Einstellung: sonst klettert der naechste Zug aus einem Griff, den
        // es in dieser Betriebsart nie gab.
        ResetClimbGrip(g_climbGrip);
        g_climbActive = false;
        g_climbOnLadder = false;
        g_climbAxis = 0.0F;
        g_climbWasGripping = false;
        selection = SetRetailVrToggleVisible(
            g_vrMenuClimbing, g_climbingEnabled);
        Report(
            "INFO", "vr_climbing_changed",
            g_climbingEnabled
                ? "Ladders are climbed by grabbing with the hands."
                : "Ladders use the Retail stick controls.");
        break;
    case kVrMenuToggleMelee:
        g_meleeThrustEnabled = !g_meleeThrustEnabled;
        // Keine alte Pose oder ein bereits gestarteter Puls darf die neue
        // Betriebsart ueberleben.
        ResetMeleeActions(g_meleeActions);
        g_meleePulseUntil = 0;
        g_slideDuckPulseUntil = 0;
        g_slideForwardPulseUntil = 0;
        RefreshRetailVrSettingsControls();
        selection = SetRetailVrToggleVisible(
            g_vrMenuMelee, g_meleeThrustEnabled);
        Report(
            "INFO", "vr_melee_changed",
            g_meleeThrustEnabled
                ? "Motion-controller melee gestures are enabled."
                : "Melee uses classic Retail controls only.");
        break;
    case kVrMenuToggleMeleeWeaponStrike:
        g_meleeWeaponStrikeEnabled = !g_meleeWeaponStrikeEnabled;
        ResetMeleeActions(g_meleeActions);
        selection = SetRetailVrToggleVisible(
            g_vrMenuMeleeWeaponStrike,
            g_meleeWeaponStrikeEnabled);
        break;
    case kVrMenuToggleMeleeOffHandStrike:
        g_meleeOffHandStrikeEnabled = !g_meleeOffHandStrikeEnabled;
        ResetMeleeActions(g_meleeActions);
        selection = SetRetailVrToggleVisible(
            g_vrMenuMeleeOffHandStrike,
            g_meleeOffHandStrikeEnabled);
        break;
    case kVrMenuToggleMeleeJumpKick:
        g_meleeJumpKickEnabled = !g_meleeJumpKickEnabled;
        ResetMeleeActions(g_meleeActions);
        selection = SetRetailVrToggleVisible(
            g_vrMenuMeleeJumpKick,
            g_meleeJumpKickEnabled);
        break;
    case kVrMenuToggleMeleeSlideKick:
        g_meleeSlideKickEnabled = !g_meleeSlideKickEnabled;
        ResetMeleeActions(g_meleeActions);
        selection = SetRetailVrToggleVisible(
            g_vrMenuMeleeSlideKick,
            g_meleeSlideKickEnabled);
        break;
    case kVrMenuToggleArms:
        g_showPlayerArms = !g_showPlayerArms;
        selection = SetRetailVrToggleVisible(
            g_vrMenuArms, g_showPlayerArms);
        Report(
            "INFO", "vr_player_arms_changed",
            g_showPlayerArms
                ? "Player upper and lower arms use the visible Retail "
                  "material."
                : "Player upper and lower arms use the transparent VR "
                  "material; hands, torso and legs remain visible.");
        break;
    case kVrMenuToggleWeaponWeight:
        g_weaponWeightEnabled = !g_weaponWeightEnabled;
        ResetWeaponWeightPair(
            g_weightedWeaponInput.filters,
            WeaponWeightResetReason::enabledChanged);
        selection = SetRetailVrToggleVisible(
            g_vrMenuWeaponWeight, g_weaponWeightEnabled);
        break;
    case kVrMenuToggleWeaponRecoil:
        g_weaponRecoilEnabled = !g_weaponRecoilEnabled;
        ResetWeaponRecoil(g_weightedWeaponInput.recoil);
        InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
        g_lastWeaponRecoilTick = 0;
        selection = SetRetailVrToggleVisible(
            g_vrMenuWeaponRecoil, g_weaponRecoilEnabled);
        break;
    case kVrMenuToggleWeaponWeightDiagnostics:
        g_weaponWeightDiagnosticsEnabled =
            !g_weaponWeightDiagnosticsEnabled;
        selection = SetRetailVrToggleVisible(
            g_vrMenuWeaponWeightDiagnostics,
            g_weaponWeightDiagnosticsEnabled);
        break;
    case kVrMenuToggleWeaponProfileTarget:
        if (HasCurrentWeaponWeightProfile()) {
            if (g_vrSettingsPage == VrSettingsPage::WeaponRecoil) {
                g_vrRecoilProfileEditsCurrent =
                    !g_vrRecoilProfileEditsCurrent;
                if (g_vrRecoilProfileEditsCurrent) {
                    (void)EditableWeaponRecoilProfile();
                }
            } else {
                g_vrWeaponProfileEditsCurrent =
                    !g_vrWeaponProfileEditsCurrent;
            }
        } else {
            g_vrWeaponProfileEditsCurrent = false;
            g_vrRecoilProfileEditsCurrent = false;
        }
        RefreshRetailVrSettingsControls();
        selection = SetRetailVrToggleVisible(
            g_vrMenuWeaponProfileTarget,
            g_vrSettingsPage == VrSettingsPage::WeaponRecoil
                ? EditingCurrentWeaponRecoilProfile()
                : EditingCurrentWeaponWeightProfile());
        break;
    case kVrMenuResetWeaponProfile:
        ResetEditableWeaponWeightProfile();
        RefreshRetailVrSettingsControls();
        selection = g_vrMenuResetWeaponProfile;
        break;
    case kVrMenuCycleFovScale: {
        g_fovScalePreset = (g_fovScalePreset + 1) % 4;
        for (int index = 0; index < 4; ++index) {
            SetRetailControlVisible(
                g_vrMenuFovScale[index],
                index == g_fovScalePreset);
        }
        ApplyFovScalePreset();
        selection = g_vrMenuFovScale[g_fovScalePreset];
        char message[96]{};
        std::snprintf(
            message, sizeof(message),
            "Stereo camera and OpenXR projection FOV scale set to %u%%.",
            CurrentFovScalePercent());
        Report("INFO", "vr_fov_scale_changed", message);
        break;
    }
    case kVrMenuTogglePhysicalLean:
        g_physicalLeanEnabled = !g_physicalLeanEnabled;
        ResetLeanCollision(g_leanCollision);
        g_leanTranslationScale = 1.0F;
        selection = SetRetailVrToggleVisible(
            g_vrMenuPhysicalLean, g_physicalLeanEnabled);
        Report(
            "INFO", "vr_physical_lean_changed",
            g_physicalLeanEnabled
                ? "Leaning moves the viewpoint, limited by the world."
                : "Leaning only tilts the camera, as in the original game.");
        break;
    case kVrMenuTogglePhysicalDuck:
        g_physicalDuckEnabled = !g_physicalDuckEnabled;
        g_physicalDuckActive = false;
        selection = SetRetailVrToggleVisible(
            g_vrMenuPhysicalDuck, g_physicalDuckEnabled);
        Report(
            "INFO", "vr_physical_duck_changed",
            g_physicalDuckEnabled
                ? "Lowering the headset engages Retail crouch."
                : "Only the classic stick crouch engages Retail crouch.");
        break;
    case kVrMenuCycleLeanScale: {
        const std::size_t index = NextVrPresetIndex(
            g_leanScalePercent, kLeanScalePresets);
        g_leanScalePercent = kLeanScalePresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuLeanScale, index);
        break;
    }
    case kVrMenuCycleTurnSpeed:
        g_turnSpeedPreset = (g_turnSpeedPreset + 1) % 3;
        for (int index = 0; index < 3; ++index) {
            SetRetailControlVisible(
                g_vrMenuTurnSpeed[index],
                index == g_turnSpeedPreset);
        }
        selection = g_vrMenuTurnSpeed[g_turnSpeedPreset];
        break;
    case kVrMenuCycleWeaponWeight: {
        WeaponWeightProfile& profile = EditableWeaponWeightProfile();
        const std::size_t index = NextVrPresetIndex(
            profile.weight, kWeaponWeightPresets);
        profile.weight = kWeaponWeightPresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuWeaponWeightValue, index);
        break;
    }
    case kVrMenuCycleWeaponPositionFollow: {
        WeaponWeightProfile& profile = EditableWeaponWeightProfile();
        const std::size_t index = NextVrPresetIndex(
            profile.positionalFollow, kWeaponPositionFollowPresets);
        profile.positionalFollow = kWeaponPositionFollowPresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuWeaponPositionFollow, index);
        break;
    }
    case kVrMenuCycleWeaponRotationFollow: {
        WeaponWeightProfile& profile = EditableWeaponWeightProfile();
        const std::size_t index = NextVrPresetIndex(
            profile.rotationalFollow, kWeaponRotationFollowPresets);
        profile.rotationalFollow = kWeaponRotationFollowPresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuWeaponRotationFollow, index);
        break;
    }
    case kVrMenuCycleWeaponCatchUp: {
        WeaponWeightProfile& profile = EditableWeaponWeightProfile();
        const std::size_t index = NextVrPresetIndex(
            profile.catchUpStrength, kWeaponCatchUpPresets);
        profile.catchUpStrength = kWeaponCatchUpPresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuWeaponCatchUp, index);
        break;
    }
    case kVrMenuCycleWeaponRecoilStrength: {
        WeaponRecoilProfile& profile = EditableWeaponRecoilProfile();
        const std::size_t index = NextVrPresetIndex(
            profile.strength,
            kWeaponRecoilStrengthPresets);
        profile.strength = kWeaponRecoilStrengthPresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuWeaponRecoilStrength, index);
        break;
    }
    case kVrMenuCycleWeaponRecoilRise: {
        WeaponRecoilProfile& profile = EditableWeaponRecoilProfile();
        const std::size_t index = NextVrPresetIndex(
            profile.muzzleRise,
            kWeaponRecoilRisePresets);
        profile.muzzleRise = kWeaponRecoilRisePresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuWeaponRecoilRise, index);
        break;
    }
    case kVrMenuCycleWeaponRecoilRecovery: {
        WeaponRecoilProfile& profile = EditableWeaponRecoilProfile();
        const std::size_t index = NextVrPresetIndex(
            profile.recovery,
            kWeaponRecoilRecoveryPresets);
        profile.recovery = kWeaponRecoilRecoveryPresets[index];
        selection = SetRetailVrPresetVisible(
            g_vrMenuWeaponRecoilRecovery, index);
        break;
    }
    case kVrMenuRecenter:
        RequestVrOriginRecenter();
        selection = g_vrMenuRecenter;
        break;
    case kVrMenuDefaults:
        ApplyVrDefaults();
        RefreshRetailVrSettingsControls();
        selection = g_vrMenuDefaults;
        break;
    case kVrMenuBack:
        NavigateBackRetailVrSettingsPage();
        return 1;
    default:
        return g_retailMenuOnCommand(
            menu, command, parameter1, parameter2);
    }

    MarkRetailVrMenuForLayout();
    SelectRetailVrMenuControl(selection);
    ResetRetailVrMenuScroll();
    SaveVrSettings();
    return 1;
}

void __fastcall HookRetailMenuOnFocus(
    void* menu, void* ignoredEdx, bool focus) {
    (void)ignoredEdx;
    if (menu == g_vrMenuOwner && g_vrMenuControlsBuilt) {
        if (g_vrSettingsPageActive) {
            ResetRetailVrMenuScroll();
        }
        g_vrMenuFirstVisibleRow = 0;
        g_vrSettingsPageActive = false;
        RestoreRetailSystemMenuControls();
        // Fokusgewinn ist eine sichere Aussage: Das Pausenmenü ist offen.
        // Fokusverlust ist es ausdrücklich nicht. Optionen, Speichern und
        // Laden sind eigene Menüobjekte, die dem Systemmenü den Fokus
        // entziehen. Würde hier "kein Menü" angenommen, verlöre das
        // Untermenü seine Controllersteuerung und der Bildpfad schaltete
        // zurück auf die Welt. Deshalb übernimmt dann wieder die Heuristik
        // über die Frische des Weapon-Manager-Updates.
        if (focus) {
            g_menuActivationHoldUntil = GetTickCount64() + 1000;
            g_menuFocusKnown = true;
            g_menuFocusActive = true;
            if (g_setMenuActive != nullptr) {
                __try {
                    g_setMenuActive(TRUE);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    Report(
                        "WARN", "menu_render_mode_failed",
                        "The bridge rejected the Retail menu focus update.");
                }
            }
        } else {
            g_menuActivationHoldUntil = 0;
            g_menuFocusKnown = false;
        }
    }
    g_retailMenuOnFocus(menu, focus);
}

void RemoveRetailVrMenuHooks() noexcept {
    const void* targets[] = {
        g_retailMenuInitTarget,
        g_retailMenuOnCommandTarget,
        g_retailMenuOnFocusTarget,
    };
    for (const void* target : targets) {
        if (target != nullptr) {
            MH_DisableHook(const_cast<void*>(target));
            MH_RemoveHook(const_cast<void*>(target));
        }
    }
    g_retailMenuInitTarget = nullptr;
    g_retailMenuOnCommandTarget = nullptr;
    g_retailMenuOnFocusTarget = nullptr;
    g_retailMenuInit = nullptr;
    g_retailMenuOnCommand = nullptr;
    g_retailMenuOnFocus = nullptr;
}

bool InstallRetailVrMenuHooks() noexcept {
    HMODULE retail = GetModuleHandleW(L"GameOrig.dll");
    if (retail == nullptr) {
        return false;
    }
    auto* const base =
        reinterpret_cast<unsigned char*>(retail);
    auto* const init = base + kRetailMenuSystemInitRva;
    auto* const onCommand =
        base + kRetailMenuSystemOnCommandRva;
    auto* const onFocus = base + kRetailMenuSystemOnFocusRva;
    auto* const getControl =
        base + kRetailListGetControlRva;
    auto* const swapItems =
        base + kRetailListSwapItemsRva;
    if (!MatchesCode(
            init, kRetailMenuInitPrefix,
            sizeof(kRetailMenuInitPrefix)) ||
        !MatchesCode(
            onCommand, kRetailMenuOnCommandPrefix,
            sizeof(kRetailMenuOnCommandPrefix)) ||
        !MatchesCode(
            onFocus, kRetailMenuOnFocusPrefix,
            sizeof(kRetailMenuOnFocusPrefix)) ||
        !MatchesCode(
            getControl, kRetailListGetControlPrefix,
            sizeof(kRetailListGetControlPrefix)) ||
        !MatchesCode(
            swapItems, kRetailListSwapItemsPrefix,
            sizeof(kRetailListSwapItemsPrefix))) {
        Report(
            "ERROR", "vr_settings_menu_layout_mismatch",
            "Retail 1.08 menu signatures did not match; the pause "
            "menu remains untouched.");
        return false;
    }

    g_retailMenuAddControl =
        reinterpret_cast<RetailMenuAddControlFunction>(
            base + kRetailBaseMenuAddWideControlThunkRva);
    g_retailListGetControl =
        reinterpret_cast<RetailListGetControlFunction>(getControl);
    g_retailListSwapItems =
        reinterpret_cast<RetailListSwapItemsFunction>(swapItems);
    g_retailListSetSelection =
        reinterpret_cast<RetailListSetSelectionFunction>(
            base + kRetailListSetSelectionRva);

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK &&
        initialize != MH_ERROR_ALREADY_INITIALIZED) {
        Report(
            "ERROR", "vr_settings_menu_hook_initialize_failed",
            MH_StatusToString(initialize));
        return false;
    }
    g_retailMenuInitTarget = init;
    g_retailMenuOnCommandTarget = onCommand;
    g_retailMenuOnFocusTarget = onFocus;
    MH_STATUS status = MH_CreateHook(
        init, reinterpret_cast<void*>(&HookRetailMenuInit),
        reinterpret_cast<void**>(&g_retailMenuInit));
    if (status == MH_OK) {
        status = MH_CreateHook(
            onCommand,
            reinterpret_cast<void*>(&HookRetailMenuOnCommand),
            reinterpret_cast<void**>(&g_retailMenuOnCommand));
    }
    if (status == MH_OK) {
        status = MH_CreateHook(
            onFocus,
            reinterpret_cast<void*>(&HookRetailMenuOnFocus),
            reinterpret_cast<void**>(&g_retailMenuOnFocus));
    }
    if (status == MH_OK) {
        status = MH_EnableHook(init);
    }
    if (status == MH_OK) {
        status = MH_EnableHook(onCommand);
    }
    if (status == MH_OK) {
        status = MH_EnableHook(onFocus);
    }
    if (status != MH_OK) {
        Report(
            "ERROR", "vr_settings_menu_hook_install_failed",
            MH_StatusToString(status));
        RemoveRetailVrMenuHooks();
        return false;
    }
    Report(
        "INFO", "vr_settings_menu_hooks_installed",
        "Verified Retail CMenuSystem Init, OnCommand and OnFocus "
        "hooks are active.");
    return true;
}

// Zeigen ersetzt das Hinlaufen, also muss die Retail-Reichweite ueber die
// Nasenlaenge hinausgehen. 60 Einheiten sind in LithTech-Zoll rund 1,5 m:
// bequem aus dem Stand erreichbar, aber nicht quer durch den Raum. Der Wert
// wird nur angehoben, nie gesenkt, damit ein groesserer Retail- oder
// Missionswert erhalten bleibt.
constexpr float kVrInteractionReach = 60.0F;

void UpdateInteractionReachOverride(bool stereoEnabled) noexcept {
    if (g_disableInteractionHooks ||
        g_retailCheckForIntersectTarget == nullptr) {
        return;
    }
    if (!g_interactionReachOriginalKnown) {
        __try {
            const HCONSOLEVAR activation =
                g_client->GetConsoleVariable("ActivationDistance");
            const HCONSOLEVAR pickup =
                g_client->GetConsoleVariable("PickupDistance");
            if (activation == nullptr || pickup == nullptr) {
                return;
            }
            g_activationReachOriginal =
                g_client->GetConsoleVariableFloat(activation);
            g_pickupReachOriginal =
                g_client->GetConsoleVariableFloat(pickup);
            g_interactionReachOriginalKnown = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
    }
    if (stereoEnabled == g_interactionReachApplied) {
        return;
    }
    const float activationValue =
        stereoEnabled
            ? (g_activationReachOriginal > kVrInteractionReach
                   ? g_activationReachOriginal
                   : kVrInteractionReach)
            : g_activationReachOriginal;
    const float pickupValue =
        stereoEnabled
            ? (g_pickupReachOriginal > kVrInteractionReach
                   ? g_pickupReachOriginal
                   : kVrInteractionReach)
            : g_pickupReachOriginal;
    __try {
        if (g_client->SetConsoleVariableFloat(
                "ActivationDistance", activationValue) == LT_OK &&
            g_client->SetConsoleVariableFloat(
                "PickupDistance", pickupValue) == LT_OK) {
            g_interactionReachApplied = stereoEnabled;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void UpdateCrosshairOverride() noexcept {
    if (g_client == nullptr) {
        return;
    }

    bool stereoAimGuideEnabled = false;
    __try {
        stereoAimGuideEnabled =
            g_isStereoEnabled != nullptr && g_isStereoEnabled();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        stereoAimGuideEnabled = false;
    }

    if (!g_crosshairOriginalKnown) {
        __try {
            const HCONSOLEVAR crosshair =
                g_client->GetConsoleVariable("DisableCrosshair");
            g_crosshairOriginalValue =
                crosshair == nullptr
                    ? 0.0F
                    : g_client->GetConsoleVariableFloat(crosshair);
            g_crosshairOriginalKnown = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
    }
    if (!g_recoilOriginalKnown) {
        __try {
            const HCONSOLEVAR recoil =
                g_client->GetConsoleVariable("CamRecoilKick");
            g_recoilOriginalValue =
                recoil == nullptr
                    ? -1.0F
                    : g_client->GetConsoleVariableFloat(recoil);
            g_recoilOriginalKnown = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
    }

    __try {
        if (stereoAimGuideEnabled != g_crosshairOverrideApplied &&
            g_client->SetConsoleVariableFloat(
                "DisableCrosshair",
                stereoAimGuideEnabled
                    ? 1.0F
                    : g_crosshairOriginalValue) == LT_OK) {
            g_crosshairOverrideApplied = stereoAimGuideEnabled;
        }
        if (stereoAimGuideEnabled != g_recoilOverrideApplied &&
            g_client->SetConsoleVariableFloat(
                "CamRecoilKick",
                stereoAimGuideEnabled
                    ? 0.0F
                    : g_recoilOriginalValue) == LT_OK) {
            g_recoilOverrideApplied = stereoAimGuideEnabled;
            Report(
                "INFO",
                stereoAimGuideEnabled
                    ? "vr_camera_recoil_disabled"
                    : "vr_camera_recoil_restored",
                stereoAimGuideEnabled
                    ? "Persistent Retail camera pitch/yaw recoil is "
                      "disabled while stereo VR is active."
                    : "Retail camera recoil setting was restored.");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    UpdateInteractionReachOverride(stereoAimGuideEnabled);
}

void RestoreRetailCameraCollisionPath() noexcept {
    if (!g_cameraCollisionRaycastApplied ||
        !g_cameraCollisionOriginalKnown ||
        g_client == nullptr) {
        return;
    }
    __try {
        if (g_client->SetConsoleVariableFloat(
                "CameraCollisionUseObject",
                g_cameraCollisionOriginalUseObject) == LT_OK) {
            g_cameraCollisionRaycastApplied = false;
            Report(
                "INFO", "vr_camera_collision_retail_restored",
                "Retail's original camera-collision path was restored.");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Retail normally moves an invisible collision model from the previous camera
// position to the desired one. Contact resolution can alternate between
// nearby valid positions and is visible as first-person camera wobble in VR.
// Its built-in raycast fallback keeps anti-clipping without that moving body.
void UpdateVrCameraCollisionPath() noexcept {
    if (g_client == nullptr ||
        g_isStereoEnabled == nullptr ||
        g_isFlatPanelActive == nullptr) {
        return;
    }

    bool nativeVrWorld = false;
    __try {
        nativeVrWorld =
            g_isStereoEnabled() != FALSE &&
            g_isFlatPanelActive() == FALSE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    if (!g_cameraCollisionOriginalKnown) {
        __try {
            const HCONSOLEVAR variable =
                g_client->GetConsoleVariable(
                    "CameraCollisionUseObject");
            if (variable == nullptr) {
                return;
            }
            g_cameraCollisionOriginalUseObject =
                g_client->GetConsoleVariableFloat(variable);
            g_cameraCollisionOriginalKnown = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
    }

    if (!nativeVrWorld) {
        RestoreRetailCameraCollisionPath();
        return;
    }
    if (g_cameraCollisionRaycastApplied) {
        return;
    }
    __try {
        if (g_client->SetConsoleVariableFloat(
                "CameraCollisionUseObject", 0.0F) == LT_OK) {
            g_cameraCollisionRaycastApplied = true;
            Report(
                "INFO", "vr_camera_collision_raycast_enabled",
                "Native VR uses Retail's raycast camera anti-clipping "
                "instead of its moving collision model.");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool VrCameraCollisionObjectFilter(
    HOBJECT object, void* ignoredUserData) {
    (void)ignoredUserData;
    if (object == nullptr || g_client == nullptr) {
        return true;
    }

    __try {
        ILTCommon* const common = g_client->Common();
        std::uint32_t userFlags = 0;
        if (common != nullptr &&
            common->GetObjectFlags(
                object, OFT_User, userFlags) == LT_OK &&
            IsCharacterCollisionObject(userFlags)) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Unknown objects retain Retail's conservative collision behavior.
    }
    return true;
}

bool TraceVrCameraDirection(
    LTVector& position, const LTVector& direction,
    float clipDistance, bool positive) noexcept {
    IntersectQuery query;
    query.m_From = position;
    query.m_To =
        position +
        direction * (positive ? clipDistance : -clipDistance);
    query.m_Flags =
        INTERSECT_OBJECTS | IGNORE_NONSOLID | INTERSECT_HPOLY;
    query.m_FilterFn = &VrCameraCollisionObjectFilter;

    IntersectInfo hit;
    __try {
        if (!g_client->IntersectSegment(query, &hit)) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    const float hitDistance = (hit.m_Point - position).Mag();
    if (!std::isfinite(hitDistance) ||
        hitDistance >= clipDistance) {
        return true;
    }
    const float correction = clipDistance - hitDistance;
    position +=
        direction * (positive ? -correction : correction);
    return true;
}

// Retails ray-only camera path has no object filter. Consequently the six
// short anti-clipping rays can hit a character's large invisible box while
// the headset is close to a corpse and alternately push the camera to either
// side. Keep the right/left, ceiling/floor and forward/backward layout, but
// let only world and prop geometry influence the VR camera.
void __fastcall HookRetailCalcNoClip(
    void* playerCamera, void* ignoredEdx,
    LTVector& position, LTRotation& rotation) {
    (void)ignoredEdx;
    bool nativeVrWorld = false;
    __try {
        nativeVrWorld =
            g_isStereoEnabled != nullptr &&
            g_isFlatPanelActive != nullptr &&
            g_isStereoEnabled() != FALSE &&
            g_isFlatPanelActive() == FALSE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        nativeVrWorld = false;
    }
    if (!nativeVrWorld || g_client == nullptr) {
        g_retailCalcNoClip(
            playerCamera, position, rotation);
        return;
    }

    float clipDistance = 30.0F;
    __try {
        const HCONSOLEVAR variable =
            g_client->GetConsoleVariable("CameraClipDist");
        if (variable != nullptr) {
            const float configured =
                g_client->GetConsoleVariableFloat(variable);
            if (std::isfinite(configured) && configured > 0.0F) {
                clipDistance = configured;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    const LTVector right = rotation.Right();
    const LTVector up = rotation.Up();
    const LTVector forward = rotation.Forward();
    if (!TraceVrCameraDirection(
            position, right, clipDistance, true)) {
        TraceVrCameraDirection(
            position, right, clipDistance, false);
    }
    if (!TraceVrCameraDirection(
            position, up, clipDistance, true)) {
        TraceVrCameraDirection(
            position, up, clipDistance, false);
    }
    if (!TraceVrCameraDirection(
            position, forward, clipDistance, true)) {
        TraceVrCameraDirection(
            position, forward, clipDistance, false);
    }

    if (InterlockedCompareExchange(
            &g_cameraCharacterFilterActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "vr_camera_character_collision_filtered",
            "VR camera anti-clipping ignores character and corpse hitboxes "
            "while retaining world and prop collision.");
    }
}

// Liefert die Mündung in Weltkoordinaten.
//
// Bevorzugt wird die Rekonstruktion aus derselben Transformation, mit der auch
// die sichtbare Waffe gesetzt wird: {gripTransform.m_vPos,
// fireTransform.m_rRot} plus dem starren Sockelversatz im Waffenraum. Die
// Welt-Sockettransformation der Engine trägt dagegen noch den animierten
// Retail-Waffen-Sway und weicht beim schnellen Strafen seitlich ab.
//
// Zielstrahl und Projektil müssen denselben Ursprung verwenden, sonst driften
// sie genau in dieser Situation auseinander.
bool ResolveMuzzleWorldTransform(LTRigidTransform& muzzle) noexcept {
    if (g_weaponAim.muzzleLocalValid && g_weaponAim.gripValid) {
        muzzle.m_rRot =
            g_weaponAim.fireTransform.m_rRot *
            g_weaponAim.muzzleRotationInWeapon;
        muzzle.m_vPos =
            g_weaponAim.gripTransform.m_vPos +
            g_weaponAim.fireTransform.m_rRot.RotateVector(
                g_weaponAim.muzzleOffsetInWeapon);
        return true;
    }
    if (g_weaponAim.muzzleValid) {
        muzzle = g_weaponAim.muzzleTransform;
        return true;
    }
    muzzle = g_weaponAim.fireTransform;
    return false;
}

bool ResolveLeftMuzzleWorldTransform(
    LTRigidTransform& muzzle) noexcept {
    if (g_weaponAim.leftMuzzleLocalValid &&
        g_weaponAim.leftWeaponTransformValid) {
        muzzle.m_rRot =
            g_weaponAim.leftWeaponTransform.m_rRot *
            g_weaponAim.leftMuzzleRotationInWeapon;
        muzzle.m_vPos =
            g_weaponAim.leftWeaponTransform.m_vPos +
            g_weaponAim.leftWeaponTransform.m_rRot.RotateVector(
                g_weaponAim.leftMuzzleOffsetInWeapon);
        return true;
    }
    if (g_weaponAim.leftMuzzleValid) {
        muzzle = g_weaponAim.leftMuzzleTransform;
        return true;
    }
    if (g_weaponAim.leftAimValid) {
        muzzle = g_weaponAim.leftAimTransform;
        return false;
    }
    return false;
}

struct RetailVectorFilterParams {
    void* weapon;
    LTVector firePosition;
    LTVector endPosition;
};
static_assert(
    sizeof(RetailVectorFilterParams) == 28,
    "Retail CW_VOFF_Params layout changed.");

bool TraceAimPose(
    const LTRigidTransform& muzzle,
    LTVector& rayStart, LTVector& rayEnd) noexcept {
    LTVector right;
    LTVector up;
    LTVector forward;
    muzzle.m_rRot.GetVectors(right, up, forward);
    rayStart = muzzle.m_vPos;
    rayEnd = rayStart + forward * 10000.0F;

    IntersectQuery query;
    // The small forward offset avoids selecting the local weapon/body when
    // the controller aim origin happens to be inside its collision volume.
    query.m_From = rayStart + forward * 8.0F;
    query.m_To = rayEnd;
    query.m_Flags =
        INTERSECT_OBJECTS | IGNORE_NONSOLID | INTERSECT_HPOLY;
    RetailVectorFilterParams filterParams{
        nullptr, rayStart, rayEnd};
    if (g_retailVectorObjectFilter != nullptr) {
        // This is the same callback Retail uses for hitscan weapons. It
        // rejects the broad CharacterHitBox unless the ray also crosses one
        // of the animated/ragdoll skeleton-node spheres.
        query.m_FilterFn = g_retailVectorObjectFilter;
        query.m_pUserData = &filterParams;
    }
    IntersectInfo hit;
    __try {
        if (g_client->IntersectSegment(query, &hit)) {
            rayEnd = hit.m_Point;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

bool TraceWeaponAim(
    LTVector& rayStart, LTVector& rayEnd) noexcept {
    if (g_client == nullptr || !g_weaponAim.valid) {
        return false;
    }

    LTRigidTransform muzzle;
    ResolveMuzzleWorldTransform(muzzle);
    return TraceAimPose(muzzle, rayStart, rayEnd);
}

bool TraceLeftWeaponAim(
    LTVector& rayStart, LTVector& rayEnd) noexcept {
    if (g_client == nullptr || g_weaponAim.leftWeaponModel == nullptr ||
        !g_weaponAim.leftWeaponTransformValid) {
        return false;
    }
    LTRigidTransform muzzle;
    ResolveLeftMuzzleWorldTransform(muzzle);
    return TraceAimPose(muzzle, rayStart, rayEnd);
}

// Ursprung und Richtung, mit denen Retail nach Schaltern und Items suchen
// soll. Es ist bewusst dieselbe Muendungstransformation, aus der auch der
// sichtbare rote Zielstrahl und die Fire-Vectors entstehen: Was der Spieler
// anvisiert, wird damit auch aktiviert.
bool ResolveInteractionRayPose(LTRigidTransform& pose) noexcept {
    if (!g_weaponAim.valid) {
        return false;
    }
    ResolveMuzzleWorldTransform(pose);
    return true;
}

unsigned char* ResolveRetailPlayerCamera() noexcept {
    if (g_retailPlayerMgrPointer == nullptr) {
        return nullptr;
    }
    __try {
        auto* const playerMgr =
            static_cast<unsigned char*>(*g_retailPlayerMgrPointer);
        if (playerMgr == nullptr) {
            return nullptr;
        }
        return *reinterpret_cast<unsigned char**>(
            playerMgr + kRetailPlayerMgrCameraOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool RotationIsFiniteAndUsable(const LTRotation& rotation) noexcept {
    const float magnitude = rotation.GetComponentMagSqr();
    return std::isfinite(rotation.m_Quat[LTRotation::QX]) &&
           std::isfinite(rotation.m_Quat[LTRotation::QY]) &&
           std::isfinite(rotation.m_Quat[LTRotation::QZ]) &&
           std::isfinite(rotation.m_Quat[LTRotation::QW]) &&
           std::isfinite(magnitude) && magnitude > 0.25F &&
           magnitude < 4.0F;
}

bool RotationToPitchYawRoll(
    const LTRotation& rotation, float& pitch,
    float& yaw, float& roll) noexcept {
    LTVector right;
    LTVector up;
    LTVector forward;
    rotation.GetVectors(right, up, forward);
    pitch = std::asin(std::clamp(-forward.y, -1.0F, 1.0F));
    yaw = std::atan2(forward.x, forward.z);
    roll = std::atan2(right.y, up.y);
    return std::isfinite(pitch) &&
           std::isfinite(yaw) &&
           std::isfinite(roll);
}

bool ResolveYawOnlyRotation(
    const LTRotation& rotation,
    LTRotation& yawOnlyRotation) noexcept {
    float ignoredPitch = 0.0F;
    float yaw = 0.0F;
    float ignoredRoll = 0.0F;
    if (!RotationToPitchYawRoll(
            rotation, ignoredPitch, yaw, ignoredRoll)) {
        return false;
    }
    yawOnlyRotation = LTRotation(0.0F, yaw, 0.0F);
    yawOnlyRotation.Normalize();
    return RotationIsFiniteAndUsable(yawOnlyRotation);
}

bool IsRetailCinematicCamera() noexcept {
    unsigned char* const camera = ResolveRetailPlayerCamera();
    if (camera == nullptr) {
        return false;
    }
    __try {
        std::int32_t mode = -1;
        std::memcpy(
            &mode, camera + kRetailCameraModeOffset, sizeof(mode));
        return mode == kRetailCameraModeCinematic;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadRetailAnimatedCamera(bool& animated, std::int32_t& descriptor) noexcept {
    unsigned char* const camera = ResolveRetailPlayerCamera();
    if (camera == nullptr) {
        return false;
    }
    __try {
        std::memcpy(
            &descriptor,
            camera + kRetailCameraLastDescriptorOffset,
            sizeof(descriptor));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (descriptor < -1 || descriptor > 0xFF) {
        return false;
    }
    animated =
        descriptor == kRetailCameraDescriptorRotation ||
        descriptor == kRetailCameraDescriptorRotationAim;
    return true;
}

bool ReadRetailDesiredCameraPosition(LTVector& position) noexcept {
    unsigned char* const camera = ResolveRetailPlayerCamera();
    if (camera == nullptr) {
        return false;
    }
    __try {
        std::memcpy(
            &position, camera + kRetailCameraPositionOffset,
            sizeof(position));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return std::isfinite(position.x) &&
           std::isfinite(position.y) &&
           std::isfinite(position.z);
}

bool ReadRetailPlayerMovementAllowed(bool& allowed) noexcept {
    if (g_retailPlayerMgrPointer == nullptr) {
        return false;
    }
    __try {
        const auto* const playerMgr =
            static_cast<const unsigned char*>(
                *g_retailPlayerMgrPointer);
        if (playerMgr == nullptr) {
            return false;
        }
        const unsigned char value =
            playerMgr[kRetailPlayerMgrAllowMovementOffset];
        if (value > 1) {
            return false;
        }
        allowed = value != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

const void* const* ResolveRetailMoveManagerSlot() noexcept;

bool ReadRetailMovingLure(bool& movingLure, std::int32_t& physicsModel) noexcept {
    const void* const* const slot = ResolveRetailMoveManagerSlot();
    if (slot == nullptr) {
        return false;
    }
    __try {
        const auto* const move =
            static_cast<const unsigned char*>(*slot);
        if (move == nullptr) {
            return false;
        }
        const auto* const vehicle =
            *reinterpret_cast<const unsigned char* const*>(
                move + kRetailMoveVehicleManagerOffset);
        if (vehicle == nullptr) {
            return false;
        }
        std::memcpy(
            &physicsModel,
            vehicle + kRetailVehiclePhysicsModelOffset,
            sizeof(physicsModel));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (physicsModel < 0 || physicsModel > 1) {
        return false;
    }
    movingLure = physicsModel == kRetailPlayerPhysicsModelLure;
    return true;
}

void BeginSlideKickViewStabilization() noexcept {
    const ULONGLONG now = GetTickCount64();
    if (g_slideKickView.active) {
        // A second kick may start while Retail is still finishing the first
        // camera animation. Keep the original pre-kick basis; recapturing here
        // could turn an intermediate animated pitch into the new neutral view.
        g_slideKickView.startTick = now;
        Report(
            "INFO", "slide_kick_view_stabilization_extended",
            "Another slide kick extended the existing stable camera basis.");
        return;
    }

    unsigned char* const camera = ResolveRetailPlayerCamera();
    if (camera == nullptr) {
        return;
    }

    LTRotation localRotation;
    __try {
        std::memcpy(
            localRotation.m_Quat,
            camera + kRetailCameraRotationSecondOffset,
            sizeof(localRotation.m_Quat));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (!RotationIsFiniteAndUsable(localRotation)) {
        return;
    }
    localRotation.Normalize();
    float pitch = 0.0F;
    float yaw = 0.0F;
    float roll = 0.0F;
    if (!RotationToPitchYawRoll(
            localRotation, pitch, yaw, roll)) {
        return;
    }

    g_slideKickView.localRotation = localRotation;
    g_slideKickView.pitch = pitch;
    g_slideKickView.roll = roll;
    g_slideKickView.startTick = now;
    g_slideKickView.active = true;
    Report(
        "INFO", "slide_kick_view_stabilization_started",
        "Captured the pre-kick local camera rotation.");
}

// Retails CPlayerCamera speichert beim Verlassen einer CAM_Rotation-Animation
// deren letzte Socket-Drehung als neue lokale Blickrichtung. In VR gehoert die
// Blickneigung jedoch ausschliesslich dem HMD. Solange der von uns ausgelöste
// Slide Kick läuft, halten wir deshalb Pitch und Roll von m_rLocalRotation
// sowie m_rTargetAttachRot auf dem Wert vor dem Tritt. Retails aktueller Yaw
// wird dagegen in jedem Frame übernommen, damit Snap-Turn weiterhin sofort
// funktioniert. Position, Körperdrehung und die gesamte Körperanimation
// bleiben unverändert.
void MaintainSlideKickViewBase() noexcept {
    if (!g_slideKickView.active) {
        return;
    }
    unsigned char* const camera = ResolveRetailPlayerCamera();
    if (camera == nullptr) {
        g_slideKickView = {};
        return;
    }
    LTRotation worldRotation;
    LTRotation currentLocalRotation;
    __try {
        std::memcpy(
            worldRotation.m_Quat,
            camera + kRetailCameraRotationFirstOffset,
            sizeof(worldRotation.m_Quat));
        std::memcpy(
            currentLocalRotation.m_Quat,
            camera + kRetailCameraRotationSecondOffset,
            sizeof(currentLocalRotation.m_Quat));
        if (!RotationIsFiniteAndUsable(worldRotation) ||
            !RotationIsFiniteAndUsable(currentLocalRotation)) {
            g_slideKickView = {};
            return;
        }
        worldRotation.Normalize();
        currentLocalRotation.Normalize();
        float ignoredPitch = 0.0F;
        float currentYaw = 0.0F;
        float ignoredRoll = 0.0F;
        if (!RotationToPitchYawRoll(
                currentLocalRotation, ignoredPitch,
                currentYaw, ignoredRoll)) {
            g_slideKickView = {};
            return;
        }
        g_slideKickView.localRotation = LTRotation(
            g_slideKickView.pitch, currentYaw,
            g_slideKickView.roll);
        g_slideKickView.localRotation.Normalize();
        LTRotation targetAttachRotation =
            worldRotation * g_slideKickView.localRotation;
        targetAttachRotation.Normalize();

        std::memcpy(
            camera + kRetailCameraRotationSecondOffset,
            g_slideKickView.localRotation.m_Quat,
            sizeof(g_slideKickView.localRotation.m_Quat));
        std::memcpy(
            camera + kRetailCameraTargetAttachRotationOffset,
            targetAttachRotation.m_Quat,
            sizeof(targetAttachRotation.m_Quat));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_slideKickView = {};
    }
}

bool ResolveSlideKickViewBase(
    const LTRotation& retailRotation,
    LTRotation& stableRotation) noexcept {
    if (!g_slideKickView.active) {
        return false;
    }
    unsigned char* const camera = ResolveRetailPlayerCamera();
    if (camera == nullptr) {
        g_slideKickView = {};
        return false;
    }

    LTRotation worldRotation;
    __try {
        std::memcpy(
            worldRotation.m_Quat,
            camera + kRetailCameraRotationFirstOffset,
            sizeof(worldRotation.m_Quat));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_slideKickView = {};
        return false;
    }
    if (!RotationIsFiniteAndUsable(worldRotation)) {
        g_slideKickView = {};
        return false;
    }
    worldRotation.Normalize();
    stableRotation =
        worldRotation * g_slideKickView.localRotation;
    stableRotation.Normalize();

    const ULONGLONG elapsed =
        GetTickCount64() - g_slideKickView.startTick;
    constexpr ULONGLONG kStableHoldMs = 5000;
    if (elapsed >= kStableHoldMs) {
        g_slideKickView = {};
        Report(
            "INFO", "slide_kick_view_stabilized",
            "The pre-kick camera basis was held for five seconds after the "
            "last slide kick.");
    }
    (void)retailRotation;
    return true;
}

void* ResolveRetailPickupDetector() noexcept {
    if (g_retailPlayerMgrPointer == nullptr) {
        return nullptr;
    }
    __try {
        auto* const playerMgr =
            static_cast<unsigned char*>(*g_retailPlayerMgrPointer);
        if (playerMgr == nullptr) {
            return nullptr;
        }
        return playerMgr + kRetailPlayerMgrPickupDetectorOffset;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// Ob die Kamera fuer einen Retail-Aufruf ueberhaupt umgesetzt werden darf.
// Waehrend Zwischensequenzen und im Komfortpanel gehoert sie der Engine —
// dieselbe Grenze, an der auch der Taschenlampenpfad zurueckweicht.
bool IsNativeVrWorldActive() noexcept {
    if (!g_hookInstalled || g_disableStereoRender ||
        g_isStereoEnabled == nullptr ||
        g_isHostConnected == nullptr) {
        return false;
    }
    __try {
        return g_isStereoEnabled() != FALSE &&
               g_isHostConnected() != FALSE &&
               (g_isFlatPanelActive == nullptr ||
                g_isFlatPanelActive() == FALSE);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsInteractionOverrideAllowed() noexcept {
    return !g_disableInteractionHooks && IsNativeVrWorldActive();
}

// CheckForIntersect liest Position und Drehung der Kamera ausschliesslich in
// seinen ersten Instruktionen und rechnet danach nur noch mit Kopien. Die drei
// Member duerfen deshalb fuer die Dauer des Originalaufrufs auf die
// Waffenpose zeigen, solange sie danach unveraendert zurueckkehren.
void __fastcall HookRetailCheckForIntersect(
    void* targetMgr, void* ignoredEdx, float* distance) {
    unsigned char* const camera = ResolveRetailPlayerCamera();
    LTRigidTransform ray;
    if (camera == nullptr || !IsInteractionOverrideAllowed() ||
        !ResolveInteractionRayPose(ray)) {
        g_retailCheckForIntersect(targetMgr, ignoredEdx, distance);
        return;
    }

    float savedPosition[3];
    float savedFirst[4];
    float savedSecond[4];
    float* position = nullptr;
    float* first = nullptr;
    float* second = nullptr;
    __try {
        position = reinterpret_cast<float*>(
            camera + kRetailCameraPositionOffset);
        first = reinterpret_cast<float*>(
            camera + kRetailCameraRotationFirstOffset);
        second = reinterpret_cast<float*>(
            camera + kRetailCameraRotationSecondOffset);
        std::memcpy(savedPosition, position, sizeof(savedPosition));
        std::memcpy(savedFirst, first, sizeof(savedFirst));
        std::memcpy(savedSecond, second, sizeof(savedSecond));

        position[0] = ray.m_vPos.x;
        position[1] = ray.m_vPos.y;
        position[2] = ray.m_vPos.z;
        first[0] = ray.m_rRot.m_Quat[0];
        first[1] = ray.m_rRot.m_Quat[1];
        first[2] = ray.m_rRot.m_Quat[2];
        first[3] = ray.m_rRot.m_Quat[3];
        // Der zweite Faktor wird zur Identitaet, damit das Produkt genau die
        // Waffendrehung ergibt.
        second[0] = 0.0F;
        second[1] = 0.0F;
        second[2] = 0.0F;
        second[3] = 1.0F;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_retailCheckForIntersect(targetMgr, ignoredEdx, distance);
        return;
    }

    g_retailCheckForIntersect(targetMgr, ignoredEdx, distance);

    __try {
        std::memcpy(position, savedPosition, sizeof(savedPosition));
        std::memcpy(first, savedFirst, sizeof(savedFirst));
        std::memcpy(second, savedSecond, sizeof(savedSecond));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    if (InterlockedCompareExchange(
            &g_interactionRayActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "interaction_ray_active",
            "Switches and consoles are selected along the VR weapon ray "
            "instead of the head-mounted view direction.");
    }
}

// Der Aufsammelkegel haengt am Kameraobjekt. Ihm fuer genau diesen Aufruf die
// Waffenpose zu geben ist dasselbe Muster wie beim Taschenlampenpfad; nur der
// Pickup-Detektor des Spielers ist betroffen, andere Detektoren laufen
// unveraendert weiter.
void __fastcall HookRetailObjectDetectorUpdate(
    void* detector, void* ignoredEdx, float elapsedSeconds) {
    unsigned char* const camera = ResolveRetailPlayerCamera();
    LTRigidTransform ray;
    if (g_client == nullptr || camera == nullptr ||
        detector != ResolveRetailPickupDetector() ||
        !IsInteractionOverrideAllowed() ||
        !ResolveInteractionRayPose(ray)) {
        g_retailObjectDetectorUpdate(
            detector, ignoredEdx, elapsedSeconds);
        return;
    }

    HLOCALOBJ cameraObject = nullptr;
    LTRigidTransform saved;
    __try {
        cameraObject = *reinterpret_cast<HLOCALOBJ*>(
            camera + kRetailCameraObjectOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        cameraObject = nullptr;
    }
    if (cameraObject == nullptr ||
        g_client->GetObjectTransform(cameraObject, &saved) != LT_OK ||
        g_client->SetObjectTransform(cameraObject, ray) != LT_OK) {
        g_retailObjectDetectorUpdate(
            detector, ignoredEdx, elapsedSeconds);
        return;
    }

    g_retailObjectDetectorUpdate(detector, ignoredEdx, elapsedSeconds);
    g_client->SetObjectTransform(cameraObject, saved);

    if (InterlockedCompareExchange(
            &g_pickupRayActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "pickup_ray_active",
            "The Retail pickup cone follows the VR weapon ray, so items are "
            "taken by pointing and using instead of by standing on them.");
    }
}

int ReadRetailGameState() noexcept;
bool ResolveHeadFlashlightPose(LTRigidTransform& pose) noexcept;

struct WristHudData {
    std::uint32_t health{0};
    std::uint32_t armor{0};
    std::uint32_t maxHealth{0};
    std::uint32_t maxArmor{0};
    std::uint32_t ammo{0};
    std::uint32_t throwables[3]{};
    std::uint32_t medkits{0};
    float airPercent{1.0F};
};

bool ResolveRetailWristHudTargets() noexcept {
    if (g_retailWristHudResolveAttempted) {
        return g_retailPlayerStatsPointer != nullptr &&
               g_retailWeaponDatabasePointer != nullptr &&
               g_retailDatabasePointer != nullptr &&
               g_retailGetAmmoCount != nullptr &&
               g_retailGetWeaponData != nullptr &&
               g_retailGetPlayerGrenade != nullptr &&
               g_retailGetGearRecord != nullptr &&
               g_retailGetGearCount != nullptr;
    }
    g_retailWristHudResolveAttempted = true;

    HMODULE const module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return false;
    }
    auto* const base = reinterpret_cast<unsigned char*>(module);
    __try {
        const auto* const dos =
            reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
            dos->e_lfanew <= 0) {
            return false;
        }
        const auto* const nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(
                base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->FileHeader.TimeDateStamp !=
                kRetailGameClientTimeDateStamp ||
            nt->OptionalHeader.SizeOfImage !=
                kRetailGameClientSizeOfImage) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    auto* const ammoCount = base + kRetailGetAmmoCountRva;
    auto* const weaponData = base + kRetailGetWeaponDataRva;
    auto* const playerGrenade = base + kRetailGetPlayerGrenadeRva;
    auto* const gearRecord = base + kRetailGetGearRecordRva;
    auto* const gearCount = base + kRetailGetGearCountRva;
    constexpr unsigned char kAmmoCountThunk[] = {
        0xE9};
    constexpr unsigned char kWeaponDataPrefix[] = {
        0x56, 0x8B, 0x74, 0x24, 0x08, 0x85, 0xF6, 0x75,
        0x06, 0x33, 0xC0, 0x5E, 0xC2, 0x08, 0x00};
    constexpr unsigned char kPlayerGrenadePrefix[] = {
        0x8B, 0xC1, 0x8B, 0x0D};
    constexpr unsigned char kPlayerGrenadeBody[] = {
        0x8B, 0x40, 0x48, 0x8B, 0x11};
    constexpr unsigned char kGearRecordPrefix[] = {
        0x53, 0x8B, 0x59, 0x34, 0x57, 0x8B, 0x7C, 0x24,
        0x0C};
    constexpr unsigned char kGearCountThunk[] = {
        0xE9};
    if (!MatchesCode(
            ammoCount, kAmmoCountThunk, sizeof(kAmmoCountThunk)) ||
        !MatchesCode(
            weaponData, kWeaponDataPrefix,
            sizeof(kWeaponDataPrefix)) ||
        !MatchesCode(
            playerGrenade, kPlayerGrenadePrefix,
            sizeof(kPlayerGrenadePrefix)) ||
        !MatchesCode(
            playerGrenade + 8, kPlayerGrenadeBody,
            sizeof(kPlayerGrenadeBody)) ||
        !MatchesCode(
            gearRecord, kGearRecordPrefix,
            sizeof(kGearRecordPrefix)) ||
        !MatchesCode(
            gearCount, kGearCountThunk,
            sizeof(kGearCountThunk))) {
        Report(
            "ERROR", "wrist_hud_layout_mismatch",
            "Retail status or throwable accessors did not match; the "
            "wrist HUD stays disabled.");
        return false;
    }

    g_retailPlayerStatsPointer =
        reinterpret_cast<const void* const*>(
            base + kRetailPlayerStatsPointerRva);
    g_retailWeaponDatabasePointer =
        reinterpret_cast<const void* const*>(
            base + kRetailWeaponDatabasePointerRva);
    g_retailDatabasePointer =
        reinterpret_cast<IDatabaseMgr* const*>(
            base + kRetailDatabasePointerRva);
    g_retailGetAmmoCount =
        reinterpret_cast<RetailGetAmmoCountFunction>(ammoCount);
    g_retailGetWeaponData =
        reinterpret_cast<RetailGetWeaponDataFunction>(weaponData);
    g_retailGetPlayerGrenade =
        reinterpret_cast<RetailGetPlayerGrenadeFunction>(playerGrenade);
    g_retailGetGearRecord =
        reinterpret_cast<RetailGetGearRecordFunction>(gearRecord);
    g_retailGetGearCount =
        reinterpret_cast<RetailGetGearCountFunction>(gearCount);
    Report(
        "INFO", "wrist_hud_retail_data_ready",
        "Verified Retail health, armor, ammo and all three throwable "
        "inventory slots are available to the wrist HUD.");
    return true;
}

bool ReadRetailWristHudData(WristHudData& data) noexcept {
    if (!ResolveRetailWristHudTargets()) {
        return false;
    }

    __try {
        const auto* const stats =
            reinterpret_cast<const unsigned char*>(
                *g_retailPlayerStatsPointer);
        const void* const weaponDatabase =
            *g_retailWeaponDatabasePointer;
        IDatabaseMgr* const database = *g_retailDatabasePointer;
        if (stats == nullptr || weaponDatabase == nullptr ||
            database == nullptr) {
            return false;
        }

        std::memcpy(
            &data.health,
            stats + kRetailPlayerStatsHealthOffset,
            sizeof(data.health));
        std::memcpy(
            &data.armor,
            stats + kRetailPlayerStatsArmorOffset,
            sizeof(data.armor));
        std::memcpy(
            &data.maxHealth,
            stats + kRetailPlayerStatsMaxHealthOffset,
            sizeof(data.maxHealth));
        std::memcpy(
            &data.maxArmor,
            stats + kRetailPlayerStatsMaxArmorOffset,
            sizeof(data.maxArmor));
        std::memcpy(
            &data.airPercent,
            stats + kRetailPlayerStatsAirPercentOffset,
            sizeof(data.airPercent));
        HRECORD currentAmmo = nullptr;
        std::memcpy(
            &currentAmmo,
            stats + kRetailPlayerStatsCurrentAmmoOffset,
            sizeof(currentAmmo));
        data.ammo =
            g_retailGetAmmoCount(stats, currentAmmo);

        for (std::uint8_t slot = 0; slot < 3; ++slot) {
            HRECORD const grenade =
                g_retailGetPlayerGrenade(weaponDatabase, slot);
            HRECORD const weapon =
                g_retailGetWeaponData(
                    weaponDatabase, grenade, false);
            HATTRIBUTE const ammoAttribute =
                database->GetAttribute(weapon, "AmmoName");
            HRECORD const ammo = database->GetRecordLink(
                ammoAttribute, 0, nullptr);
            data.throwables[slot] =
                g_retailGetAmmoCount(stats, ammo);
        }
        HRECORD const medkit =
            g_retailGetGearRecord(weaponDatabase, "MedKit");
        data.medkits =
            g_retailGetGearCount(stats, medkit);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    if (data.maxHealth == 0 || data.maxHealth > 1000000U ||
        data.maxArmor > 1000000U ||
        !std::isfinite(data.airPercent)) {
        return false;
    }
    data.airPercent = std::clamp(data.airPercent, 0.0F, 1.0F);
    return true;
}

LTVector WristHudPoint(
    const LTVector& center, const LTVector& right,
    const LTVector& up, float x, float y) noexcept {
    return center + right * x + up * y;
}

constexpr std::size_t kWristHudQuadBatchSize = 128;
LT_POLYG4 g_wristHudQuadBatch[kWristHudQuadBatchSize];
std::size_t g_wristHudQuadBatchCount = 0;

void FlushWristHudRects(ILTDrawPrim* drawPrim) noexcept {
    if (drawPrim == nullptr || g_wristHudQuadBatchCount == 0) {
        return;
    }
    drawPrim->DrawPrim(
        g_wristHudQuadBatch,
        static_cast<std::uint32_t>(g_wristHudQuadBatchCount));
    g_wristHudQuadBatchCount = 0;
}

void DrawWristHudRect(
    ILTDrawPrim* drawPrim, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float left, float bottom, float rightEdge, float top,
    std::uint8_t red, std::uint8_t green, std::uint8_t blue,
    std::uint8_t alpha) noexcept {
    if (g_wristHudQuadBatchCount == kWristHudQuadBatchSize) {
        FlushWristHudRects(drawPrim);
    }
    LT_POLYG4& quad =
        g_wristHudQuadBatch[g_wristHudQuadBatchCount++];
    quad.verts[0].pos =
        WristHudPoint(center, right, up, left, top);
    quad.verts[1].pos =
        WristHudPoint(center, right, up, rightEdge, top);
    quad.verts[2].pos =
        WristHudPoint(center, right, up, rightEdge, bottom);
    quad.verts[3].pos =
        WristHudPoint(center, right, up, left, bottom);
    for (LT_VERTG& vertex : quad.verts) {
        vertex.rgba.Init(red, green, blue, alpha);
    }
}

void DrawWristHudLine(
    ILTDrawPrim* drawPrim, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float x0, float y0, float x1, float y1, float thickness,
    std::uint8_t red, std::uint8_t green, std::uint8_t blue,
    std::uint8_t alpha = 255) noexcept {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001F || thickness <= 0.0F) {
        return;
    }
    if (g_wristHudQuadBatchCount == kWristHudQuadBatchSize) {
        FlushWristHudRects(drawPrim);
    }

    const float halfWidth = thickness * 0.5F;
    const float perpendicularX = -dy * halfWidth / length;
    const float perpendicularY = dx * halfWidth / length;
    LT_POLYG4& quad =
        g_wristHudQuadBatch[g_wristHudQuadBatchCount++];
    quad.verts[0].pos = WristHudPoint(
        center, right, up,
        x0 + perpendicularX, y0 + perpendicularY);
    quad.verts[1].pos = WristHudPoint(
        center, right, up,
        x1 + perpendicularX, y1 + perpendicularY);
    quad.verts[2].pos = WristHudPoint(
        center, right, up,
        x1 - perpendicularX, y1 - perpendicularY);
    quad.verts[3].pos = WristHudPoint(
        center, right, up,
        x0 - perpendicularX, y0 - perpendicularY);
    for (LT_VERTG& vertex : quad.verts) {
        vertex.rgba.Init(red, green, blue, alpha);
    }
}

void DrawWristHudFrame(
    ILTDrawPrim* drawPrim, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float left, float bottom, float rightEdge, float top,
    float thickness, std::uint8_t red, std::uint8_t green,
    std::uint8_t blue, std::uint8_t alpha = 255) noexcept {
    DrawWristHudRect(
        drawPrim, center, right, up,
        left, top - thickness, rightEdge, top,
        red, green, blue, alpha);
    DrawWristHudRect(
        drawPrim, center, right, up,
        left, bottom, rightEdge, bottom + thickness,
        red, green, blue, alpha);
    DrawWristHudRect(
        drawPrim, center, right, up,
        left, bottom + thickness, left + thickness, top - thickness,
        red, green, blue, alpha);
    DrawWristHudRect(
        drawPrim, center, right, up,
        rightEdge - thickness, bottom + thickness, rightEdge, top - thickness,
        red, green, blue, alpha);
}

enum WristHudStrokeSegment : std::uint16_t {
    kStrokeTop = 1U << 0,
    kStrokeUpperLeft = 1U << 1,
    kStrokeUpperRight = 1U << 2,
    kStrokeMiddleLeft = 1U << 3,
    kStrokeMiddleRight = 1U << 4,
    kStrokeLowerLeft = 1U << 5,
    kStrokeLowerRight = 1U << 6,
    kStrokeBottom = 1U << 7,
    kStrokeUpperLeftDiagonal = 1U << 8,
    kStrokeUpperRightDiagonal = 1U << 9,
    kStrokeLowerLeftDiagonal = 1U << 10,
    kStrokeLowerRightDiagonal = 1U << 11,
    kStrokeUpperCenter = 1U << 12,
    kStrokeLowerCenter = 1U << 13,
    kStrokeUpperDot = 1U << 14,
    kStrokeLowerDot = 1U << 15,
};

std::uint16_t WristHudStrokeGlyph(char character) noexcept {
    switch (character) {
    case '0':
        return kStrokeTop | kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeLowerLeft | kStrokeLowerRight | kStrokeBottom;
    case '1':
        return kStrokeUpperRight | kStrokeLowerRight;
    case '2':
        return kStrokeTop | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerLeft | kStrokeBottom;
    case '3':
        return kStrokeTop | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerRight | kStrokeBottom;
    case '4':
        return kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerRight;
    case '5':
        return kStrokeTop | kStrokeUpperLeft |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerRight | kStrokeBottom;
    case '6':
        return kStrokeTop | kStrokeUpperLeft |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerLeft | kStrokeLowerRight | kStrokeBottom;
    case '7':
        return kStrokeTop | kStrokeUpperRight | kStrokeLowerRight;
    case '8':
        return kStrokeTop | kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerLeft | kStrokeLowerRight | kStrokeBottom;
    case '9':
        return kStrokeTop | kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerRight | kStrokeBottom;
    case 'A':
        return kStrokeTop | kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerLeft | kStrokeLowerRight;
    case 'C':
        return kStrokeTop | kStrokeUpperLeft |
               kStrokeLowerLeft | kStrokeBottom;
    case 'G':
        return kStrokeTop | kStrokeUpperLeft | kStrokeMiddleRight |
               kStrokeLowerLeft | kStrokeLowerRight | kStrokeBottom;
    case 'H':
        return kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerLeft | kStrokeLowerRight;
    case 'I':
        return kStrokeTop | kStrokeBottom |
               kStrokeUpperCenter | kStrokeLowerCenter;
    case 'K':
        return kStrokeUpperLeft | kStrokeLowerLeft |
               kStrokeUpperRightDiagonal | kStrokeLowerRightDiagonal;
    case 'M':
        return kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeLowerLeft | kStrokeLowerRight |
               kStrokeUpperLeftDiagonal | kStrokeUpperRightDiagonal;
    case 'P':
        return kStrokeTop | kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerLeft;
    case 'R':
        return kStrokeTop | kStrokeUpperLeft | kStrokeUpperRight |
               kStrokeMiddleLeft | kStrokeMiddleRight |
               kStrokeLowerLeft | kStrokeLowerRightDiagonal;
    case ':':
        return kStrokeUpperDot | kStrokeLowerDot;
    default: return 0;
    }
}

void DrawWristHudStrokeGlyph(
    ILTDrawPrim* drawPrim, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float left, float top, float height, char character,
    std::uint8_t red, std::uint8_t green, std::uint8_t blue,
    std::uint8_t alpha) noexcept {
    constexpr float kSegments[][4] = {
        {0.12F, 1.00F, 0.88F, 1.00F},
        {0.00F, 0.90F, 0.00F, 0.56F},
        {1.00F, 0.90F, 1.00F, 0.56F},
        {0.10F, 0.50F, 0.47F, 0.50F},
        {0.53F, 0.50F, 0.90F, 0.50F},
        {0.00F, 0.44F, 0.00F, 0.10F},
        {1.00F, 0.44F, 1.00F, 0.10F},
        {0.12F, 0.00F, 0.88F, 0.00F},
        {0.10F, 0.90F, 0.47F, 0.55F},
        {0.90F, 0.90F, 0.53F, 0.55F},
        {0.47F, 0.45F, 0.10F, 0.10F},
        {0.53F, 0.45F, 0.90F, 0.10F},
        {0.50F, 0.90F, 0.50F, 0.56F},
        {0.50F, 0.44F, 0.50F, 0.10F},
        {0.43F, 0.68F, 0.57F, 0.68F},
        {0.43F, 0.32F, 0.57F, 0.32F},
    };
    const std::uint16_t glyph = WristHudStrokeGlyph(character);
    const float width = height * 0.56F;
    const float bottom = top - height;
    const float thickness = (std::max)(height * 0.065F, 0.045F);
    for (std::size_t index = 0;
         index < _countof(kSegments); ++index) {
        if ((glyph & (1U << index)) == 0) {
            continue;
        }
        const float* const segment = kSegments[index];
        DrawWristHudLine(
            drawPrim, center, right, up,
            left + segment[0] * width,
            bottom + segment[1] * height,
            left + segment[2] * width,
            bottom + segment[3] * height,
            thickness, red, green, blue, alpha);
    }
}

void DrawWristHudText(
    ILTDrawPrim* drawPrim, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float left, float top, float height, const char* text,
    std::uint8_t red, std::uint8_t green, std::uint8_t blue,
    std::uint8_t alpha = 255) noexcept {
    if (text == nullptr) {
        return;
    }
    float cursor = left;
    for (const char* character = text;
         *character != '\0'; ++character) {
        DrawWristHudStrokeGlyph(
            drawPrim, center, right, up,
            cursor, top, height, *character,
            red, green, blue, alpha);
        cursor += height * 0.74F;
    }
}

void DrawWristHudNumber(
    ILTDrawPrim* drawPrim, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float left, float top, float height, std::uint32_t value,
    std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept {
    char number[8]{};
    _snprintf_s(
        number, sizeof(number), _TRUNCATE, "%u",
        std::min(value, 9999U));
    DrawWristHudText(
        drawPrim, center, right, up, left, top, height, number,
        red, green, blue);
}

void DrawWristHudBar(
    ILTDrawPrim* drawPrim, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float left, float bottom, float width, float height,
    float amount, std::uint8_t red, std::uint8_t green,
    std::uint8_t blue) noexcept {
    DrawWristHudRect(
        drawPrim, center, right, up,
        left, bottom, left + width, bottom + height,
        10, 25, 32, 220);
    DrawWristHudFrame(
        drawPrim, center, right, up,
        left, bottom, left + width, bottom + height,
        0.07F, 42, 105, 120, 245);
    const float clamped = std::clamp(amount, 0.0F, 1.0F);
    if (clamped > 0.0F) {
        DrawWristHudRect(
            drawPrim, center, right, up,
            left + 0.12F, bottom + 0.12F,
            left + 0.12F + (width - 0.24F) * clamped,
            bottom + height - 0.12F,
            red, green, blue, 245);
    }
    for (int tick = 1; tick < 4; ++tick) {
        const float x = left + width * static_cast<float>(tick) * 0.25F;
        DrawWristHudRect(
            drawPrim, center, right, up,
            x - 0.025F, bottom + 0.08F,
            x + 0.025F, bottom + height - 0.08F,
            3, 16, 22, 235);
    }
}

bool ResolveWristHudPlacement(
    LTVector& center, LTVector& right, LTVector& up) noexcept {
    if (!g_weaponAim.leftGripValid ||
        !g_weaponAim.leftAimValid ||
        !g_weaponAim.trackingBaseValid ||
        g_twoHandedGripActive ||
        g_cutsceneCameraState ||
        GetTickCount64() - g_lastWeaponManagerUpdateTick >
            kPlayingFrameFreshMilliseconds ||
        ReadRetailGameState() != kRetailGameStatePlaying) {
        g_wristHudVisibleUntil = 0;
        g_dualPistolWristCandidateSince = 0;
        g_dualPistolWristHudVisible = false;
        return false;
    }

    LTRigidTransform head;
    if (!ResolveHeadFlashlightPose(head)) {
        return false;
    }
    // Anatomische Lage und Zielrichtung sind bei Dual Pistols nicht
    // dasselbe: Die sichtbare Waffenmodellrotation enthaelt den
    // modellspezifischen Flash-Socket-Versatz. Fuer den Uhrblick kommt die
    // Handflaeche aus der Grip-Pose, fuer die Ziel-Unterdrueckung dagegen die
    // echte OpenXR-Aim-Pose. Das verhindert, dass eine korrekt ausgerichtete
    // Support-Pistole wegen ihrer Modellachse als abgewinkeltes Handgelenk
    // gilt.
    const LTVector handForward =
        g_weaponAim.leftGripTransform.m_rRot.Forward();
    const LTVector supportAimForward =
        g_weaponAim.leftAimTransform.m_rRot.Forward();
    LTVector wrist =
        EffectiveLeftHandPosition() - handForward * 4.0F;
    LTVector palmNormal =
        g_weaponAim.leftGripTransform.m_rRot.Up();
    LTVector wristToHead = head.m_vPos - wrist;
    const float distance = wristToHead.Mag();
    if (distance < 12.0F || distance > 100.0F) {
        return false;
    }
    const float wristBelowEyes = wristToHead.y;
    wristToHead.Normalize();
    const float wristSurfaceFacing =
        std::fabs(palmNormal.Dot(wristToHead));
    const LTVector headForward = head.m_rRot.Forward();
    const float weaponHeadAlignment =
        headForward.Dot(supportAimForward);
    // Runtime-Gripraeume koennen die Oberseite des Controllers
    // unterschiedlich orientieren. Das Fenster schwebt deshalb immer auf der
    // dem Gesicht zugewandten Handgelenkseite, nie im Controller/Arm.
    if (palmNormal.Dot(wristToHead) < 0.0F) {
        palmNormal = -palmNormal;
    }
    // Das Panel sitzt einige Zentimeter vor der Handflaeche. Zusammen mit
    // dem spaeter deaktivierten Z-Test bleibt die virtuelle Hand unter dem
    // Display und kann Zahlen oder Linien nicht mehr verdecken.
    center = wrist + palmNormal * 5.0F;

    LTVector gazeDirection = center - head.m_vPos;
    gazeDirection.Normalize();
    const float wristGazeAlignment =
        headForward.Dot(gazeDirection);
    const bool dualPistols = DualPistolsVrActive();
    const bool lookingAtWrist =
        fearvr::IsLookingAtWrist(
            wristGazeAlignment, g_dualPistolWristHudVisible);
    const ULONGLONG now = GetTickCount64();
    const float supportTrigger = dualPistols
        ? g_currentInput.trigger[FEARVR_HAND_LEFT]
        : 0.0F;
    // Ohne zweite Pistole ist nur die anatomische Uhrpose relevant. Die
    // linke Aim-Achse existiert auch fuer die freie Hand, hat dort aber keine
    // semantische Bedeutung und hatte echte Uhrblicke zuletzt faelschlich
    // blockiert. Mit Support-Pistole bleibt ihre Schussrichtung dagegen der
    // entscheidende Schutz vor HUD-Einblendungen beim Zielen und Hueftschuss.
    const bool readingPose = dualPistols
        ? fearvr::IsDualPistolWristReadingPose(
              weaponHeadAlignment, wristSurfaceFacing,
              supportTrigger, g_dualPistolWristHudVisible)
        : fearvr::IsWristBackTilted(
              wristSurfaceFacing, g_dualPistolWristHudVisible);
    const bool candidate = lookingAtWrist && readingPose;

    // Diese kompakte Telemetrie macht Controller-/Runtime-Unterschiede nach
    // einem Headset-Test messbar. Sie gilt nun fuer jede Ausruestung, weil
    // auch die freie Support-Hand die bewusste Rueckneigung erfordert.
    if (wristGazeAlignment >= 0.85F &&
        now >= g_dualPistolWristDiagnosticAt) {
        char message[224]{};
        std::snprintf(
            message, sizeof(message),
            "dual=%d distance=%.1f below_eyes=%.1f gaze=%.2f "
            "weapon_gaze=%.2f surface=%.2f trigger=%.2f "
            "reading=%d visible=%d",
            dualPistols ? 1 : 0,
            static_cast<double>(distance),
            static_cast<double>(wristBelowEyes),
            static_cast<double>(wristGazeAlignment),
            static_cast<double>(weaponHeadAlignment),
            static_cast<double>(wristSurfaceFacing),
            static_cast<double>(supportTrigger),
            readingPose ? 1 : 0,
            g_dualPistolWristHudVisible ? 1 : 0);
        Report("INFO", "wrist_reading_pose", message);
        g_dualPistolWristDiagnosticAt = now + 750;
    }

    if (g_dualPistolWristHudVisible) {
        if (!candidate) {
            g_dualPistolWristHudVisible = false;
            g_dualPistolWristCandidateSince = 0;
            g_wristHudVisibleUntil = 0;
            return false;
        }
    } else if (!fearvr::UpdateDualPistolWristDwell(
                   candidate, now,
                   g_dualPistolWristCandidateSince)) {
        g_wristHudVisibleUntil = 0;
        return false;
    } else {
        g_dualPistolWristHudVisible = true;
    }
    // Keine neutrale Nachlaufzeit: Sobald die Hand wieder normal steht, die
    // Pistole in Schussrichtung zeigt oder der Blick weggeht, ist die Sicht
    // unmittelbar frei.
    g_wristHudVisibleUntil = now;
    if (!g_dualPistolWristHudVisible) {
        return false;
    }

    right = head.m_rRot.Right();
    up = head.m_rRot.Up();
    return true;
}

void RenderWristHud(HLOCALOBJ camera) noexcept {
    if (g_client == nullptr || camera == nullptr) {
        return;
    }

    LTVector center;
    LTVector right;
    LTVector up;
    if (!ResolveWristHudPlacement(center, right, up)) {
        return;
    }
    WristHudData data;
    if (!ReadRetailWristHudData(data)) {
        return;
    }

    ILTDrawPrim* const drawPrim = g_client->GetDrawPrim();
    if (drawPrim == nullptr) {
        return;
    }
    const HOBJECT oldCamera = drawPrim->GetCamera();
    const ELTDrawPrimTransformMode oldTransformMode =
        drawPrim->GetTransformMode();
    const ELTDrawPrimZMode oldZMode = drawPrim->GetZMode();
    const ELTDrawPrimRenderMode oldRenderMode =
        drawPrim->GetRenderMode();

    drawPrim->SetCamera(camera);
    drawPrim->SetTransformMode(eLTDrawPrimTransformMode_World);
    // Ein Wrist-Display ist Information, keine reale Flaeche. Ohne Z-Lesen
    // bleibt es vor der Hand lesbar; Weltposition und Stereo-Parallaxe bleiben
    // trotzdem erhalten.
    drawPrim->SetZMode(eLTDrawPrimZMode_None);
    drawPrim->SetRenderMode(
        eLTDrawPrimRenderMode_Modulate_Translucent);
    drawPrim->SetTexture(nullptr);
    g_wristHudQuadBatchCount = 0;

    // 15,2 x 9,2 Game-Units entsprechen ungefaehr 15,2 x 9,2 cm. Der Aufbau
    // folgt einem technischen Instrument: duenne Segmentzeichen, klare
    // Zellgrenzen und gerasterte Balken statt einer Block-/Pixelschrift.
    DrawWristHudRect(
        drawPrim, center, right, up,
        -7.6F, -4.6F, 7.6F, 4.6F,
        2, 10, 16, 232);
    DrawWristHudFrame(
        drawPrim, center, right, up,
        -7.55F, -4.55F, 7.55F, 4.55F,
        0.11F, 46, 218, 238, 250);
    // Separates Uhrmodul: Der kompakte Aufsatz haelt die Statuszellen frei
    // und passt als klar abgegrenztes Instrument zum technischen Layout.
    DrawWristHudRect(
        drawPrim, center, right, up,
        -2.35F, 4.55F, 2.35F, 5.78F,
        2, 10, 16, 232);
    DrawWristHudFrame(
        drawPrim, center, right, up,
        -2.35F, 4.55F, 2.35F, 5.78F,
        0.10F, 46, 218, 238, 250);
    const ULONGLONG clockNow = GetTickCount64();
    if (clockNow >= g_wristHudClockRefreshAt) {
        SYSTEMTIME localTime{};
        GetLocalTime(&localTime);
        _snprintf_s(
            g_wristHudClockText, sizeof(g_wristHudClockText),
            _TRUNCATE, "%02u:%02u",
            static_cast<unsigned>(localTime.wHour),
            static_cast<unsigned>(localTime.wMinute));
        g_wristHudClockRefreshAt = clockNow + 500;
    }
    DrawWristHudText(
        drawPrim, center, right, up,
        -1.43F, 5.55F, 0.78F, g_wristHudClockText,
        88, 230, 242, 255);
    DrawWristHudRect(
        drawPrim, center, right, up,
        -7.35F, 4.20F, -4.9F, 4.34F,
        76, 235, 245, 250);
    DrawWristHudRect(
        drawPrim, center, right, up,
        4.9F, 4.20F, 7.35F, 4.34F,
        76, 235, 245, 250);

    // Hauptsektionen: Vitals, Munition, vier Inventarzellen und Atemluft.
    DrawWristHudRect(
        drawPrim, center, right, up,
        -7.35F, 1.25F, 7.35F, 1.34F,
        31, 126, 143, 235);
    DrawWristHudRect(
        drawPrim, center, right, up,
        -7.35F, -0.92F, 7.35F, -0.83F,
        31, 126, 143, 235);
    DrawWristHudRect(
        drawPrim, center, right, up,
        -7.35F, -3.48F, 7.35F, -3.39F,
        31, 126, 143, 235);
    DrawWristHudRect(
        drawPrim, center, right, up,
        -0.045F, 1.34F, 0.045F, 4.15F,
        31, 126, 143, 235);
    for (int divider = 1; divider < 4; ++divider) {
        const float x = -7.35F + 3.675F * static_cast<float>(divider);
        DrawWristHudRect(
            drawPrim, center, right, up,
            x - 0.045F, -3.39F, x + 0.045F, -0.92F,
            31, 126, 143, 235);
    }

    const float healthRatio =
        static_cast<float>(data.health) /
        static_cast<float>(data.maxHealth);
    const float armorRatio =
        data.maxArmor == 0
            ? 0.0F
            : static_cast<float>(data.armor) /
                  static_cast<float>(data.maxArmor);
    const std::uint8_t healthRed =
        healthRatio < 0.3F ? 245 : 70;
    const std::uint8_t healthGreen =
        healthRatio < 0.3F ? 65 : 235;

    DrawWristHudText(
        drawPrim, center, right, up,
        -6.95F, 3.92F, 0.72F, "HP",
        healthRed, healthGreen, 100);
    DrawWristHudNumber(
        drawPrim, center, right, up,
        -4.75F, 4.02F, 1.12F, data.health,
        healthRed, healthGreen, 100);
    DrawWristHudText(
        drawPrim, center, right, up,
        0.65F, 3.92F, 0.72F, "AR",
        75, 170, 255);
    DrawWristHudNumber(
        drawPrim, center, right, up,
        2.85F, 4.02F, 1.12F, data.armor,
        75, 170, 255);
    DrawWristHudBar(
        drawPrim, center, right, up,
        -6.95F, 1.73F, 6.20F, 0.42F, healthRatio,
        healthRed, healthGreen, 90);
    DrawWristHudBar(
        drawPrim, center, right, up,
        0.65F, 1.73F, 6.30F, 0.42F, armorRatio,
        65, 155, 255);

    DrawWristHudText(
        drawPrim, center, right, up,
        -6.95F, 0.79F, 0.72F, "AM",
        255, 192, 72);
    DrawWristHudNumber(
        drawPrim, center, right, up,
        -4.35F, 0.93F, 1.34F, data.ammo,
        255, 220, 105);

    DrawWristHudText(
        drawPrim, center, right, up,
        -6.95F, -1.37F, 0.54F, "GR",
        245, 132, 70);
    DrawWristHudNumber(
        drawPrim, center, right, up,
        -5.45F, -2.04F, 0.92F, data.throwables[0],
        255, 180, 95);
    DrawWristHudText(
        drawPrim, center, right, up,
        -3.28F, -1.37F, 0.54F, "MI",
        205, 105, 245);
    DrawWristHudNumber(
        drawPrim, center, right, up,
        -1.78F, -2.04F, 0.92F, data.throwables[1],
        225, 145, 255);
    DrawWristHudText(
        drawPrim, center, right, up,
        0.40F, -1.37F, 0.54F, "RC",
        245, 88, 105);
    DrawWristHudNumber(
        drawPrim, center, right, up,
        1.90F, -2.04F, 0.92F, data.throwables[2],
        255, 145, 150);
    DrawWristHudText(
        drawPrim, center, right, up,
        4.07F, -1.37F, 0.54F, "MK",
        90, 235, 150);
    DrawWristHudNumber(
        drawPrim, center, right, up,
        5.57F, -2.04F, 0.92F, data.medkits,
        125, 255, 175);

    DrawWristHudText(
        drawPrim, center, right, up,
        -6.95F, -3.77F, 0.48F, "AIR",
        70, 205, 235, 235);
    DrawWristHudBar(
        drawPrim, center, right, up,
        -4.65F, -4.18F, 11.60F, 0.36F, data.airPercent,
        45, 205, 245);

    FlushWristHudRects(drawPrim);
    drawPrim->SetRenderMode(oldRenderMode);
    drawPrim->SetZMode(oldZMode);
    drawPrim->SetTransformMode(oldTransformMode);
    drawPrim->SetCamera(oldCamera);

    if (InterlockedCompareExchange(
            &g_wristHudActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "wrist_hud_active",
            "The technical VR status HUD appears in front of the support "
            "wrist only while the player looks at it and is not holding "
            "the weapon with both hands. With Dual Pistols, the support "
            "weapon must be deliberately turned away from the firing "
            "direction and held briefly; aiming or firing hides it "
            "immediately.");
    }
}

void RenderWeaponAimGuide(HLOCALOBJ camera) noexcept {
    if (g_client == nullptr || camera == nullptr ||
        !g_weaponAim.valid || !g_weaponAimGuideEnabled ||
        // Ohne Waffe in der Hand kein Strahl aus der Hand.
        g_weaponDisabled) {
        return;
    }

    LTVector rayStart;
    LTVector rayEnd;
    if (!TraceWeaponAim(rayStart, rayEnd)) {
        return;
    }

    ILTDrawPrim* const drawPrim = g_client->GetDrawPrim();
    if (drawPrim == nullptr) {
        return;
    }

    const HOBJECT oldCamera = drawPrim->GetCamera();
    const ELTDrawPrimTransformMode oldTransformMode =
        drawPrim->GetTransformMode();
    const ELTDrawPrimZMode oldZMode = drawPrim->GetZMode();
    const ELTDrawPrimRenderMode oldRenderMode =
        drawPrim->GetRenderMode();

    drawPrim->SetCamera(camera);
    drawPrim->SetTransformMode(eLTDrawPrimTransformMode_World);
    drawPrim->SetZMode(eLTDrawPrimZMode_NoWrite);
    drawPrim->SetRenderMode(
        eLTDrawPrimRenderMode_Modulate_Additive);

    LT_LINEG laser;
    laser.verts[0].pos = rayStart;
    laser.verts[1].pos = rayEnd;
    laser.verts[0].rgba.Init(255, 8, 8, 235);
    laser.verts[1].rgba.Init(255, 0, 0, 190);
    drawPrim->DrawPrim(&laser);

    LTVector leftRayStart;
    LTVector leftRayEnd;
    if (TraceLeftWeaponAim(leftRayStart, leftRayEnd)) {
        LT_LINEG leftLaser;
        leftLaser.verts[0].pos = leftRayStart;
        leftLaser.verts[1].pos = leftRayEnd;
        leftLaser.verts[0].rgba.Init(255, 45, 12, 235);
        leftLaser.verts[1].rgba.Init(255, 8, 0, 190);
        drawPrim->DrawPrim(&leftLaser);
    }

    drawPrim->SetRenderMode(oldRenderMode);
    drawPrim->SetZMode(oldZMode);
    drawPrim->SetTransformMode(oldTransformMode);
    drawPrim->SetCamera(oldCamera);

    if (InterlockedCompareExchange(
            &g_weaponAimGuideActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "weapon_aim_guide_active",
            "Red collision rays follow the independently aimed OpenXR "
            "weapons; the stereo crosshair is disabled.");
    }
}

void RenderWeaponCollisionBox(HLOCALOBJ camera) noexcept;

void SetDevMenuQuad(
    LT_POLYG4& quad, const LTVector& center,
    const LTVector& right, const LTVector& up,
    float halfWidth, float halfHeight,
    std::uint8_t red, std::uint8_t green,
    std::uint8_t blue, std::uint8_t alpha) noexcept {
    quad.verts[0].pos = center - right * halfWidth + up * halfHeight;
    quad.verts[1].pos = center + right * halfWidth + up * halfHeight;
    quad.verts[2].pos = center + right * halfWidth - up * halfHeight;
    quad.verts[3].pos = center - right * halfWidth - up * halfHeight;
    for (LT_VERTG& vertex : quad.verts) {
        vertex.rgba.Init(red, green, blue, alpha);
    }
}

void FormatCurrentWeaponName(
    wchar_t* text, std::size_t textCount) noexcept {
    if (text == nullptr || textCount == 0) {
        return;
    }
    text[0] = L'\0';
    const WeightedWeaponInputState& weighted = g_weightedWeaponInput;
    if (weighted.weapon == nullptr || weighted.profileName[0] == '\0') {
        _snwprintf_s(text, textCount, _TRUNCATE, L"NONE");
        return;
    }
    if (std::strcmp(weighted.profileName, "default") == 0) {
        _snwprintf_s(text, textCount, _TRUNCATE, L"UNKNOWN");
        return;
    }
    struct FriendlyWeaponName {
        const char* profile;
        const wchar_t* display;
    };
    constexpr FriendlyWeaponName names[] = {
        {"pistol", L"PISTOL"},
        {"rdpistol", L"RD PISTOL"},
        {"submachinegun", L"SUBMACHINE GUN"},
        {"assaultrifle", L"ASSAULT RIFLE"},
        {"shotgun", L"SHOTGUN"},
        {"nailgun", L"PENETRATOR"},
        {"rocketlauncher", L"ROCKET LAUNCHER"},
        {"cannon", L"CANNON"},
        {"plasmaweapon", L"PARTICLE WEAPON"},
    };
    for (const FriendlyWeaponName& name : names) {
        if (std::strcmp(weighted.profileName, name.profile) == 0) {
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"%s", name.display);
            return;
        }
    }
    std::size_t used = 0;
    for (; weighted.profileName[used] != '\0' &&
           used + 1 < textCount; ++used) {
        const unsigned char value = static_cast<unsigned char>(
            weighted.profileName[used]);
        text[used] = value == '_'
            ? L' '
            : static_cast<wchar_t>(
                  value >= 'a' && value <= 'z'
                      ? value - 'a' + 'A' : value);
    }
    text[used] = L'\0';
}

void FormatDevMenuRow(
    DevMenuTab tab, std::size_t row, wchar_t* text,
    std::size_t textCount) noexcept {
    if (text == nullptr || textCount == 0) {
        return;
    }
    text[0] = L'\0';
    const wchar_t* const onOff[] = {L"OFF", L"ON"};
    switch (tab) {
    case DevMenuTab::recoil: {
        const WeaponRecoilProfile& recoil =
            DisplayedWeaponRecoilProfile();
        switch (row) {
        case 0:
            _snwprintf_s(text, textCount, _TRUNCATE, L"RECOIL: %s",
                         onOff[g_weaponRecoilEnabled ? 1 : 0]);
            break;
        case 1:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"EDIT PROFILE: %s",
                EditingCurrentWeaponRecoilProfile()
                    ? L"CURRENT" : L"DEFAULT");
            break;
        case 2:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"RECOIL STRENGTH: %.0f%%",
                static_cast<double>(recoil.strength * 100.0F));
            break;
        case 3:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"MUZZLE RISE: %.0f%%",
                static_cast<double>(recoil.muzzleRise * 100.0F));
            break;
        case 4:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"RECOVERY SPEED: %.0f%%",
                static_cast<double>(recoil.recovery * 100.0F));
            break;
        }
        break;
    }
    case DevMenuTab::weight: {
        const WeaponWeightProfile& weight = EditableWeaponWeightProfile();
        static constexpr const wchar_t* kWeightPresetNames[]{
            L"NONE", L"LIGHT", L"MEDIUM", L"HEAVY"};
        const int preset = std::clamp(
            static_cast<int>(g_weaponWeightPreset), 0, 3);
        switch (row) {
        case 0:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"SIMULATED WEIGHT: %s",
                kWeightPresetNames[preset]);
            break;
        case 1:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"EDIT PROFILE: %s",
                EditingCurrentWeaponWeightProfile()
                    ? L"CURRENT" : L"DEFAULT");
            break;
        case 2:
            _snwprintf_s(text, textCount, _TRUNCATE, L"WEIGHT: %.2fX",
                         static_cast<double>(weight.weight));
            break;
        case 3:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"POSITION FOLLOW: %.0f",
                static_cast<double>(weight.positionalFollow));
            break;
        case 4:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"ROTATION FOLLOW: %.0f",
                static_cast<double>(weight.rotationalFollow));
            break;
        case 5:
            _snwprintf_s(text, textCount, _TRUNCATE, L"CATCH-UP: %.1f",
                         static_cast<double>(weight.catchUpStrength));
            break;
        }
        break;
    }
    case DevMenuTab::collision: {
        const WeaponCollisionProfile& collision =
            DisplayedWeaponCollisionProfile();
        switch (row) {
        case 0:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"WORLD COLLISION: %s",
                onOff[g_weaponCollisionEnabled ? 1 : 0]);
            break;
        case 1:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"SHOW COLLISION BOX: %s",
                onOff[g_weaponCollisionDebugEnabled ? 1 : 0]);
            break;
        case 2:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"EDIT PROFILE: %s",
                EditingCurrentWeaponCollisionProfile()
                    ? L"CURRENT" : L"DEFAULT");
            break;
        case 3:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"BOX LENGTH: %.0f%%",
                static_cast<double>(collision.lengthScale * 100.0F));
            break;
        case 4:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"BOX WIDTH: %.0f CM",
                static_cast<double>(collision.widthMeters * 100.0F));
            break;
        case 5:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"BOX HEIGHT: %.0f CM",
                static_cast<double>(collision.heightMeters * 100.0F));
            break;
        case 6:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"FORWARD OFFSET: %+.0f CM",
                static_cast<double>(
                    collision.forwardOffsetMeters * 100.0F));
            break;
        }
        break;
    }
    case DevMenuTab::weapon:
        switch (row) {
        case 0:
            _snwprintf_s(text, textCount, _TRUNCATE, L"AIM GUIDE: %s",
                         onOff[g_weaponAimGuideEnabled ? 1 : 0]);
            break;
        case 1:
            _snwprintf_s(text, textCount, _TRUNCATE, L"SHOW ARMS: %s",
                         onOff[g_showPlayerArms ? 1 : 0]);
            break;
        case 2:
            _snwprintf_s(text, textCount, _TRUNCATE, L"TWO-HAND GRIP: %s",
                         onOff[g_twoHandedGripEnabled ? 1 : 0]);
            break;
        }
        break;
    case DevMenuTab::ik:
        if (!g_devMenuIkLeftHandPage) {
            switch (row) {
            case 0:
                _snwprintf_s(
                    text, textCount, _TRUNCATE, L"IK PAGE: ELBOWS");
                break;
            case 1:
                _snwprintf_s(
                    text, textCount, _TRUNCATE, L"SHOW ARMS: %s",
                    onOff[g_showPlayerArms ? 1 : 0]);
                break;
            case 2:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"ELBOW OUTWARD: %.0f%%",
                    static_cast<double>(
                        g_armIkTuning.elbowOutward * 100.0F));
                break;
            case 3:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"ELBOW DOWN: %.0f%%",
                    static_cast<double>(
                        g_armIkTuning.elbowDown * 100.0F));
                break;
            case 4:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"ELBOW BACK: %+.0f%%",
                    static_cast<double>(
                        g_armIkTuning.elbowBack * 100.0F));
                break;
            case 5:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"ELBOW STABILITY: %s",
                    onOff[g_armIkTuning.preserveElbowContinuity ? 1 : 0]);
                break;
            case 6:
                _snwprintf_s(
                    text, textCount, _TRUNCATE, L"RESET ALL IK");
                break;
            }
        } else {
            switch (row) {
            case 0:
                _snwprintf_s(
                    text, textCount, _TRUNCATE, L"IK PAGE: LEFT HAND");
                break;
            case 1:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HAND RIGHT: %+.1f CM",
                    static_cast<double>(
                        g_armIkTuning.leftHandRightMeters * 100.0F));
                break;
            case 2:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HAND UP: %+.1f CM",
                    static_cast<double>(
                        g_armIkTuning.leftHandUpMeters * 100.0F));
                break;
            case 3:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HAND FORWARD: %+.1f CM",
                    static_cast<double>(
                        g_armIkTuning.leftHandForwardMeters * 100.0F));
                break;
            case 4:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HAND PITCH: %+.0f DEG",
                    static_cast<double>(
                        g_armIkTuning.leftHandPitchDegrees));
                break;
            case 5:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HAND YAW: %+.0f DEG",
                    static_cast<double>(
                        g_armIkTuning.leftHandYawDegrees));
                break;
            case 6:
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HAND ROLL: %+.0f DEG",
                    static_cast<double>(
                        g_armIkTuning.leftHandRollDegrees));
                break;
            }
        }
        break;
    case DevMenuTab::movement: {
        const wchar_t* const turnSpeeds[] = {L"SLOW", L"NORMAL", L"FAST"};
        switch (row) {
        case 0:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"HMD TRANSLATION: %s",
                onOff[QueryBooleanOption(
                    g_isTranslationEnabled, false) ? 1 : 0]);
            break;
        case 1:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"MOVE DIRECTION: %s",
                g_headRelativeMovement ? L"HEAD" : L"BODY");
            break;
        case 2:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"HEAD BOB: %s",
                g_forceHeadBobDisabled && !g_headBobEnabled
                    ? L"LOCKED OFF" : onOff[g_headBobEnabled ? 1 : 0]);
            break;
        case 3:
            _snwprintf_s(text, textCount, _TRUNCATE, L"PHYSICAL LEAN: %s",
                         onOff[g_physicalLeanEnabled ? 1 : 0]);
            break;
        case 4:
            _snwprintf_s(text, textCount, _TRUNCATE,
                         L"LEAN STRENGTH: %d%%", g_leanScalePercent);
            break;
        case 5:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"TURN SPEED: %s",
                turnSpeeds[std::clamp(g_turnSpeedPreset, 0, 2)]);
            break;
        case 6:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"CLIMBING: %s",
                g_climbingEnabled ? L"HANDS" : L"CLASSIC");
            break;
        case 7:
            if (!g_headTracking.floorSpace) {
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HEIGHT MODE: LOCAL FALLBACK");
            } else if (g_configuredEyeHeightMeters > 0.0F) {
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HEIGHT: SET %.0f CM (RESET)",
                    static_cast<double>(
                        g_configuredEyeHeightMeters * 100.0F));
            } else if (IsPlausibleEyeHeightMeters(
                           g_headTracking.sessionEyeHeightMeters)) {
                _snwprintf_s(
                    text, textCount, _TRUNCATE,
                    L"HEIGHT: AUTO %.0f CM",
                    static_cast<double>(
                        g_headTracking.sessionEyeHeightMeters * 100.0F));
            } else {
                _snwprintf_s(
                    text, textCount, _TRUNCATE, L"HEIGHT: AUTO");
            }
            break;
        case 8:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"SET HEIGHT FROM HMD");
            break;
        }
        break;
    }
    case DevMenuTab::melee:
        switch (row) {
        case 0:
            _snwprintf_s(text, textCount, _TRUNCATE, L"MELEE GESTURES: %s",
                         onOff[g_meleeThrustEnabled ? 1 : 0]);
            break;
        case 1:
            _snwprintf_s(text, textCount, _TRUNCATE, L"WEAPON STRIKE: %s",
                         onOff[g_meleeWeaponStrikeEnabled ? 1 : 0]);
            break;
        case 2:
            _snwprintf_s(text, textCount, _TRUNCATE, L"OFF-HAND STRIKE: %s",
                         onOff[g_meleeOffHandStrikeEnabled ? 1 : 0]);
            break;
        case 3:
            _snwprintf_s(text, textCount, _TRUNCATE, L"JUMP KICK: %s",
                         onOff[g_meleeJumpKickEnabled ? 1 : 0]);
            break;
        case 4:
            _snwprintf_s(text, textCount, _TRUNCATE, L"SLIDE KICK: %s",
                         onOff[g_meleeSlideKickEnabled ? 1 : 0]);
            break;
        }
        break;
    case DevMenuTab::vr:
        switch (row) {
        case 0:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"STEREO HUD: %s",
                onOff[QueryBooleanOption(g_isStereoHudEnabled, true) ? 1 : 0]);
            break;
        case 1:
            _snwprintf_s(text, textCount, _TRUNCATE, L"FOV SCALE: %u%%",
                         CurrentFovScalePercent());
            break;
        case 2:
            _snwprintf_s(text, textCount, _TRUNCATE, L"HANDEDNESS: %s",
                         g_leftHandedBindings ? L"LEFT" : L"RIGHT");
            break;
        case 3:
            _snwprintf_s(text, textCount, _TRUNCATE, L"HAPTICS: %s",
                         onOff[g_controllerHapticsEnabled ? 1 : 0]);
            break;
        case 4:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"WEAPON DIAGNOSTICS: %s",
                onOff[g_weaponWeightDiagnosticsEnabled ? 1 : 0]);
            break;
        case 5:
            _snwprintf_s(
                text, textCount, _TRUNCATE, L"RENDER SCALE: %u%%",
                g_renderScalePercent);
            break;
        }
        break;
    }
}

void AppendDevMenuBitmapText(
    const wchar_t* text, const LTVector& anchor,
    float horizontalAnchor, float pixelSizeMeters,
    std::uint8_t red, std::uint8_t green,
    std::uint8_t blue, std::uint8_t alpha,
    std::size_t& quadCount) noexcept {
    if (text == nullptr || pixelSizeMeters <= 0.0F ||
        quadCount >= g_devMenuGlyphQuads.size()) {
        return;
    }
    const std::size_t length = std::wcslen(text);
    if (length == 0) {
        return;
    }
    const float pixelSize = pixelSizeMeters * kGameUnitsPerMeter;
    const float totalWidth =
        (static_cast<float>(length * 6U - 1U)) * pixelSize;
    const float startX = -horizontalAnchor * totalWidth;
    const LTVector towardViewer = g_devMenu.normal * -0.42F;

    for (std::size_t character = 0; character < length; ++character) {
        const auto* const glyph = DevMenuGlyphRows(text[character]);
        if (glyph == nullptr) {
            continue;
        }
        for (std::size_t row = 0; row < glyph->size(); ++row) {
            const std::uint8_t bits = (*glyph)[row];
            for (std::size_t column = 0; column < 5U; ++column) {
                if ((bits & (1U << (4U - column))) == 0 ||
                    quadCount >= g_devMenuGlyphQuads.size()) {
                    continue;
                }
                const float x = startX +
                    static_cast<float>(character * 6U + column) *
                        pixelSize +
                    pixelSize * 0.5F;
                const float y =
                    (3.0F - static_cast<float>(row)) * pixelSize;
                SetDevMenuQuad(
                    g_devMenuGlyphQuads[quadCount++],
                    anchor + g_devMenu.right * x +
                        g_devMenu.up * y + towardViewer,
                    g_devMenu.right, g_devMenu.up,
                    pixelSize * 0.43F, pixelSize * 0.43F,
                    red, green, blue, alpha);
            }
        }
    }
}

void AnchorFloatingDevMenu(HLOCALOBJ camera) noexcept {
    if (g_client == nullptr || camera == nullptr) {
        return;
    }
    LTRigidTransform cameraTransform;
    if (g_client->GetObjectTransform(camera, &cameraTransform) != LT_OK) {
        return;
    }
    LTVector right;
    LTVector up;
    LTVector forward;
    cameraTransform.m_rRot.GetVectors(right, up, forward);
    right.Normalize();
    up.Normalize();
    forward.Normalize();
    g_devMenu.center = cameraTransform.m_vPos +
        forward * (kDevMenuDistanceMeters * kGameUnitsPerMeter) -
        up * (kDevMenuVerticalOffsetMeters * kGameUnitsPerMeter);
    g_devMenu.right = right;
    g_devMenu.up = up;
    g_devMenu.normal = forward;
    g_devMenu.anchorValid = true;
}

void RenderFloatingDevMenu(HLOCALOBJ camera) noexcept {
    if (!g_devMenu.open || g_client == nullptr || camera == nullptr) {
        return;
    }
    if (!g_devMenu.anchorValid) {
        AnchorFloatingDevMenu(camera);
    }
    if (!g_devMenu.anchorValid) {
        return;
    }

    constexpr float kWidth = kDevMenuWidthMeters * kGameUnitsPerMeter;
    constexpr float kHeight = kDevMenuHeightMeters * kGameUnitsPerMeter;
    constexpr float kHeader = kDevMenuHeaderMeters * kGameUnitsPerMeter;
    constexpr float kTitle = kDevMenuTitleMeters * kGameUnitsPerMeter;
    constexpr float kTab = kDevMenuTabMeters * kGameUnitsPerMeter;
    constexpr float kRow = kDevMenuRowMeters * kGameUnitsPerMeter;
    constexpr float kTextInset = 0.035F * kGameUnitsPerMeter;
    const LTVector towardViewer = g_devMenu.normal * -0.12F;

    ILTDrawPrim* const drawPrim = g_client->GetDrawPrim();
    if (drawPrim == nullptr) {
        return;
    }
    const HOBJECT oldCamera = drawPrim->GetCamera();
    const ELTDrawPrimTransformMode oldTransformMode =
        drawPrim->GetTransformMode();
    const ELTDrawPrimZMode oldZMode = drawPrim->GetZMode();
    const ELTDrawPrimRenderMode oldRenderMode =
        drawPrim->GetRenderMode();

    drawPrim->SetCamera(camera);
    drawPrim->SetTransformMode(eLTDrawPrimTransformMode_World);
    drawPrim->SetZMode(eLTDrawPrimZMode_None);
    drawPrim->SetRenderMode(
        eLTDrawPrimRenderMode_Modulate_Translucent);
    const bool drawBlockActive =
        drawPrim->BeginDrawPrimBlock() == LT_OK;

    LT_POLYG4 background;
    SetDevMenuQuad(
        background, g_devMenu.center, g_devMenu.right, g_devMenu.up,
        kWidth * 0.5F, kHeight * 0.5F, 4, 10, 18, 225);
    drawPrim->DrawPrim(&background);

    const float tabWidth = kWidth / static_cast<float>(kDevMenuTabCount);
    const float tabY = kHeight * 0.5F - kTitle - kTab * 0.5F;
    for (std::size_t tab = 0; tab < kDevMenuTabCount; ++tab) {
        const bool active =
            tab == static_cast<std::size_t>(g_devMenu.selectedTab);
        const bool hovered =
            g_devMenu.pointerValid &&
            g_devMenu.pointerRegion == DevMenuHitRegion::tab &&
            g_devMenu.pointerIndex == tab;
        if (!active && !hovered) {
            continue;
        }
        const float tabX = -kWidth * 0.5F +
            (static_cast<float>(tab) + 0.5F) * tabWidth;
        LT_POLYG4 tabHighlight;
        SetDevMenuQuad(
            tabHighlight,
            g_devMenu.center + g_devMenu.right * tabX +
                g_devMenu.up * tabY + towardViewer,
            g_devMenu.right, g_devMenu.up,
            tabWidth * 0.46F, kTab * 0.40F,
            hovered ? 150 : 20, hovered ? 95 : 125,
            hovered ? 20 : 170, 220);
        drawPrim->DrawPrim(&tabHighlight);
    }

    const float selectedY = kHeight * 0.5F - kHeader -
        (static_cast<float>(g_devMenu.selectedRow) + 0.5F) * kRow;
    LT_POLYG4 highlight;
    SetDevMenuQuad(
        highlight,
        g_devMenu.center + g_devMenu.up * selectedY + towardViewer,
        g_devMenu.right, g_devMenu.up,
        kWidth * 0.47F, kRow * 0.44F, 20, 125, 170, 210);
    drawPrim->DrawPrim(&highlight);

    if (g_devMenu.pointerValid) {
        LT_POLYG4 pointer;
        SetDevMenuQuad(
            pointer, g_devMenu.pointerWorld + towardViewer * 2.0F,
            g_devMenu.right, g_devMenu.up,
            0.007F * kGameUnitsPerMeter,
            0.007F * kGameUnitsPerMeter, 255, 190, 40, 255);
        drawPrim->DrawPrim(&pointer);
        if (g_weaponAim.valid) {
            LT_LINEG ray;
            ray.verts[0].pos = g_weaponAim.fireTransform.m_vPos;
            ray.verts[1].pos = g_devMenu.pointerWorld;
            ray.verts[0].rgba.Init(40, 220, 255, 210);
            ray.verts[1].rgba.Init(255, 190, 40, 245);
            drawPrim->DrawPrim(&ray);
        }
    }

    std::size_t glyphQuadCount = 0;
    const LTVector titleAnchor =
        g_devMenu.center + g_devMenu.up *
            (kHeight * 0.5F - kTitle * 0.43F);
    wchar_t title[80] = L"FEAR VR LIVE TUNING";
    if (g_devMenu.selectedTab == DevMenuTab::recoil ||
        g_devMenu.selectedTab == DevMenuTab::weight ||
        g_devMenu.selectedTab == DevMenuTab::collision ||
        g_devMenu.selectedTab == DevMenuTab::weapon) {
        wchar_t weaponName[48]{};
        FormatCurrentWeaponName(weaponName, std::size(weaponName));
        _snwprintf_s(
            title, std::size(title), _TRUNCATE,
            L"EQUIPPED: %s", weaponName);
    }
    AppendDevMenuBitmapText(
        title, titleAnchor, 0.5F, 0.0034F,
        240, 250, 255, 255, glyphQuadCount);

    for (std::size_t tab = 0; tab < kDevMenuTabCount; ++tab) {
        const float tabX = -kWidth * 0.5F +
            (static_cast<float>(tab) + 0.5F) * tabWidth;
        const bool active =
            tab == static_cast<std::size_t>(g_devMenu.selectedTab);
        AppendDevMenuBitmapText(
            kDevMenuTabLabels[tab],
            g_devMenu.center + g_devMenu.right * tabX +
                g_devMenu.up * tabY,
            0.5F, 0.00155F,
            active ? 255 : 160, active ? 220 : 195,
            active ? 80 : 210, 255, glyphQuadCount);
    }

    const std::size_t rowCount = DevMenuRowCount(g_devMenu.selectedTab);
    for (std::size_t row = 0; row < rowCount; ++row) {
        wchar_t text[80]{};
        FormatDevMenuRow(
            g_devMenu.selectedTab, row, text, std::size(text));
        const float rowY = kHeight * 0.5F - kHeader -
            (static_cast<float>(row) + 0.5F) * kRow;
        const LTVector anchor =
            g_devMenu.center - g_devMenu.right *
                (kWidth * 0.5F - kTextInset) +
            g_devMenu.up * rowY;
        const bool selected = row == g_devMenu.selectedRow;
        AppendDevMenuBitmapText(
            text, anchor, 0.0F, 0.0035F,
            selected ? 255 : 220,
            selected ? 220 : 240,
            selected ? 80 : 248,
            255, glyphQuadCount);
    }
    const LTVector helpAnchor =
        g_devMenu.center - g_devMenu.up *
            (kHeight * 0.5F - 0.025F * kGameUnitsPerMeter);
    AppendDevMenuBitmapText(
        L"POINT + TRIGGER/A: CHANGE  STICK: NAV  B: CLOSE",
        helpAnchor, 0.5F, 0.00185F,
        155, 200, 220, 255, glyphQuadCount);
    if (glyphQuadCount != 0) {
        drawPrim->DrawPrim(
            g_devMenuGlyphQuads.data(),
            static_cast<std::uint32_t>(glyphQuadCount));
    }

    if (drawBlockActive) {
        drawPrim->EndDrawPrimBlock();
    }

    drawPrim->SetRenderMode(oldRenderMode);
    drawPrim->SetZMode(oldZMode);
    drawPrim->SetTransformMode(oldTransformMode);
    drawPrim->SetCamera(oldCamera);
}

static_assert(sizeof(void*) == 4,
              "The GameClient loader must remain x86.");

// Read-only Abbildung der in ltmodule.h/ltmodule.cpp dokumentierten
// VC7.1-Layouts. Wir registrieren keine zusätzlichen Holder im Engine-
// Datenbestand, sondern lesen ausschließlich die bereits ausgewählte
// Default-Implementierung.
struct InterfaceArrayAbi {
    std::uint32_t count;
    std::uint32_t capacity;
    void** items;
};

struct InterfaceDatabaseAbi {
    void** vtable;
    void* trackedPointers;
    InterfaceArrayAbi* interfaces;
};

struct InterfaceNameManagerAbi {
    void** vtable;
    const char* name;
    std::int32_t version;
    void* implementations;
    void* holders;
    void* currentInterface;
};

static_assert(sizeof(InterfaceArrayAbi) == 12,
              "Official database_array ABI changed.");
static_assert(sizeof(InterfaceDatabaseAbi) == 12,
              "Official CInterfaceDatabase ABI changed.");
static_assert(sizeof(InterfaceNameManagerAbi) == 24,
              "Official CInterfaceNameMgr ABI changed.");

void* FindCurrentInterface(void* masterDatabase, const char* name,
                           std::int32_t version) noexcept {
    __try {
        auto* database =
            static_cast<InterfaceDatabaseAbi*>(masterDatabase);
        InterfaceArrayAbi* array = database->interfaces;
        if (array == nullptr || array->count > array->capacity ||
            array->count > 4096 || array->items == nullptr) {
            return nullptr;
        }
        for (std::uint32_t index = 0; index < array->count; ++index) {
            auto* manager = static_cast<InterfaceNameManagerAbi*>(
                array->items[index]);
            if (manager != nullptr && manager->name != nullptr &&
                manager->version == version &&
                std::strcmp(manager->name, name) == 0) {
                return manager->currentInterface;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

class StereoRenderGuard {
public:
    StereoRenderGuard() noexcept {
        g_inStereoRender = true;
    }
    ~StereoRenderGuard() {
        g_inStereoRender = false;
    }

    StereoRenderGuard(const StereoRenderGuard&) = delete;
    StereoRenderGuard& operator=(const StereoRenderGuard&) = delete;
};

bool PrepareTrackedEyePoses(
    const FearVrRenderRequest& request,
    RelativeEyePose (&eyePose)[FEARVR_EYE_COUNT]) noexcept {
    const FearVrPose currentCenter = CenterHeadPose(request);
    if (!IsValidPose(currentCenter)) {
        return false;
    }
    g_headTracking.currentCenter = currentCenter;
    g_headTracking.floorSpace =
        (request.flags & FEARVR_RF_FLOOR_SPACE) != 0;
    g_headTracking.verticalTranslationOverride = false;
    g_headTracking.verticalTranslationMeters = 0.0F;
    if (g_headTracking.floorSpace) {
        if (!IsPlausibleEyeHeightMeters(
                g_headTracking.sessionEyeHeightMeters) &&
            IsPlausibleEyeHeightMeters(currentCenter.py)) {
            g_headTracking.sessionEyeHeightMeters = currentCenter.py;
        }
        const float activeEyeHeight =
            g_configuredEyeHeightMeters > 0.0F
            ? g_configuredEyeHeightMeters
            : g_headTracking.sessionEyeHeightMeters;
        if (IsPlausibleEyeHeightMeters(activeEyeHeight)) {
            g_headTracking.verticalTranslationOverride = true;
            g_headTracking.verticalTranslationMeters =
                CalibratedVerticalOffsetMeters(
                    currentCenter.py, activeEyeHeight);
        }
    }

    const ULONGLONG now = GetTickCount64();
    LONG resetGeneration = InterlockedCompareExchange(
        &g_trackingResetGeneration, 0, 0);
    if (request.frameId == g_headTracking.lastFrameId) {
        if (g_headTracking.lastFreshFrameTick != 0 &&
            now - g_headTracking.lastFreshFrameTick > 250) {
            if (!g_headTracking.trackingLost) {
                Report(
                    "WARN", "head_tracking_lost",
                    "No fresh OpenXR pose for 250ms; mono fallback active.");
            }
            g_headTracking.trackingLost = true;
            g_headTracking.centered = false;
            return false;
        }
    } else {
        g_headTracking.lastFrameId = request.frameId;
        g_headTracking.lastFreshFrameTick = now;
        const bool originRecenterRequested =
            g_headTracking.centered &&
            g_headTracking.recenterGeneration !=
                request.recenterGeneration;
        // A bridge origin recenter invalidates controller and weapon filters
        // that share the old tracking basis. Advance the local reset once,
        // before recording the new basis, so presentation is rebuilt together
        // without causing another recenter on the following frame.
        if (originRecenterRequested) {
            resetGeneration =
                InterlockedIncrement(&g_trackingResetGeneration);
        }
        const bool recenter =
            !g_headTracking.centered ||
            g_headTracking.trackingLost ||
            originRecenterRequested ||
            g_headTracking.resetGeneration != resetGeneration;
        if (recenter) {
            const FearVrPose previousRecenter =
                g_headTracking.recenter;
            g_headTracking.recenter =
                YawOnlyRecenterPose(currentCenter);
            if (!IsValidPose(g_headTracking.recenter)) {
                g_headTracking.centered = false;
                return false;
            }
            g_headTracking.recenterGeneration =
                request.recenterGeneration;
            g_headTracking.resetGeneration = resetGeneration;
            g_headTracking.centered = true;
            g_headTracking.trackingLost = false;
            const float yawRadians = 2.0F * std::atan2(
                g_headTracking.recenter.qy,
                g_headTracking.recenter.qw);
            const float previousYawRadians =
                IsValidPose(previousRecenter)
                ? 2.0F * std::atan2(
                      previousRecenter.qy,
                      previousRecenter.qw)
                : yawRadians;
            constexpr float kRadiansToDegrees =
                57.29577951308232F;
            const float yawDeltaDegrees =
                std::remainder(
                    yawRadians - previousYawRadians,
                    6.283185307179586F) *
                kRadiansToDegrees;
            char message[320]{};
            std::snprintf(
                message, sizeof(message),
                "generation=%u current_q=(%.4f,%.4f,%.4f,%.4f) "
                "yaw=%.2f deg previous=%.2f deg delta=%.2f deg. "
                "Physical pitch and roll remain unchanged.",
                request.recenterGeneration,
                static_cast<double>(currentCenter.qx),
                static_cast<double>(currentCenter.qy),
                static_cast<double>(currentCenter.qz),
                static_cast<double>(currentCenter.qw),
                static_cast<double>(
                    yawRadians * kRadiansToDegrees),
                static_cast<double>(
                    previousYawRadians * kRadiansToDegrees),
                static_cast<double>(yawDeltaDegrees));
            Report(
                "INFO", "head_tracking_recentered",
                message);
        }
    }
    if (!g_headTracking.centered) {
        return false;
    }

    // Both modes use the same relative HMD center, but room-scale keeps a
    // normal play-area range while artificial lean retains its short cap.
    const bool roomScaleTranslation =
        (request.flags & FEARVR_RF_TRANSLATION_ON) != 0;
    const bool translationEnabled =
        roomScaleTranslation || g_physicalLeanEnabled;
    const float maximumTranslationMeters = roomScaleTranslation
        ? kRoomScaleMaxTranslationMeters
        : kPhysicalLeanMaxTranslationMeters;
    for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
        eyePose[eye] = EyePoseRelativeToRecenter(
            g_headTracking.recenter, currentCenter,
            request.eye[eye].pose, translationEnabled,
            maximumTranslationMeters, 1.0F,
            g_headTracking.verticalTranslationOverride,
            g_headTracking.verticalTranslationMeters);
        if (!eyePose[eye].valid) {
            return false;
        }
    }
    if (InterlockedCompareExchange(
            &g_headTrackingActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "head_tracking_active",
            translationEnabled
                ? "Relative HMD rotation and bounded translation are active."
                : "Relative HMD rotation is active; translation is disabled.");
    }
    return true;
}


// Monotone Uhr in Nanosekunden fuer die Gestenerkennung. `GetTickCount64`
// waere zu grob: Seine Aufloesung liegt bei etwa 16 ms und damit in der
// Groessenordnung eines ganzen Bildes.
std::uint64_t MonotonicNanoseconds() noexcept {
    static LARGE_INTEGER frequency{};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
        if (frequency.QuadPart == 0) {
            return 0;
        }
    }
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<std::uint64_t>(
        (counter.QuadPart * 1000000000LL) / frequency.QuadPart);
}

// Wie viel vom physischen Kopfversatz die Welt zulaesst.
//
// Ein Strahl von der Spielerkameraposition entlang des gewuenschten Versatzes
// misst die freie Strecke; `lean_collision.h` macht daraus den Anteil, der
// uebrig bleibt. Ohne diese Pruefung wanderte der Blickpunkt beim Lehnen in
// die Wand, denn die Kollisionskapsel des Spielers bleibt stehen.
//
// Einmal pro Bild, nicht pro Auge: Der Unterschied zwischen beiden Augen
// betraegt wenige Zentimeter und wuerde nur einen zweiten Strahl kosten.
float UpdateLeanTranslationScale(
    const LTVector& cameraPosition,
    const LTVector& worldOffset,
    bool collisionEnabled) noexcept {
    if (!collisionEnabled || g_client == nullptr) {
        ResetLeanCollision(g_leanCollision);
        return 1.0F;
    }
    const float desired = worldOffset.Mag();
    float target = 1.0F;
    if (desired > 1.0F) {
        const LTVector direction = worldOffset / desired;
        // Drei Strahlen statt einem: Mitte, und je einer seitlich versetzt.
        // Eine Kante liegt fast immer nur vor einem Teil des Kopfes; ein
        // einzelner Strahl trifft sie deshalb bildweise mal und mal nicht,
        // und die Begrenzung wechselte im selben Takt. Von allen dreien gilt
        // der engste Wert.
        LTVector sideways = direction.Cross(LTVector(0.0F, 1.0F, 0.0F));
        const float sidewaysLength = sideways.Mag();
        if (sidewaysLength > 0.001F) {
            sideways /= sidewaysLength;
        } else {
            sideways = LTVector(1.0F, 0.0F, 0.0F);
        }
        constexpr float kProbeSpreadUnits = 9.0F;
        const LTVector probeOrigins[] = {
            cameraPosition,
            cameraPosition + sideways * kProbeSpreadUnits,
            cameraPosition - sideways * kProbeSpreadUnits,
        };
        for (const LTVector& origin : probeOrigins) {
            IntersectQuery query;
            query.m_From = origin;
            query.m_To = origin + direction *
                (desired + kLeanCollisionMarginUnits);
            query.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
            IntersectInfo hit;
            __try {
                if (g_client->IntersectSegment(query, &hit)) {
                    const float scale = LeanCollisionScale(
                        desired, (hit.m_Point - origin).Mag(), true);
                    if (scale < target) {
                        target = scale;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                target = 1.0F;
                break;
            }
        }
    }
    return UpdateLeanCollision(
        g_leanCollision, target, MonotonicNanoseconds());
}

float CurrentCutsceneCameraBlend(ULONGLONG now) noexcept {
    if (!g_cutsceneCameraState ||
        g_cutsceneCameraActivationTick == 0 ||
        now < g_cutsceneCameraActivationTick) {
        return 0.0F;
    }
    constexpr float kCutsceneTransitionMilliseconds = 350.0F;
    const float linear = std::clamp(
        static_cast<float>(now - g_cutsceneCameraActivationTick) /
            kCutsceneTransitionMilliseconds,
        0.0F, 1.0F);
    return linear * linear * (3.0F - 2.0F * linear);
}

LTRotation ResolveCutsceneCameraBaseRotation(
    const LTRotation& retailRotation, float blend) noexcept {
    LTRotation resolved = retailRotation;
    ResolveSlideKickViewBase(retailRotation, resolved);
    if (blend <= 0.0F) {
        return resolved;
    }

    float ignoredPitch = 0.0F;
    float cinematicYaw = 0.0F;
    float ignoredRoll = 0.0F;
    if (!RotationToPitchYawRoll(
            retailRotation, ignoredPitch,
            cinematicYaw, ignoredRoll)) {
        return resolved;
    }
    LTRotation stabilized(0.0F, cinematicYaw, 0.0F);
    stabilized.Normalize();
    LTRotation blended;
    blended.Slerp(resolved, stabilized, blend);
    blended.Normalize();
    return blended;
}

void RebaseCutsceneWeaponPresentation(
    const LTRigidTransform& stableCameraBase) noexcept {
    if (!g_retailPersistentUnsupported ||
        !g_cutsceneCameraState ||
        !g_weaponAim.trackingBaseValid) {
        return;
    }

    const LTRigidTransform worldCorrection =
        stableCameraBase * g_weaponAim.trackingBase.GetInverse();
    if (g_weaponAim.valid) {
        g_weaponAim.fireTransform =
            worldCorrection * g_weaponAim.fireTransform;
    }
    if (g_weaponAim.gripValid) {
        g_weaponAim.gripTransform =
            worldCorrection * g_weaponAim.gripTransform;
    }
    if (g_weaponAim.leftAimValid) {
        g_weaponAim.leftAimTransform =
            worldCorrection * g_weaponAim.leftAimTransform;
    }
    if (g_weaponAim.leftGripValid) {
        g_weaponAim.leftGripTransform =
            worldCorrection * g_weaponAim.leftGripTransform;
    }
    if (g_weaponAim.muzzleValid) {
        g_weaponAim.muzzleTransform =
            worldCorrection * g_weaponAim.muzzleTransform;
    }
    g_weaponAim.trackingBase = stableCameraBase;

    // Retail placed the visible weapon during its earlier gameplay update.
    // Refresh it from the same late-latched basis as the hand bones so a
    // moving helicopter cannot leave the model one frame behind.
    if (g_weaponAim.retailWeapon != nullptr &&
        g_weaponAim.valid && g_weaponAim.gripValid &&
        g_retailSetWeaponTransform != nullptr) {
        const LTTransform synchronizedTransform(
            g_weaponAim.gripTransform.m_vPos,
            g_weaponAim.fireTransform.m_rRot, 1.0F);
        __try {
            g_retailSetWeaponTransform(
                g_weaponAim.retailWeapon,
                synchronizedTransform);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void StageBodyPresentation(
    const LTRigidTransform& retailCameraBase,
    const LTRigidTransform& stableCameraBase,
    const LTVector& gameplayWorldOffset) noexcept {
    g_bodyPresentationWorldOffset = LTVector(0.0F, 0.0F, 0.0F);
    g_bodyPresentationOffsetActive = false;
    if (g_playerBodyObject == nullptr ||
        !g_bodyPresentationNodeControl.installed) {
        g_cutsceneBodyPresentation = {};
        return;
    }

    __try {
        LTRigidTransform originalBodyTransform;
        if (g_client->GetObjectTransform(
                g_playerBodyObject,
                &originalBodyTransform) != LT_OK) {
            g_cutsceneBodyPresentation = {};
            return;
        }
        if (g_retailPersistentUnsupported &&
            g_cutsceneCameraState) {
            if (!g_cutsceneBodyPresentation.valid ||
                g_cutsceneBodyPresentation.bodyObject !=
                    g_playerBodyObject) {
                // Keep a world-space positional offset from the stable
                // camera. It deliberately contains no camera rotation:
                // node controls already solved the real controller poses.
                g_cutsceneBodyPresentation.bodyOffsetFromCamera =
                    originalBodyTransform.m_vPos -
                    retailCameraBase.m_vPos;
                g_cutsceneBodyPresentation.bodyObject =
                    g_playerBodyObject;
                g_cutsceneBodyPresentation.valid = true;
            }
            g_bodyPresentationWorldOffset =
                stableCameraBase.m_vPos +
                g_cutsceneBodyPresentation.bodyOffsetFromCamera -
                originalBodyTransform.m_vPos;
        } else {
            g_cutsceneBodyPresentation = {};
            if (gameplayWorldOffset.MagSqr() < 0.0001F) {
                return;
            }
            // Only the visible body follows room-scale head translation.
            // The player object and its collision capsule stay untouched.
            g_bodyPresentationWorldOffset = gameplayWorldOffset;
        }
        g_bodyPresentationOffsetActive =
            g_bodyPresentationWorldOffset.MagSqr() >= 0.0001F;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_cutsceneBodyPresentation = {};
        g_bodyPresentationWorldOffset = LTVector(0.0F, 0.0F, 0.0F);
        g_bodyPresentationOffsetActive = false;
    }
}

LTRESULT RenderStereo(ILTRenderer* renderer, HLOCALOBJ camera,
                      const char* techniqueOverride) {
    g_stereoStep = "check_arguments";
    if (renderer == nullptr || camera == nullptr) {
        g_stereoStep = "fallback_invalid_arguments";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "check_technique_override";
    if (techniqueOverride != nullptr) {
        g_stereoStep = "fallback_technique_override";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "check_recursion";
    if (g_inStereoRender) {
        g_stereoStep = "fallback_recursive_render";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "check_dependencies";
    if (g_client == nullptr ||
        g_isHostConnected == nullptr || g_isStereoEnabled == nullptr ||
        g_getRenderRequest == nullptr ||
        g_beginEye == nullptr || g_captureEye == nullptr ||
        g_endStereoFrame == nullptr) {
        g_stereoStep = "fallback_missing_dependency";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "query_stereo_enabled";
    if (g_disableStereoRender) {
        g_stereoStep = "stereo_disabled_by_switch";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    if (!g_isStereoEnabled()) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "query_host_connected";
    if (!g_isHostConnected()) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "read_render_request";
    FearVrRenderRequest request{};
    if (!g_getRenderRequest(&request) ||
        (request.flags & FEARVR_RF_VALID) == 0 ||
        request.frameId == 0) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    if (g_waitForNewRenderRequest != nullptr &&
        g_lastStereoRenderRequestId != 0) {
        g_stereoStep = "wait_for_fresh_render_request";
        FearVrRenderRequest freshRequest{};
        constexpr std::uint32_t kMaximumPacingWaitMilliseconds = 20;
        if (g_waitForNewRenderRequest(
                request.frameId,
                kMaximumPacingWaitMilliseconds,
                &freshRequest) &&
            (freshRequest.flags & FEARVR_RF_VALID) != 0 &&
            freshRequest.frameId != 0) {
            request = freshRequest;
        }
    }
    if ((request.flags & FEARVR_RF_FLATSCREEN) != 0) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "validate_fov_and_ipd";
    const SymmetricFov symmetric = SharedSymmetricFov(
        request.eye[FEARVR_EYE_LEFT].fov,
        request.eye[FEARVR_EYE_RIGHT].fov);
    const float ipd = InterpupillaryDistanceMeters(request);
    if (!symmetric.valid || !std::isfinite(ipd) ||
        ipd < 0.02F || ipd > 0.12F ||
        g_client->GetCameraFOV == nullptr ||
        g_client->SetCameraFOV == nullptr) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "prepare_head_tracking";
    RelativeEyePose trackedEye[FEARVR_EYE_COUNT]{};
    if (!PrepareTrackedEyePoses(request, trackedEye)) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "get_camera_transform";
    LTRigidTransform originalTransform;
    if (g_client->GetObjectTransform(
            camera, &originalTransform) != LT_OK) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    const bool cinematicCamera = IsRetailCinematicCamera();
    const ULONGLONG renderTick = GetTickCount64();
    const bool playingFrameFresh =
        g_lastWeaponManagerUpdateTick != 0 &&
        renderTick >= g_lastWeaponManagerUpdateTick &&
        renderTick - g_lastWeaponManagerUpdateTick <=
            kPlayingFrameFreshMilliseconds;
    bool playerMovementAllowed = true;
    const bool playerMovementKnown =
        ReadRetailPlayerMovementAllowed(playerMovementAllowed);
    bool animatedCamera = false;
    std::int32_t cameraDescriptor = -1;
    const bool cameraDescriptorKnown =
        ReadRetailAnimatedCamera(animatedCamera, cameraDescriptor);
    bool movingLure = false;
    std::int32_t physicsModel = -1;
    const bool physicsModelKnown =
        ReadRetailMovingLure(movingLure, physicsModel);
    // Not every in-engine cutscene switches to kCM_Cinematic. Scripted
    // PlayerCamera sequences can keep their old mode, but stop the weapon
    // manager for their whole duration. This is the same verified boundary
    // already used to suspend command injection safely.
    const bool cutsceneCamera =
        cinematicCamera || animatedCamera || movingLure ||
        g_retailPersistentUnsupported || !playingFrameFresh ||
        (playerMovementKnown && !playerMovementAllowed);
    if (!g_cutsceneCameraStateKnown ||
        cutsceneCamera != g_cutsceneCameraState) {
        g_cutsceneCameraStateKnown = true;
        g_cutsceneCameraState = cutsceneCamera;
        g_cutsceneCameraActivationTick =
            cutsceneCamera ? renderTick : 0;
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "active=%d cinematic_mode=%d playing_frame_fresh=%d "
            "movement_known=%d movement_allowed=%d "
            "descriptor_known=%d descriptor=%d animated_camera=%d "
            "physics_known=%d physics_model=%d moving_lure=%d "
            "persistent_unsupported=%d",
            cutsceneCamera ? 1 : 0,
            cinematicCamera ? 1 : 0,
            playingFrameFresh ? 1 : 0,
            playerMovementKnown ? 1 : 0,
            playerMovementAllowed ? 1 : 0,
            cameraDescriptorKnown ? 1 : 0,
            cameraDescriptor,
            animatedCamera ? 1 : 0,
            physicsModelKnown ? 1 : 0,
            physicsModel,
            movingLure ? 1 : 0,
            g_retailPersistentUnsupported ? 1 : 0);
        Report(
            "INFO", "vr_cutscene_camera_stabilization", message);
    }
    const float cutsceneBlend =
        CurrentCutsceneCameraBlend(renderTick);

    // Der Weapon-Manager sieht sowohl CPlayerCamera::m_vPos als auch die
    // anschliessend geglaettete Kameraobjekt-Hoehe. Seine Wahl wird fuer die
    // Augen wiederverwendet, damit Blick, Haende und Waffe auf Treppen oder im
    // Sprung niemals verschiedene vertikale Bezugspositionen erhalten.
    LTVector visualBasePosition = originalTransform.m_vPos;
    if (!cutsceneCamera && g_visualCameraHeightValid) {
        const ULONGLONG now = GetTickCount64();
        if (now >= g_visualCameraHeightSampleTick &&
            now - g_visualCameraHeightSampleTick <=
                kVisualCameraHeightFreshMilliseconds) {
            visualBasePosition = g_visualCameraPosition;
        }
    }
    if (cutsceneCamera && !cinematicCamera) {
        // PlayerCamera::UpdateFirstPerson/UpdateFollow calls CalcNonClipPos
        // after storing the requested camera position in m_vPos. Collision
        // correction can alternate between nearby wall-safe positions and
        // becomes visible as cutscene wobble. Render VR from the requested
        // pre-collision position while leaving the engine camera, player
        // physics and every gameplay collision untouched.
        LTVector desiredPosition;
        if (ReadRetailDesiredCameraPosition(desiredPosition)) {
            // Blend the collision correction out instead of switching bases
            // in one frame. Once complete, no collision result remains in the
            // rendered VR position, so alternating ray hits cannot jitter it.
            visualBasePosition =
                originalTransform.m_vPos +
                (desiredPosition - originalTransform.m_vPos) *
                    cutsceneBlend;
        }
    }

    g_stereoStep = "get_camera_fov";
    float originalFovX = 0.0F;
    float originalFovY = 0.0F;
    g_client->GetCameraFOV(
        camera, &originalFovX, &originalFovY);
    if (!std::isfinite(originalFovX) ||
        !std::isfinite(originalFovY)) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoRecovery.transform = originalTransform;
    g_stereoRecovery.fovX = originalFovX;
    g_stereoRecovery.fovY = originalFovY;
    g_stereoRecovery.valid = true;

    LTRotation viewBaseRotation = originalTransform.m_rRot;
    if (cutsceneCamera) {
        // A cinematic camera owns a scripted orientation containing authored
        // shake, pitch and roll. In a headset those components fight the
        // physical head pose and appear as camera wobble. Preserve only the
        // scene's yaw so camera cuts and horizontal staging still work; pitch
        // and roll remain exclusively under HMD control.
        viewBaseRotation = ResolveCutsceneCameraBaseRotation(
            originalTransform.m_rRot, cutsceneBlend);
    } else if (
        g_twoHandedGripActive &&
        g_weaponAim.trackingBaseValid &&
        playingFrameFresh) {
        // CPlayerCamera's rendered object includes transient HeadBobMgr and
        // recoil rotations. CClientWeaponMgr receives the clean permanent
        // camera rotation instead, which is also the basis used to transform
        // both tracked controllers. Rendering the eyes from a different,
        // sinusoidally tilted basis makes an otherwise fixed two-handed weapon
        // appear to rise and fall while strafing or firing. Keep the shared
        // gameplay yaw/pitch basis here; the full physical HMD rotation is
        // multiplied in below for each eye.
        viewBaseRotation = g_weaponAim.trackingBase.m_rRot;
        viewBaseRotation.Normalize();
        if (InterlockedCompareExchange(
                &g_twoHandedViewRotationStabilizedLogged, 1, 0) == 0) {
            Report(
                "INFO", "two_handed_view_rotation_stabilized",
                "Two-handed eyes, controllers and weapon share the clean "
                "Retail gameplay rotation; strafe bob and recoil no longer "
                "tilt the VR view around the weapon.");
        }
    } else {
        ResolveSlideKickViewBase(
            originalTransform.m_rRot, viewBaseRotation);
        // Retail's camera stores persistent mouse/gamepad pitch underneath the
        // render object. In VR that rotation would be multiplied by the HMD
        // and cannot be removed by an HMD recenter. Gameplay therefore keeps
        // only Retail yaw (body/smooth turn); physical pitch and roll belong
        // exclusively to the headset.
        LTRotation yawOnlyRotation;
        if (ResolveYawOnlyRotation(
                viewBaseRotation, yawOnlyRotation)) {
            viewBaseRotation = yawOnlyRotation;
        }
    }
    const LTRigidTransform stableCameraBase(
        visualBasePosition, viewBaseRotation);
    LTVector bodyPresentationWorldOffset(
        0.0F, 0.0F, 0.0F);

    // The common center delta is world-limited. Both eye poses contain it as
    // the same term, so the blocked part can be removed from each eye here.
    const bool roomScaleTranslation =
        (request.flags & FEARVR_RF_TRANSLATION_ON) != 0;
    const float maximumTranslationMeters = roomScaleTranslation
        ? kRoomScaleMaxTranslationMeters
        : kPhysicalLeanMaxTranslationMeters;
    g_stereoStep = "limit_head_translation";
    if (cutsceneCamera) {
        // A scripted camera already owns the viewer's world-space position.
        // Keeping the common HMD center translation here moves the eyes away
        // from a helicopter seat or other authored anchor, eventually behind
        // the player body. Remove only that shared translation; the
        // per-eye IPD and full HMD rotation remain active.
        const TrackingVector rawDelta =
            HeadTranslationRelativeToRecenter(
                g_headTracking.recenter,
                g_headTracking.currentCenter,
                maximumTranslationMeters,
                g_headTracking.verticalTranslationOverride,
                g_headTracking.verticalTranslationMeters);
        for (RelativeEyePose& tracked : trackedEye) {
            tracked.positionMeters.x -= rawDelta.x * cutsceneBlend;
            tracked.positionMeters.y -= rawDelta.y * cutsceneBlend;
            tracked.positionMeters.z -= rawDelta.z * cutsceneBlend;
        }
        g_leanViewOffsetUnits = LTVector(0.0F, 0.0F, 0.0F);
    } else if (roomScaleTranslation) {
        // Room-scale is direct physical movement: keep it 1:1 and do not
        // apply the artificial lean-strength multiplier. The render body
        // follows the permitted horizontal projection while the world ray
        // prevents the camera from crossing nearby solid geometry.
        const TrackingVector rawDelta =
            HeadTranslationRelativeToRecenter(
                g_headTracking.recenter,
                g_headTracking.currentCenter,
                kRoomScaleMaxTranslationMeters,
                g_headTracking.verticalTranslationOverride,
                g_headTracking.verticalTranslationMeters);
        const LTVector rawUnits(
            rawDelta.x * kGameUnitsPerMeter,
            rawDelta.y * kGameUnitsPerMeter,
            rawDelta.z * kGameUnitsPerMeter);
        const LTVector horizontalUnits(
            rawUnits.x, 0.0F, rawUnits.z);
        const LTVector worldOffset =
            viewBaseRotation.RotateVector(horizontalUnits);
        g_leanTranslationScale = UpdateLeanTranslationScale(
            originalTransform.m_vPos, worldOffset, true);
        g_leanViewOffsetUnits = LTVector(
            rawUnits.x * g_leanTranslationScale,
            rawUnits.y,
            rawUnits.z * g_leanTranslationScale);
        bodyPresentationWorldOffset =
            viewBaseRotation.RotateVector(
                LTVector(
                    g_leanViewOffsetUnits.x, 0.0F,
                    g_leanViewOffsetUnits.z));

        // The prepared eye poses contain the unblocked center delta. Replace
        // only that shared term, preserving per-eye IPD and full HMD rotation.
        for (RelativeEyePose& tracked : trackedEye) {
            tracked.positionMeters.x -=
                rawDelta.x * (1.0F - g_leanTranslationScale);
            tracked.positionMeters.z -=
                rawDelta.z * (1.0F - g_leanTranslationScale);
        }
    } else if (g_physicalLeanEnabled) {
        // `rawDelta` steckt so in den Augenposen und ist deshalb die Groesse,
        // die unten wieder herausgerechnet wird. Gewollt ist dagegen der
        // verstaerkte Versatz fuer Blickpunkt, Haende und Waffe.
        const TrackingVector rawDelta =
            HeadTranslationRelativeToRecenter(
                g_headTracking.recenter, g_headTracking.currentCenter,
                kPhysicalLeanMaxTranslationMeters,
                g_headTracking.verticalTranslationOverride,
                g_headTracking.verticalTranslationMeters);
        const float leanScale =
            static_cast<float>(std::clamp(g_leanScalePercent, 100, 400)) *
            0.01F;
        const TrackingVector headDelta{
            rawDelta.x * leanScale,
            rawDelta.y,
            rawDelta.z * leanScale};
        // Der gesamte erlaubte Kopfversatz bleibt relativ zur unveraenderten
        // Retail-Spielerposition. Es werden keine Bewegungsachsen erzeugt:
        // Dadurch existieren weder ein nachlaufender Koerper noch ein
        // Rueckweg, der die Kamera hinter das Koerpermodell setzen koennte.
        TrackingVector remaining = headDelta;
        const LTVector remainingUnits(
            remaining.x * kGameUnitsPerMeter,
            remaining.y * kGameUnitsPerMeter,
            remaining.z * kGameUnitsPerMeter);
        const LTVector horizontalUnits(
            remainingUnits.x, 0.0F, remainingUnits.z);
        const LTVector worldOffset =
            viewBaseRotation.RotateVector(horizontalUnits);
        g_leanTranslationScale = UpdateLeanTranslationScale(
            originalTransform.m_vPos, worldOffset, true);

        g_leanViewOffsetUnits = LTVector(
            remaining.x * g_leanTranslationScale * kGameUnitsPerMeter,
            remaining.y * kGameUnitsPerMeter,
            remaining.z * g_leanTranslationScale * kGameUnitsPerMeter);
        bodyPresentationWorldOffset =
            viewBaseRotation.RotateVector(
                LTVector(
                    g_leanViewOffsetUnits.x, 0.0F,
                    g_leanViewOffsetUnits.z));

        // Beide Augenposen tragen den rohen Kopfversatz als denselben
        // Summanden. Ersetzt wird er durch den verstaerkten und an der Welt
        // begrenzten Lean-Versatz.
        for (RelativeEyePose& tracked : trackedEye) {
            tracked.positionMeters.x -=
                rawDelta.x - remaining.x * g_leanTranslationScale;
            tracked.positionMeters.y -=
                rawDelta.y - remaining.y;
            tracked.positionMeters.z -=
                rawDelta.z - remaining.z * g_leanTranslationScale;
        }
    } else {
        g_leanViewOffsetUnits = LTVector(0.0F, 0.0F, 0.0F);
        ResetLeanCollision(g_leanCollision);
        g_leanTranslationScale = 1.0F;
    }
    if (!cutsceneCamera) {
        // Im normalen Spiel folgt der sichtbare Körper dem Yaw des Spielers,
        // nicht HMD-Pitch oder -Roll. So bleibt er beim Hoch-/Runterschauen
        // ruhig unter dem Kopf und Brust/Kragen verlassen den Komfortkegel.
        LTVector planarForward = viewBaseRotation.Forward();
        planarForward.y = 0.0F;
        if (planarForward.MagSqr() > 0.0001F) {
            planarForward.Normalize();
            bodyPresentationWorldOffset -=
                planarForward * kPlayerBodyRearwardOffsetUnits;
        }
        bodyPresentationWorldOffset.y -=
            kPlayerBodyDownwardOffsetUnits;
    }
    RebaseCutsceneWeaponPresentation(stableCameraBase);
    StageBodyPresentation(
        originalTransform, stableCameraBase,
        bodyPresentationWorldOffset);

    const float stereoFovX = symmetric.halfHorizontal * 2.0F;
    const float stereoFovY = symmetric.halfVertical * 2.0F;
    LTRESULT eyeResult[FEARVR_EYE_COUNT]{LT_ERROR, LT_ERROR};
    std::uint32_t renderedEyes = 0;

    StereoRenderGuard guard;
    for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
        g_stereoStep =
            eye == FEARVR_EYE_LEFT
                ? "set_left_transform"
                : "set_right_transform";
        LTRigidTransform eyeTransform = originalTransform;
        eyeTransform.m_vPos = visualBasePosition;
        const RelativeEyePose& tracked = trackedEye[eye];
        const LTVector localOffset(
            tracked.positionMeters.x * kGameUnitsPerMeter,
            tracked.positionMeters.y * kGameUnitsPerMeter,
            tracked.positionMeters.z * kGameUnitsPerMeter);
        eyeTransform.m_vPos +=
            viewBaseRotation.RotateVector(localOffset);
        const LTRotation headRotation(
            tracked.rotation.x, tracked.rotation.y,
            tracked.rotation.z, tracked.rotation.w);
        eyeTransform.m_rRot =
            viewBaseRotation * headRotation;
        if (g_client->SetObjectTransform(
                camera, eyeTransform) != LT_OK) {
            break;
        }

        g_stereoStep =
            eye == FEARVR_EYE_LEFT ? "set_left_fov" : "set_right_fov";
        g_client->SetCameraFOV(camera, stereoFovX, stereoFovY);
        g_stereoStep =
            eye == FEARVR_EYE_LEFT
                ? "begin_left_eye"
                : "begin_right_eye";
        g_beginEye(eye);
        g_stereoStep =
            eye == FEARVR_EYE_LEFT
                ? "clear_left_target"
                : "clear_right_target";
        if (renderer->ClearRenderTarget(
                CLEARRTARGET_ALL, 0) != LT_OK) {
            // CaptureEye also restores a supersampled D3D9 render target.
            g_captureEye(eye);
            break;
        }
        g_stereoStep =
            eye == FEARVR_EYE_LEFT
                ? "render_left_eye"
                : "render_right_eye";
        eyeResult[eye] = g_renderCameraWithOverride(
            renderer, camera, nullptr);
        if (eyeResult[eye] == LT_OK) {
            g_stereoStep =
                eye == FEARVR_EYE_LEFT
                    ? "render_left_muzzle_flash"
                    : "render_right_muzzle_flash";
            RenderVrMuzzleFlash(camera);
            g_stereoStep =
                eye == FEARVR_EYE_LEFT
                    ? "render_left_aim_guide"
                    : "render_right_aim_guide";
            RenderWeaponCollisionBox(camera);
            RenderWeaponAimGuide(camera);
            g_stereoStep =
                eye == FEARVR_EYE_LEFT
                    ? "render_left_wrist_hud"
                    : "render_right_wrist_hud";
            RenderWristHud(camera);
            g_stereoStep =
                eye == FEARVR_EYE_LEFT
                    ? "render_left_dev_menu"
                    : "render_right_dev_menu";
            RenderFloatingDevMenu(camera);
            g_stereoStep =
                eye == FEARVR_EYE_LEFT
                    ? "capture_left_eye"
                    : "capture_right_eye";
        }
        // Always leave the eye scope. EndStereoFrame receives frame 0 below
        // when rendering failed, so a restored but incomplete eye pair can
        // never be published.
        g_captureEye(eye);
        ++renderedEyes;
    }

    g_bodyPresentationOffsetActive = false;
    g_stereoStep = "restore_camera_transform";
    g_client->SetObjectTransform(camera, originalTransform);
    g_stereoStep = "restore_camera_fov";
    g_client->SetCameraFOV(camera, originalFovX, originalFovY);
    g_stereoRecovery.valid = false;
    g_stereoStep = "end_stereo_frame";
    const bool stereoComplete =
        renderedEyes == FEARVR_EYE_COUNT &&
        eyeResult[FEARVR_EYE_LEFT] == LT_OK &&
        eyeResult[FEARVR_EYE_RIGHT] == LT_OK;
    FearVrGameCameraSample gameCamera{};
    gameCamera.frameId = request.frameId;
    gameCamera.pose.px =
        visualBasePosition.x / kGameUnitsPerMeter;
    gameCamera.pose.py =
        visualBasePosition.y / kGameUnitsPerMeter;
    gameCamera.pose.pz =
        visualBasePosition.z / kGameUnitsPerMeter;
    gameCamera.pose.qx =
        viewBaseRotation.m_Quat[LTRotation::QX];
    gameCamera.pose.qy =
        viewBaseRotation.m_Quat[LTRotation::QY];
    gameCamera.pose.qz =
        viewBaseRotation.m_Quat[LTRotation::QZ];
    gameCamera.pose.qw =
        viewBaseRotation.m_Quat[LTRotation::QW];
    gameCamera.flags = FEARVR_GCF_VALID;
    g_endStereoFrame(
        stereoComplete ? request.frameId : 0,
        stereoComplete ? &gameCamera : nullptr);

    if (!stereoComplete) {
        if (InterlockedCompareExchange(
                &g_stereoFallbackLogged, 1, 0) == 0) {
            Report(
                "WARN", "stereo_render_fallback",
                "An eye render failed; restored camera and rendered mono.");
        }
        g_stereoStep = "clear_mono_fallback";
        renderer->ClearRenderTarget(CLEARRTARGET_ALL, 0);
        g_stereoStep = "render_mono_fallback";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_lastStereoRenderRequestId = request.frameId;
    if (InterlockedCompareExchange(
            &g_firstStereoFrameLogged, 1, 0) == 0) {
        Report(
            "INFO", "stereo_render_active",
            "Player camera world render is running once per eye.");
    }
    g_stereoStep = "complete";
    return eyeResult[renderedEyes - 1];
}

LTRESULT InvokeStereoProtected(
    ILTRenderer* renderer, HLOCALOBJ camera,
    const char* techniqueOverride) {
    if (g_renderCameraWithOverride == nullptr) {
        return LT_ERROR;
    }
    if (InterlockedCompareExchange(
            &g_firstHookCallLogged, 1, 0) == 0) {
        Report(
            "INFO", "rendercamera_hook_called",
            "The confirmed RenderCamera path entered native stereo.");
    }

    volatile DWORD exceptionCode = 0;
    __try {
        return RenderStereo(
            renderer, camera, techniqueOverride);
    } __except (
        exceptionCode = GetExceptionCode(),
        EXCEPTION_EXECUTE_HANDLER) {
    }

    const char* const failedStep = g_stereoStep;
    g_inStereoRender = false;
    if (g_stereoRecovery.valid && g_client != nullptr &&
        camera != nullptr) {
        __try {
            g_bodyPresentationOffsetActive = false;
            g_client->SetObjectTransform(
                camera, g_stereoRecovery.transform);
            g_client->SetCameraFOV(
                camera, g_stereoRecovery.fovX,
                g_stereoRecovery.fovY);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    g_bodyPresentationOffsetActive = false;
    g_stereoRecovery.valid = false;
    if (g_endStereoFrame != nullptr) {
        __try {
            // Besides invalidating the partial pair, this restores any
            // supersampled D3D9 eye target left active by the exception.
            g_endStereoFrame(0, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    char message[192]{};
    std::snprintf(
        message, sizeof(message),
        "Structured exception 0x%08lX at %s; camera restored and mono resumed.",
        static_cast<unsigned long>(exceptionCode), failedStep);
    Report("ERROR", "stereo_render_exception", message);
    g_stereoStep = "mono_after_exception";
    return g_renderCameraWithOverride(
        renderer, camera, techniqueOverride);
}

// Verwirft alle zwischengespeicherten Weltobjekt- und Node-Handles, ohne die
// Engine damit noch einmal aufzurufen. Nach einem Levelwechsel oder einer
// Sequenzuebernahme sind sie zerstoert; jeder weitere Zugriff schriebe in
// fremden Speicher. Alles wird auf dem naechsten gueltigen Frame neu
// eingerichtet.
void ForgetWorldObjectsAfterLevelChange() noexcept {
    const bool hadState =
        g_playerCameraObject != nullptr ||
        g_playerBodyObject != nullptr ||
        g_flashlightModel != nullptr ||
        g_flashlightLight != nullptr ||
        g_vrMuzzleFlash[kVrMuzzleFlashRight].flare != nullptr ||
        g_vrMuzzleFlash[kVrMuzzleFlashRight].light != nullptr ||
        g_vrMuzzleFlash[kVrMuzzleFlashLeft].flare != nullptr ||
        g_vrMuzzleFlash[kVrMuzzleFlashLeft].light != nullptr ||
        g_rightHandControl.installed || g_leftHandControl.installed ||
        g_bodyPresentationNodeControl.installed;

    g_preRenderCameraOverridePending = false;
    g_preRenderCameraOverrideObject = nullptr;
    g_flashlightModel = nullptr;
    g_flashlightLight = nullptr;
    g_flashlightModelWeapon = nullptr;
    for (VrMuzzleFlashState& flash : g_vrMuzzleFlash) {
        flash = VrMuzzleFlashState{};
    }
    g_playerCameraObject = nullptr;
    ResetVerticalCameraHeight(g_verticalCameraHeight);
    ResetTwoHandedJumpCamera(g_twoHandedJumpCamera);
    g_twoHandedCameraPosition = TwoHandedCameraPositionState{};
    g_visualCameraPosition = LTVector(0.0F, 0.0F, 0.0F);
    g_visualCameraHeightSampleTick = 0;
    g_visualCameraHeightValid = false;
    g_physicalDuckActive = false;
    g_retailUnsupportedSince = 0;
    g_retailSupportedSince = 0;
    g_retailPersistentUnsupported = false;
    g_cutsceneCameraStateKnown = false;
    g_cutsceneCameraState = false;
    g_cutsceneCameraActivationTick = 0;
    g_cutsceneBodyPresentation = {};
    g_playerBodyObject = nullptr;
    g_rightHandControl = HandNodeControlState{};
    g_leftHandControl = HandNodeControlState{};
    g_bodyPresentationNodeControl = BodyPresentationNodeControlState{};
    g_bodyPresentationWorldOffset = LTVector(0.0F, 0.0F, 0.0F);
    g_bodyPresentationOffsetActive = false;
    g_weaponAim = WeaponAimState{};
    g_aimPoseCache[FEARVR_HAND_RIGHT] = TrackedPoseCache{};
    g_aimPoseCache[FEARVR_HAND_LEFT] = TrackedPoseCache{};
    g_gripPoseCache[FEARVR_HAND_RIGHT] = TrackedPoseCache{};
    g_gripPoseCache[FEARVR_HAND_LEFT] = TrackedPoseCache{};
    g_supportRightGripPoseCache = TrackedPoseCache{};
    g_twoHandedGripActive = false;
    g_twoHandedGrip = TwoHandedGripState{};
    g_weaponDisabled = false;
    g_retailVisibilityInitializedWeapon = nullptr;
    g_retailWeaponUpdateInProgress = nullptr;
    g_weightedWeaponInput = {};
    ResetWeaponWeightPair(
        g_weightedWeaponInput.filters,
        WeaponWeightResetReason::sceneLoaded);
    InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
    g_lastWeaponRecoilTick = 0;

    if (hadState) {
        Report(
            "INFO", "world_objects_forgotten",
            "Level change or cutscene detected; cached body, hand-node and "
            "flashlight handles were dropped before reuse.");
    }
}

void RestorePreRenderCameraOverride() noexcept {
    if (!g_preRenderCameraOverridePending) {
        return;
    }
    // Zuerst den Zustand loeschen, damit ein Fehlschlag den Override nicht
    // dauerhaft offen laesst.
    HLOCALOBJ const target = g_preRenderCameraOverrideObject;
    g_preRenderCameraOverridePending = false;
    g_preRenderCameraOverrideObject = nullptr;
    if (target == nullptr || g_client == nullptr) {
        return;
    }
    // Nur das entfuehrte Objekt zuruecksetzen. Zeigt g_playerCameraObject
    // inzwischen woanders hin, hat eine Zwischensequenz die Kamera getauscht.
    __try {
        g_client->SetObjectTransform(target, g_preRenderCameraRecovery);
        g_client->SetCameraFOV(
            target, g_preRenderCameraRecoveryFovX,
            g_preRenderCameraRecoveryFovY);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void StagePreRenderCameraOverride() noexcept {
    if (g_preRenderCameraOverridePending || g_client == nullptr ||
        g_playerCameraObject == nullptr ||
        g_getRenderRequest == nullptr ||
        g_isStereoEnabled == nullptr ||
        g_isFlatPanelActive == nullptr ||
        !g_headTracking.centered || g_headTracking.trackingLost ||
        g_cutsceneCameraState ||
        g_lastWeaponManagerUpdateTick == 0) {
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (now - g_headTracking.lastFreshFrameTick > 250 ||
        now - g_lastWeaponManagerUpdateTick >
            kPlayingFrameFreshMilliseconds) {
        return;
    }

    __try {
        if (g_isStereoEnabled() == FALSE ||
            g_isFlatPanelActive() != FALSE) {
            return;
        }

        FearVrRenderRequest request{};
        if (!g_getRenderRequest(&request) ||
            (request.flags & FEARVR_RF_VALID) == 0 ||
            (request.flags & FEARVR_RF_FLATSCREEN) != 0) {
            return;
        }
        LTRigidTransform retailCamera;
        if (g_client->GetObjectTransform(
                g_playerCameraObject, &retailCamera) != LT_OK) {
            return;
        }
        float retailFovX = 0.0F;
        float retailFovY = 0.0F;
        g_client->GetCameraFOV(
            g_playerCameraObject, &retailFovX, &retailFovY);
        if (!std::isfinite(retailFovX) ||
            !std::isfinite(retailFovY)) {
            return;
        }

        const RelativeEyePose center =
            TrackedPoseRelativeToRecenter(
                g_headTracking.recenter,
                g_headTracking.currentCenter);
        if (!center.valid) {
            return;
        }
        const bool translationEnabled =
            (request.flags & FEARVR_RF_TRANSLATION_ON) != 0 ||
            g_physicalLeanEnabled;
        LTRigidTransform renderTargetCamera = retailCamera;
        if (g_visualCameraHeightValid &&
            now >= g_visualCameraHeightSampleTick &&
            now - g_visualCameraHeightSampleTick <=
                kVisualCameraHeightFreshMilliseconds) {
            renderTargetCamera.m_vPos = g_visualCameraPosition;
        }
        if (translationEnabled) {
            renderTargetCamera.m_vPos.y +=
                center.positionMeters.y * kGameUnitsPerMeter;
        }

        // The game updates planar mirror/refraction textures immediately
        // before its main camera call. Correct only the center-eye height that
        // those targets need. Changing X/Z, orientation or FOV over the whole
        // pre-render window also changes Retail's portal/occlusion decisions
        // and can leave wall sectors invisible. Restore Retail's object before
        // the stereo hook builds the two real eye cameras.
        g_preRenderCameraRecovery = retailCamera;
        g_preRenderCameraRecoveryFovX = retailFovX;
        g_preRenderCameraRecoveryFovY = retailFovY;
        g_preRenderCameraOverrideObject = g_playerCameraObject;
        g_preRenderCameraOverridePending = true;
        if (g_client->SetObjectTransform(
                g_playerCameraObject, renderTargetCamera) != LT_OK) {
            g_preRenderCameraOverridePending = false;
            g_preRenderCameraOverrideObject = nullptr;
            return;
        }
        if (InterlockedCompareExchange(
                &g_renderTargetCameraStabilizedLogged, 1, 0) == 0) {
            Report(
                "INFO", "render_target_camera_stabilized",
                "Water mirror/refraction render targets use the current "
                "center-eye height without modifying Retail portal culling, "
                "camera orientation or FOV.");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        RestorePreRenderCameraOverride();
    }
}

void RemoveFlashlightModel() noexcept;
void RemoveVrMuzzleFlashObjects() noexcept;

bool ResolveHeadFlashlightPose(LTRigidTransform& pose) noexcept {
    if (!g_headTracking.centered || g_headTracking.trackingLost) {
        return false;
    }
    const RelativeEyePose tracked =
        TrackedPoseRelativeToRecenter(
            g_headTracking.recenter,
            g_headTracking.currentCenter);
    if (!tracked.valid) {
        return false;
    }
    const LTRotation localRotation(
        tracked.rotation.x, tracked.rotation.y,
        tracked.rotation.z, tracked.rotation.w);
    pose.m_vPos =
        g_weaponAim.trackingBase.m_vPos +
        g_weaponAim.trackingBase.m_rRot.RotateVector(
            g_leanViewOffsetUnits);
    pose.m_rRot =
        g_weaponAim.trackingBase.m_rRot * localRotation;
    return true;
}

bool ResolveFlashlightPoses(
    const void* activeWeapon, LTRigidTransform& modelPose,
    LTRigidTransform& lightPose, bool& showModel) noexcept {
    LTRigidTransform mountPose;
    switch (g_flashlightMount) {
    case FlashlightMount::LeftHand:
        if (!g_weaponAim.leftGripValid ||
            !g_weaponAim.leftAimValid) {
            return false;
        }
        mountPose = LTRigidTransform(
            EffectiveLeftHandPosition() +
                EffectiveFlashlightRotation().Forward() * 2.0F,
            EffectiveFlashlightRotation());
        showModel = true;
        break;
    case FlashlightMount::Head:
        if (!ResolveHeadFlashlightPose(mountPose)) {
            return false;
        }
        showModel = false;
        break;
    case FlashlightMount::Weapon: {
        if (activeWeapon == nullptr || g_weaponDisabled ||
            !g_weaponAim.valid) {
            return false;
        }
        LTRigidTransform muzzlePose;
        ResolveMuzzleWorldTransform(muzzlePose);
        LTVector right;
        LTVector up;
        LTVector forward;
        g_weaponAim.fireTransform.m_rRot.GetVectors(
            right, up, forward);
        mountPose = LTRigidTransform(
            muzzlePose.m_vPos - up * 5.0F - forward * 10.0F,
            g_weaponAim.fireTransform.m_rRot);
        lightPose = LTRigidTransform(
            muzzlePose.m_vPos - up * 3.0F + forward * 4.0F,
            g_weaponAim.fireTransform.m_rRot);
        modelPose = mountPose;
        showModel = true;
        return true;
    }
    default:
        return false;
    }

    LTVector right;
    LTVector up;
    LTVector forward;
    mountPose.m_rRot.GetVectors(right, up, forward);
    modelPose = mountPose;
    if (g_flashlightMount == FlashlightMount::Head) {
        // Der Projektor sitzt oberhalb und vor der Augenmitte. Objekt-Schatten
        // werden fuer diesen Mount weiter unten deaktiviert; so koennen Arme,
        // Waffe oder Player-Body den kopffesten Lichtkegel nicht verdecken.
        // Weltgeometrie wirft weiterhin Schatten.
        lightPose = LTRigidTransform(
            mountPose.m_vPos + up * 6.0F + forward * 12.0F,
            mountPose.m_rRot);
    } else {
        // Keep the hand projector beyond the hand, matching Retail's
        // flashlight offset and avoiding a large self-shadow.
        lightPose = LTRigidTransform(
            mountPose.m_vPos - right * 10.0F + forward * 13.0F,
            mountPose.m_rRot);
    }
    return true;
}

void SetFlashlightObjectVisible(
    HLOCALOBJ object, bool visible) noexcept {
    if (object == nullptr || g_client == nullptr) {
        return;
    }
    __try {
        ILTCommon* const common = g_client->Common();
        if (common != nullptr) {
            common->SetObjectFlags(
                object, OFT_Flags,
                visible ? FLAG_VISIBLE : 0, FLAG_VISIBLE);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Retails First-Person-Muendungsblitz wird an das animierte Waffenmodell
// geparentet. Unsere finale VR-Waffenpose wird erst nach Retails Update gesetzt;
// dadurch landet der Retail-FX in manchen Laufzeitpfaden hinter der Kamera oder
// am alten Hand-Socket. Dieser kleine native Ersatz wird aus demselben
// ausgelieferten Muzzle-Material aufgebaut, aber direkt an die von uns
// gemessene Laufmuendung gesetzt.
constexpr ULONGLONG kVrMuzzleFlashDurationMs = 70;
constexpr ULONGLONG kVrMuzzleFlashMinimumTriggerGapMs = 30;

void TriggerVrMuzzleFlash(bool leftWeaponFired) noexcept {
    VrMuzzleFlashState& flash = g_vrMuzzleFlash[
        leftWeaponFired ? kVrMuzzleFlashLeft : kVrMuzzleFlashRight];
    const ULONGLONG now = GetTickCount64();
    if (flash.lastTriggerTick != 0 &&
        now - flash.lastTriggerTick < kVrMuzzleFlashMinimumTriggerGapMs) {
        // GetFireVectors kann denselben Schuss aus zwei Retail-Pfaden melden.
        // Die Sichtzeit darf dabei verlaengert, aber die Animation nicht
        // sichtbar neu gestartet werden.
        flash.visibleUntilTick = (std::max)(
            flash.visibleUntilTick, now + kVrMuzzleFlashDurationMs);
        return;
    }
    flash.lastTriggerTick = now;
    flash.pulseStartedTick = now;
    flash.visibleUntilTick = now + kVrMuzzleFlashDurationMs;
    ++flash.pulseSerial;
    if (InterlockedCompareExchange(
            &g_vrMuzzleFlashActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "vr_muzzle_flash_active",
            "Confirmed shots drive a native muzzle flare and warm point light "
            "at the measured barrel end; empty trigger pulls stay dark.");
    }
}

bool ResolveVrMuzzleFlashPose(
    bool leftWeapon, LTRigidTransform& pose) noexcept {
    if (!g_weaponAim.valid || g_weaponDisabled) {
        return false;
    }
    if (leftWeapon) {
        if (!DualPistolsVrActive() ||
            !g_weaponAim.leftWeaponTransformValid) {
            return false;
        }
        return ResolveLeftMuzzleWorldTransform(pose);
    }
    return ResolveMuzzleWorldTransform(pose);
}

bool CreateVrMuzzleFlashObjects(
    VrMuzzleFlashState& flash,
    const LTRigidTransform& pose) noexcept {
    if (g_client == nullptr) {
        return false;
    }
    if (flash.light == nullptr) {
        ObjectCreateStruct create;
        create.m_ObjectType = OT_LIGHT;
        create.m_Pos = pose.m_vPos;
        create.m_Rotation = pose.m_rRot;
        flash.light = g_client->CreateObject(&create);
        if (flash.light != nullptr) {
            g_client->SetLightType(flash.light, eEngineLight_Point);
            g_client->SetLightRadius(flash.light, 150.0F);
            g_client->SetLightDetailSettings(
                flash.light,
                eEngineLOD_Low, eEngineLOD_Never, eEngineLOD_Never);
        }
    }
    if (flash.light == nullptr) {
        return false;
    }
    if (InterlockedCompareExchange(
            &g_vrMuzzleFlashCreatedLogged, 1, 0) == 0) {
        Report(
            "INFO", "vr_muzzle_flash_created",
            "The native shadowless muzzle-flash point light was created; "
            "visible stereo geometry is drawn directly per eye.");
    }
    return true;
}

void UpdateVrMuzzleFlash() noexcept {
    if (g_client == nullptr) {
        return;
    }
    const ULONGLONG now = GetTickCount64();
    for (std::size_t index = 0; index < kVrMuzzleFlashCount; ++index) {
        VrMuzzleFlashState& flash = g_vrMuzzleFlash[index];
        const bool active = flash.visibleUntilTick > now;
        LTRigidTransform muzzlePose;
        const bool leftWeapon = index == kVrMuzzleFlashLeft;
        if (!active || !ResolveVrMuzzleFlashPose(leftWeapon, muzzlePose)) {
            SetFlashlightObjectVisible(flash.flare, false);
            SetFlashlightObjectVisible(flash.light, false);
            continue;
        }

        LTVector right;
        LTVector up;
        LTVector forward;
        muzzlePose.m_rRot.GetVectors(right, up, forward);
        if (!CreateVrMuzzleFlashObjects(flash, muzzlePose)) {
            SetFlashlightObjectVisible(flash.flare, false);
            SetFlashlightObjectVisible(flash.light, false);
            continue;
        }

        const float age = static_cast<float>(
            now - flash.pulseStartedTick) /
            static_cast<float>(kVrMuzzleFlashDurationMs);
        const float life = (std::max)(0.0F, 1.0F - age);
        const LTRigidTransform lightPose(
            muzzlePose.m_vPos + forward * 3.0F,
            muzzlePose.m_rRot);
        g_client->SetObjectTransform(flash.light, lightPose);
        g_client->SetObjectColor(
            flash.light, 1.0F, 0.46F, 0.12F, life);
        g_client->SetLightIntensityScale(
            flash.light, 0.35F + 1.65F * life);
        SetFlashlightObjectVisible(flash.light, true);
    }
}

void RenderVrMuzzleFlash(HLOCALOBJ camera) noexcept {
    if (g_client == nullptr || camera == nullptr ||
        !g_weaponAim.valid || g_weaponDisabled) {
        return;
    }
    ILTDrawPrim* const drawPrim = g_client->GetDrawPrim();
    if (drawPrim == nullptr) {
        return;
    }
    LTRigidTransform eye;
    if (g_client->GetObjectTransform(camera, &eye) != LT_OK) {
        return;
    }

    const HOBJECT oldCamera = drawPrim->GetCamera();
    const ELTDrawPrimTransformMode oldTransformMode =
        drawPrim->GetTransformMode();
    const ELTDrawPrimZMode oldZMode = drawPrim->GetZMode();
    const ELTDrawPrimRenderMode oldRenderMode =
        drawPrim->GetRenderMode();
    drawPrim->SetCamera(camera);
    drawPrim->SetTransformMode(eLTDrawPrimTransformMode_World);
    drawPrim->SetZMode(eLTDrawPrimZMode_NoWrite);
    drawPrim->SetRenderMode(eLTDrawPrimRenderMode_Modulate_Additive);
    drawPrim->SetTexture(nullptr);

    const ULONGLONG now = GetTickCount64();
    bool rendered = false;
    for (std::size_t index = 0; index < kVrMuzzleFlashCount; ++index) {
        const VrMuzzleFlashState& flash = g_vrMuzzleFlash[index];
        if (flash.visibleUntilTick <= now) {
            continue;
        }
        LTRigidTransform muzzle;
        if (!ResolveVrMuzzleFlashPose(
                index == kVrMuzzleFlashLeft, muzzle)) {
            continue;
        }

        const float age = std::clamp(
            static_cast<float>(now - flash.pulseStartedTick) /
                static_cast<float>(kVrMuzzleFlashDurationMs),
            0.0F, 1.0F);
        const float life = 1.0F - age;
        const float variation =
            0.90F + 0.10F * static_cast<float>(flash.pulseSerial % 3);
        const float radius = 5.8F * variation * (0.72F + 0.28F * life);
        const LTVector center =
            muzzle.m_vPos + muzzle.m_rRot.Forward() * 1.0F;
        const LTVector cameraRight = eye.m_rRot.Right() * radius;
        const LTVector cameraUp = eye.m_rRot.Up() * radius;
        const LTVector perimeter[4] = {
            center - cameraRight + cameraUp,
            center + cameraRight + cameraUp,
            center + cameraRight - cameraUp,
            center - cameraRight - cameraUp,
        };
        LT_POLYG3 glow[4];
        const std::uint8_t coreAlpha = static_cast<std::uint8_t>(
            std::clamp(255.0F * life, 0.0F, 255.0F));
        for (std::size_t triangle = 0; triangle < 4; ++triangle) {
            glow[triangle].verts[0].pos = center;
            glow[triangle].verts[1].pos = perimeter[triangle];
            glow[triangle].verts[2].pos = perimeter[(triangle + 1) % 4];
            glow[triangle].verts[0].rgba.Init(
                255, 238, 170, coreAlpha);
            glow[triangle].verts[1].rgba.Init(255, 72, 8, 0);
            glow[triangle].verts[2].rgba.Init(255, 72, 8, 0);
        }
        drawPrim->DrawPrim(glow, 4);
        rendered = true;
    }

    drawPrim->SetRenderMode(oldRenderMode);
    drawPrim->SetZMode(oldZMode);
    drawPrim->SetTransformMode(oldTransformMode);
    drawPrim->SetCamera(oldCamera);
    if (rendered && InterlockedCompareExchange(
            &g_vrMuzzleFlashRenderedLogged, 1, 0) == 0) {
        Report(
            "INFO", "vr_muzzle_flash_stereo_rendered",
            "The muzzle flash was drawn directly into both stereo eyes at "
            "the same measured Flash socket used by bullets and aim rays.");
    }
}

void RemoveVrMuzzleFlashObjects() noexcept {
    for (VrMuzzleFlashState& flash : g_vrMuzzleFlash) {
        if (g_client != nullptr) {
            if (flash.flare != nullptr) {
                __try {
                    g_client->RemoveObject(flash.flare);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }
            if (flash.light != nullptr) {
                __try {
                    g_client->RemoveObject(flash.light);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }
        }
        flash = VrMuzzleFlashState{};
    }
}

void UpdateFlashlightModel(const void* activeWeapon) noexcept {
    if (g_disableFlashlight || g_client == nullptr) {
        return;
    }

    LTRigidTransform modelPose;
    LTRigidTransform lightPose;
    bool showModel = false;
    if (!ResolveFlashlightPoses(
            activeWeapon, modelPose, lightPose, showModel)) {
        SetFlashlightObjectVisible(g_flashlightModel, false);
        SetFlashlightObjectVisible(g_flashlightLight, false);
        return;
    }

    // Retail replaces the first-person hand/weapon presentation on a weapon
    // switch. Recreate our proxy at that boundary so it cannot remain tied to
    // an object that Retail has just hidden or discarded.
    if (g_flashlightModel != nullptr &&
        g_flashlightModelWeapon != activeWeapon) {
        RemoveFlashlightModel();
    }

    if (showModel && g_flashlightModel == nullptr) {
        ObjectCreateStruct create;
        create.m_ObjectType = OT_MODEL;
        create.m_Flags = FLAG_VISIBLE;
        create.m_Pos = modelPose.m_vPos;
        create.m_Rotation = modelPose.m_rRot;
        create.m_Scale = 1.5f;
        create.SetFileName("models/keypadlight.Model00p");
        g_flashlightModel = g_client->CreateObject(&create);
        if (g_flashlightModel != nullptr) {
            g_flashlightModelWeapon = activeWeapon;
            g_client->SetObjectColor(
                g_flashlightModel,
                1.0f, 0.82f, 0.35f, 1.0f);
            Report(
                "INFO", "flashlight_model_created",
                "Visible flashlight proxy created at the selected mount.");
        } else {
            Report(
                "WARN", "flashlight_model_failed",
                "Could not create the visible flashlight proxy.");
        }
    }
    if (g_flashlightModel != nullptr) {
        if (showModel) {
            g_client->SetObjectTransform(
                g_flashlightModel, modelPose);
        }
        SetFlashlightObjectVisible(
            g_flashlightModel, showModel);
    }

    if (g_flashlightLight == nullptr) {
        ObjectCreateStruct create;
        create.m_ObjectType = OT_LIGHT;
        create.m_Flags = FLAG_VISIBLE;
        create.m_Pos = lightPose.m_vPos;
        create.m_Rotation = lightPose.m_rRot;
        g_flashlightLight = g_client->CreateObject(&create);
        if (g_flashlightLight != nullptr) {
            g_client->SetLightType(
                g_flashlightLight, eEngineLight_SpotProjector);
            g_client->SetLightTexture(
                g_flashlightLight, "Tex\\Lights\\Headlight.dds");
            g_client->SetObjectColor(
                g_flashlightLight, 1.0f, 1.0f, 1.0f, 1.0f);
            g_client->SetLightRadius(g_flashlightLight, 1000.0f);
            g_client->SetLightSpotInfo(
                g_flashlightLight, 0.698132f, 0.698132f, 1.0f);
            g_client->SetLightIntensityScale(g_flashlightLight, 1.5f);
            SetFlashlightObjectVisible(
                g_flashlightLight, g_flashlightEnabled);
            Report(
                "INFO", "flashlight_light_created",
                "Native spot projector created at the selected mount.");
        } else {
            Report(
                "WARN", "flashlight_light_failed",
                "Could not create the native flashlight spot projector.");
        }
    }
    if (g_flashlightLight != nullptr) {
        // Nur das Headlight ignoriert Schatten dynamischer Objekte. Beim
        // Hand-/Waffenmount bleibt Retails volle Schattenwirkung erhalten;
        // beim Umschalten wird der vorherige LOD-Zustand deshalb explizit
        // zurueckgesetzt.
        const EEngineLOD objectShadowDetail =
            g_flashlightMount == FlashlightMount::Head
                ? eEngineLOD_Never
                : eEngineLOD_Low;
        g_client->SetLightDetailSettings(
            g_flashlightLight,
            eEngineLOD_Low, eEngineLOD_Low, objectShadowDetail);
        g_client->SetObjectTransform(g_flashlightLight, lightPose);
        SetFlashlightObjectVisible(
            g_flashlightLight, g_flashlightEnabled);
    }
}

void RemoveFlashlightModel() noexcept {
    if (g_flashlightModel != nullptr && g_client != nullptr) {
        __try {
            g_client->RemoveObject(g_flashlightModel);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    g_flashlightModel = nullptr;
    g_flashlightModelWeapon = nullptr;
    if (g_flashlightLight != nullptr && g_client != nullptr) {
        // Ohne SEH wie beim Modell darueber: Ein Weltwechsel kann das Objekt
        // bereits zerstoert haben.
        __try {
            g_client->RemoveObject(g_flashlightLight);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    g_flashlightLight = nullptr;
}

LTRESULT __fastcall HookRenderPlayerCamera(
    ILTRenderer* renderer, void* ignoredEdx, HLOCALOBJ camera) {
    (void)ignoredEdx;
    RestorePreRenderCameraOverride();
    g_playerCameraObject = camera;
    if (InterlockedCompareExchange(
            &g_playerHookCallLogged, 1, 0) == 0) {
        Report(
            "INFO", "player_rendercamera_called",
            "Retail RenderCamera slot 17 entered native stereo.");
    }
    return InvokeStereoProtected(renderer, camera, nullptr);
}

bool ExchangeVtableSlot(void** slot, void* expected,
                        void* replacement) noexcept {
    if (slot == nullptr || expected == nullptr ||
        replacement == nullptr) {
        return false;
    }
    DWORD oldProtection = 0;
    if (!VirtualProtect(
            slot, sizeof(*slot), PAGE_READWRITE,
            &oldProtection)) {
        Report(
            "ERROR", "stereo_vtable_protect_failed",
            "An ILTRenderer VTable slot could not be made writable.");
        return false;
    }
    void* const observed = InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        replacement, expected);
    DWORD ignoredProtection = 0;
    const BOOL protectionRestored = VirtualProtect(
        slot, sizeof(*slot), oldProtection,
        &ignoredProtection);
    if (observed != expected) {
        Report(
            "ERROR", "stereo_vtable_target_changed",
            "An ILTRenderer VTable slot changed unexpectedly.");
        return false;
    }
    if (!protectionRestored) {
        Report(
            "ERROR", "stereo_vtable_restore_protection_failed",
            "ILTRenderer VTable protection could not be restored.");
        return false;
    }
    return true;
}

float ActiveBindingValue(
    const RetailBinding& binding) noexcept {
    if (!std::isfinite(binding.commandMin) ||
        !std::isfinite(binding.commandMax) ||
        binding.commandMin > binding.commandMax) {
        return 1.0F;
    }
    float value = 1.0F;
    if (value < binding.commandMin) {
        value = binding.commandMin;
    }
    if (value > binding.commandMax) {
        value = binding.commandMax;
    }
    return value;
}

// Die Live-Testmatrix aus anweisung.md §14 verlangt, Lean und Slow-Mo
// getrennt zu protokollieren. Beide bekommen deshalb eigene Ereignisse mit
// Haltedauer und laufender Zählung, statt nur im gemeinsamen
// controller_command_injected aufzutauchen.
struct RegressionCommandLog {
    std::uint32_t command;
    const char* engagedEvent;
    const char* releasedEvent;
    const char* label;
    bool reportLeanRoll;
    ULONGLONG engagedTick;
    std::uint32_t count;
};

RegressionCommandLog g_regressionCommands[] = {
    {FEARVR_CMD_LEAN_LEFT, "vr_lean_left_engaged",
     "vr_lean_left_released", "Lean left", true, 0, 0},
    {FEARVR_CMD_LEAN_RIGHT, "vr_lean_right_engaged",
     "vr_lean_right_released", "Lean right", true, 0, 0},
    {FEARVR_CMD_SLOWMO, "vr_slowmo_engaged",
     "vr_slowmo_released", "Slow-mo", false, 0, 0},
};

// Ein injizierter Puls muss laenger stehen als ein einzelnes Update, sonst
// verpasst ihn der Retail-Bindungspfad je nach Bildrate.
constexpr ULONGLONG kCommandPulseMs = 100;


// Loest den globalen CMoveMgr-Zeiger ausschliesslich nach mehreren
// gegenseitigen Proben auf. Jede Probe kommt aus einer anderen Retail-
// Funktion:
//
// - PlayerLeashFn laedt denselben globalen Zeiger zweimal.
// - eine Fallabfrage liest m_bFalling bei +0x66;
// - die Genauigkeitsauswertung liest m_bOnGround bei +0x64;
// - PlayerBodyMgr prueft m_bJumped/+0x78 und m_bSwimJumped/+0x79;
// - UpdateControlFlags startet bei der DUCK-Flanke den Timer bei +0x4b0 mit
//   dem Wert aus g_vtPostureDownTime.
//
// Weicht ein Byte ab, bleibt der Zustand unverfuegbar. Damit koennen spaetere
// Kick-Meilensteine auf einer unbekannten GameOrig-Fassung nicht versehentlich
// mit geratenen Feldern arbeiten.
const void* const* ResolveRetailMoveManagerSlot() noexcept {
    static const void* const* resolved = nullptr;
    static bool attempted = false;
    if (attempted) {
        return resolved;
    }
    attempted = true;

    HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return nullptr;
    }
    auto* const base = reinterpret_cast<unsigned char*>(module);
    __try {
        const unsigned char* const pointerProbe =
            base + kRetailMoveManagerPointerProbeRva;
        constexpr unsigned char kPointerPrefix[] = {
            0x83, 0x7C, 0x24, 0x04, 0x02, 0x74, 0x33, 0x8B, 0x0D};
        constexpr unsigned char kPointerMiddle[] = {
            0x33, 0xC0, 0x89, 0x81, 0x4C, 0x04, 0x00, 0x00,
            0x8B, 0x15};
        if (!MatchesCode(
                pointerProbe, kPointerPrefix,
                sizeof(kPointerPrefix)) ||
            !MatchesCode(
                pointerProbe + 13, kPointerMiddle,
                sizeof(kPointerMiddle))) {
            Report(
                "WARN", "melee_movement_pattern_mismatch",
                "CMoveMgr's PlayerLeash pointer probe did not match Retail "
                "1.08; kick state diagnostics stay disabled.");
            return nullptr;
        }
        const auto* const firstEncoded =
            *reinterpret_cast<unsigned char* const*>(
                pointerProbe + 9);
        const auto* const secondEncoded =
            *reinterpret_cast<unsigned char* const*>(
                pointerProbe + 23);
        if (firstEncoded != base + kRetailMoveManagerPointerRva ||
            secondEncoded != firstEncoded) {
            Report(
                "WARN", "melee_movement_address_mismatch",
                "CMoveMgr's two global pointer loads disagree; kick state "
                "diagnostics stay disabled.");
            return nullptr;
        }

        const unsigned char* const fallingProbe =
            base + kRetailMoveFallingProbeRva;
        constexpr unsigned char kFallingPrefix[] = {0xA1};
        constexpr unsigned char kFallingTail[] = {
            0x8A, 0x48, 0x66, 0x84, 0xC9, 0x74, 0x07,
            0x8A, 0x48, 0x30};
        const unsigned char* const onGroundProbe =
            base + kRetailMoveOnGroundProbeRva;
        constexpr unsigned char kOnGroundPrefix[] = {0x8B, 0x15};
        constexpr unsigned char kOnGroundTail[] = {0x8A, 0x5A, 0x64};
        const unsigned char* const jumpedProbe =
            base + kRetailMoveJumpedProbeRva;
        constexpr unsigned char kJumpedPrefix[] = {0xA1};
        constexpr unsigned char kJumpedTail[] = {
            0x8A, 0x48, 0x78, 0x84, 0xC9, 0x74, 0x0B,
            0x8A, 0x48, 0x79, 0x84, 0xC9};
        if (!MatchesCode(
                fallingProbe, kFallingPrefix,
                sizeof(kFallingPrefix)) ||
            !MatchesCode(
                fallingProbe + 5, kFallingTail,
                sizeof(kFallingTail)) ||
            !MatchesCode(
                onGroundProbe, kOnGroundPrefix,
                sizeof(kOnGroundPrefix)) ||
            !MatchesCode(
                onGroundProbe + 6, kOnGroundTail,
                sizeof(kOnGroundTail)) ||
            !MatchesCode(
                jumpedProbe, kJumpedPrefix,
                sizeof(kJumpedPrefix)) ||
            !MatchesCode(
                jumpedProbe + 5, kJumpedTail,
                sizeof(kJumpedTail)) ||
            *reinterpret_cast<unsigned char* const*>(
                fallingProbe + 1) != firstEncoded ||
            *reinterpret_cast<unsigned char* const*>(
                onGroundProbe + 2) != firstEncoded ||
            *reinterpret_cast<unsigned char* const*>(
                jumpedProbe + 1) != firstEncoded) {
            Report(
                "WARN", "melee_movement_field_mismatch",
                "CMoveMgr airborne field probes did not match Retail 1.08; "
                "kick state diagnostics stay disabled.");
            return nullptr;
        }

        const unsigned char* const postureProbe =
            base + kRetailPostureDownProbeRva;
        constexpr unsigned char kPosturePrefix[] = {
            0xF6, 0x46, 0x28, 0x20, 0x74, 0x29,
            0xF6, 0x46, 0x2C, 0x20, 0x75, 0x1D,
            0x6A, 0x00, 0xB9};
        constexpr unsigned char kPostureTimer[] = {
            0x8D, 0x8E, 0xB0, 0x04, 0x00, 0x00};
        if (!MatchesCode(
                postureProbe, kPosturePrefix,
                sizeof(kPosturePrefix)) ||
            !MatchesCode(
                postureProbe + 27, kPostureTimer,
                sizeof(kPostureTimer)) ||
            *reinterpret_cast<unsigned char* const*>(
                postureProbe + 15) !=
                base + kRetailPostureDownVarTrackRva) {
            Report(
                "WARN", "melee_posture_pattern_mismatch",
                "CMoveMgr's PostureDownTime edge sequence did not match "
                "Retail 1.08; kick state diagnostics stay disabled.");
            return nullptr;
        }

        resolved = reinterpret_cast<const void* const*>(
            base + kRetailMoveManagerPointerRva);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        resolved = nullptr;
        Report(
            "WARN", "melee_movement_probe_unreadable",
            "CMoveMgr's Retail probes were not readable; kick state "
            "diagnostics stay disabled.");
    }
    if (resolved != nullptr) {
        Report(
            "INFO", "melee_movement_resolved",
            "Retail CMoveMgr and its airborne/posture fields were verified.");
    }
    return resolved;
}

float RetailPostureDownSeconds() noexcept {
    if (g_client == nullptr) {
        return 0.0F;
    }
    if (g_retailPostureDownVariable == nullptr) {
        g_retailPostureDownVariable =
            g_client->GetConsoleVariable("PostureDownTime");
    }
    if (g_retailPostureDownVariable == nullptr) {
        return 0.0F;
    }
    const float seconds =
        g_client->GetConsoleVariableFloat(
            g_retailPostureDownVariable);
    return std::isfinite(seconds) && seconds > 0.0F &&
                   seconds < 10.0F
        ? seconds
        : 0.0F;
}

bool ReadRetailMovement(
    RetailMovementSnapshot& snapshot) noexcept {
    const void* const* const slot = ResolveRetailMoveManagerSlot();
    if (slot == nullptr) {
        return false;
    }
    __try {
        const auto* const move =
            static_cast<const unsigned char*>(*slot);
        if (move == nullptr) {
            return false;
        }
        snapshot.controlFlags =
            *reinterpret_cast<const std::uint32_t*>(
                move + kRetailMoveControlFlagsOffset);
        snapshot.onGround =
            *(move + kRetailMoveOnGroundOffset) != 0;
        snapshot.falling =
            *(move + kRetailMoveFallingOffset) != 0;
        snapshot.jumped =
            *(move + kRetailMoveJumpedOffset) != 0;
        snapshot.airborne = snapshot.jumped || snapshot.falling;
        snapshot.postureDownSeconds =
            RetailPostureDownSeconds();
        snapshot.available = true;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Die echte DUCK-Flanke kommt aus CMoveMgr, die Fensterlaenge aus der von
// Retail initialisierten Konsolenvariable. M3 verwendet nur den echten
// Luftzustand; das nachgebildete PostureDown-Fenster bleibt bis M4 reine
// Diagnose.
void UpdateRetailMeleeDiagnostics() noexcept {
    RetailMovementSnapshot snapshot{};
    if (!ReadRetailMovement(snapshot)) {
        g_retailMovement = {};
        g_retailMovementHadSnapshot = false;
        g_retailDuckWasDown = false;
        g_retailPostureDownUntil = 0;
        g_retailUnsupportedSince = 0;
        g_retailSupportedSince = 0;
        g_retailPersistentUnsupported = false;
        return;
    }

    const ULONGLONG now = GetTickCount64();
    // The helicopter passenger sequence never enters Cinematic mode, never
    // selects PPM_LURE and never publishes a camera animation descriptor.
    // Retail does, however, leave the player falling without a jump or ground
    // contact for the entire ride. A real jump/fall is short; require this
    // state to persist before treating it as an unsupported moving carrier,
    // then use a short grounded hysteresis on exit to avoid a one-frame pop.
    const bool unsupported =
        snapshot.falling && !snapshot.jumped && !snapshot.onGround;
    if (unsupported) {
        g_retailSupportedSince = 0;
        if (g_retailUnsupportedSince == 0) {
            g_retailUnsupportedSince = now;
        }
        if (!g_retailPersistentUnsupported &&
            now >= g_retailUnsupportedSince &&
            now - g_retailUnsupportedSince >= 500) {
            g_retailPersistentUnsupported = true;
            Report(
                "INFO", "persistent_unsupported_camera_detected",
                "Retail has reported falling without a jump or ground "
                "contact for 500ms; moving-carrier camera stabilization "
                "is active.");
        }
    } else {
        g_retailUnsupportedSince = 0;
        const bool stablySupported =
            snapshot.onGround && !snapshot.falling;
        if (g_retailPersistentUnsupported && stablySupported) {
            if (g_retailSupportedSince == 0) {
                g_retailSupportedSince = now;
            }
            if (now >= g_retailSupportedSince &&
                now - g_retailSupportedSince >= 250) {
                g_retailPersistentUnsupported = false;
                g_retailSupportedSince = 0;
                Report(
                    "INFO", "persistent_unsupported_camera_cleared",
                    "Stable ground contact restored; moving-carrier camera "
                    "stabilization is inactive.");
            }
        } else {
            g_retailSupportedSince = 0;
        }
    }

    const bool duckDown =
        (snapshot.controlFlags & kRetailControlFlagDuck) != 0;
    if (duckDown && !g_retailDuckWasDown &&
        snapshot.postureDownSeconds > 0.0F) {
        const ULONGLONG durationMs = static_cast<ULONGLONG>(
            snapshot.postureDownSeconds * 1000.0F + 0.5F);
        g_retailPostureDownUntil = now + durationMs;
    }
    g_retailDuckWasDown = duckDown;
    snapshot.postureDownWindow =
        now < g_retailPostureDownUntil;

    const bool changed =
        !g_retailMovementHadSnapshot ||
        snapshot.onGround != g_retailMovement.onGround ||
        snapshot.falling != g_retailMovement.falling ||
        snapshot.jumped != g_retailMovement.jumped ||
        snapshot.postureDownWindow !=
            g_retailMovement.postureDownWindow;
    g_retailMovement = snapshot;
    g_retailMovementHadSnapshot = true;
    if (!changed) {
        return;
    }

    char message[256]{};
    std::snprintf(
        message, sizeof(message),
        "Retail movement: onGround=%u jumped=%u falling=%u airborne=%u "
        "postureDownWindow=%u PostureDownTime=%.3f s flags=0x%08X.",
        snapshot.onGround ? 1U : 0U,
        snapshot.jumped ? 1U : 0U,
        snapshot.falling ? 1U : 0U,
        snapshot.airborne ? 1U : 0U,
        snapshot.postureDownWindow ? 1U : 0U,
        static_cast<double>(snapshot.postureDownSeconds),
        snapshot.controlFlags);
    Report("INFO", "melee_retail_state", message);
}

// Waffenwechsel auf der rechten Primaertaste: jeder Druck schaltet eine Waffe
// weiter. Bewusst nur die Flanke, ohne Wiederholung beim Halten — anders als
// beim frueheren Stick-Ausschlag wuerde Dauerdruck sonst durchrattern.
void PrepareWeaponSwitchPulse() noexcept {
    const bool down =
        (g_currentInput.activeHands & FEARVR_HAND_MASK_RIGHT) != 0 &&
        (g_currentInput.buttons & FEARVR_IB_RIGHT_PRIMARY) != 0;
    const ULONGLONG now = GetTickCount64();
    if (down && !g_weaponSwitchTriggered) {
        g_weaponSwitchPulseUntil = now + kCommandPulseMs;
        Report(
            "INFO", "weapon_switch_gesture",
            "Weapon switch triggered by the right primary button.");
    }
    g_weaponSwitchTriggered = down;
}

// Rechte Sekundaer- bzw. Menütaste: kurz lädt nach, gehalten wirft eine
// Granate. Der Wurf löst bereits beim Erreichen der Haltezeit aus und nicht
// erst beim Loslassen, damit die Taste sich wie ein Auslöser anfühlt.
// Nachladen kommt erst beim Loslassen, weil vorher die Haltedauer unbekannt ist.
void PrepareGrenadeAndReloadPulse() noexcept {
    constexpr ULONGLONG kGrenadeHoldMs = 350;
    const bool down = RightSecondaryButtonDown(g_currentInput);
    const ULONGLONG now = GetTickCount64();

    if (down && !g_secondaryWasDown) {
        g_secondaryHoldStartTick = now;
        g_grenadeConsumed = false;
    } else if (down && !g_grenadeConsumed &&
               now - g_secondaryHoldStartTick >= kGrenadeHoldMs) {
        g_grenadePulseUntil = now + kCommandPulseMs;
        g_grenadeConsumed = true;
        Report(
            "INFO", "grenade_throw_gesture",
            "Grenade thrown after holding the right secondary/menu button.");
    } else if (!down && g_secondaryWasDown && !g_grenadeConsumed) {
        g_reloadPulseUntil = now + kCommandPulseMs;
        Report(
            "INFO", "reload_gesture",
            "Reload triggered by a short press of the right secondary/menu "
            "button.");
    }
    g_secondaryWasDown = down;
}

// Nahkampf per Geste. Die reine Logik in `melee_actions.h` unterscheidet den
// Waffenstoss von einem Stoss der freien Hand und teilt beiden eine Sperre zu.
// Variante A des Kampfdesigns: Auch der Off-Hand Strike fordert Retails
// Sekundaerangriff an; es gibt keinen sichtbaren Waffenwechsel.
bool RetailPlayerIsOnLadder() noexcept;

void UpdateMeleeActions() noexcept {
    if (!g_meleeThrustEnabled) {
        return;
    }
    MeleeActionInput frame{};
    const ULONGLONG now = GetTickCount64();
    frame.input = g_currentInput;
    frame.nowNs = MonotonicNanoseconds();
    frame.headHeightValid =
        g_physicalDuckEnabled &&
        g_headTracking.centered && !g_headTracking.trackingLost &&
        g_headTracking.lastFreshFrameTick != 0 &&
        now - g_headTracking.lastFreshFrameTick <= 250 &&
        std::isfinite(g_headTracking.currentCenter.py);
    frame.headHeightMeters = g_headTracking.currentCenter.py;
    frame.movementAvailable = g_retailMovement.available;
    frame.airborne =
        g_retailMovement.available &&
        g_retailMovement.airborne;
    frame.onGround =
        g_retailMovement.available &&
        g_retailMovement.onGround;
    frame.movingForward =
        g_retailMovement.available &&
        (g_retailMovement.controlFlags &
             kRetailControlFlagForward) != 0;
    frame.sprinting =
        g_retailMovement.available &&
        (g_retailMovement.controlFlags &
             kRetailControlFlagRun) != 0;
    frame.postureDownWindow =
        g_retailMovement.available &&
        g_retailMovement.postureDownWindow;
    frame.stickCrouch = MapControllerCommand(
        g_currentInput, FEARVR_CMD_DUCK,
        g_twoHandedGripActive).active;
    frame.aimingDownSights = MapControllerCommand(
        g_currentInput, FEARVR_CMD_FOCUS,
        g_twoHandedGripActive).active;
    frame.onLadder = RetailPlayerIsOnLadder();
    frame.weaponDisabled = g_weaponDisabled;
    frame.offHandHoldingWeapon = LeftHandOnWeapon();
    frame.weaponStrikeEnabled = g_meleeWeaponStrikeEnabled;
    frame.offHandStrikeEnabled = g_meleeOffHandStrikeEnabled;
    frame.jumpKickEnabled = g_meleeJumpKickEnabled;
    frame.slideKickEnabled = g_meleeSlideKickEnabled;
    const MeleeActionRequest request =
        fearvr::UpdateMeleeActions(g_meleeActions, frame);

    // Diagnose pro Hand: Damit bleibt erkennbar, ob die freie Hand keine
    // verwertbaren Posen liefert oder nur unter der Geschwindigkeitsschwelle
    // bleibt.
    if (now - g_meleePeakReportTick >= 3000) {
        g_meleePeakReportTick = now;
        const MeleeThrustDetector& weapon = g_meleeActions.weaponHand;
        const MeleeThrustDetector& offHand = g_meleeActions.offHand;
        if (weapon.evaluatedSamples == 0 ||
            offHand.evaluatedSamples == 0 ||
            weapon.peakForwardSpeed >= 0.5F ||
            offHand.peakForwardSpeed >= 0.5F) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "Forward hand speed: weapon %.2f m/s (%u samples), "
                "off-hand %.2f m/s (%u samples), threshold %.2f m/s. "
                "No samples means the pose or clock produced no velocity.",
                static_cast<double>(weapon.peakForwardSpeed),
                weapon.evaluatedSamples,
                static_cast<double>(offHand.peakForwardSpeed),
                offHand.evaluatedSamples,
                static_cast<double>(kMeleeThrustSpeedMps));
            Report("INFO", "melee_thrust_peak", message);
        }
        g_meleeActions.weaponHand.peakForwardSpeed = 0.0F;
        g_meleeActions.weaponHand.evaluatedSamples = 0;
        g_meleeActions.offHand.peakForwardSpeed = 0.0F;
        g_meleeActions.offHand.evaluatedSamples = 0;
    }

    if (request.action == MeleeAction::None) {
        return;
    }
    // Der Tritt entsteht aus einem Animationszustand, nicht aus einer
    // Tastenflanke. Der Puls steht deshalb laenger als bei Nachladen und
    // Granate, damit Retail ihn ueber mehrere Bilder als gehalten sieht.
    constexpr ULONGLONG kMeleePulseMs = 200;
    g_meleePulseUntil = now + kMeleePulseMs;
    if (request.action == MeleeAction::SlideKick) {
        BeginSlideKickViewStabilization();
        if (request.needsDuckEdge) {
            g_slideDuckPulseUntil = now + kMeleePulseMs;
        }
        if (request.needsForwardHold) {
            g_slideForwardPulseUntil = now + kMeleePulseMs;
        }
        Report(
            "INFO", "melee_slide_kick",
            request.needsDuckEdge
                ? "Slide kick triggered from a physical crouch; DUCK, "
                  "FORWARD and secondary attack are pulsed for 200 ms."
                : "Slide kick triggered inside Retail's stick-DUCK posture "
                  "window; FORWARD and secondary attack are pulsed for "
                  "200 ms.");
    } else if (request.action == MeleeAction::JumpKick) {
        Report(
            "INFO", "melee_jump_kick",
            "Jump kick (secondary attack) triggered from Retail's existing "
            "airborne state; no jump command was injected.");
    } else if (request.action == MeleeAction::OffHandStrike) {
        Report(
            "INFO", "melee_off_hand_strike",
            "Melee (secondary attack) triggered by a forward thrust of the "
            "free hand.");
    } else {
        Report(
            "INFO", "melee_weapon_strike",
            "Melee (secondary attack) triggered by a forward thrust of the "
            "weapon hand.");
    }
}

// Adresse des LadderMgr-Singletons, einmal aufgeloest und ueber das
// Bytemuster des Accessors abgesichert. Stimmt etwas nicht, bleibt das
// Klettern aus und das Spiel unveraendert.
const void* ResolveRetailLadderManager() noexcept {
    static const void* resolved = nullptr;
    static bool attempted = false;
    if (attempted) {
        return resolved;
    }
    attempted = true;

    HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return nullptr;
    }
    auto* const base = reinterpret_cast<unsigned char*>(module);
    __try {
        const unsigned char* const accessor =
            base + kRetailLadderInstanceRva;
        if (std::memcmp(
                accessor, kRetailLadderInstancePrefix,
                sizeof(kRetailLadderInstancePrefix)) != 0 ||
            std::memcmp(
                accessor + 6, kRetailLadderInstanceGuard,
                sizeof(kRetailLadderInstanceGuard)) != 0) {
            Report(
                "WARN", "ladder_manager_pattern_mismatch",
                "LadderMgr::Instance did not match the verified Retail 1.08 "
                "pattern; hand climbing stays disabled.");
            return nullptr;
        }
        const unsigned char* const tail =
            accessor + kRetailLadderInstanceReturnOffset;
        // mov eax, <Objekt> / ret
        if (tail[0] != 0xB8 || tail[5] != 0xC3) {
            Report(
                "WARN", "ladder_manager_pattern_mismatch",
                "LadderMgr::Instance does not end in the expected "
                "magic-static return; hand climbing stays disabled.");
            return nullptr;
        }
        // Die Adresse im Code ist bereits relokiert. Stimmt sie mit der
        // erwarteten RVA ueberein, ist das Objekt zweifelsfrei bestimmt.
        const auto* const encoded =
            *reinterpret_cast<const unsigned char* const*>(tail + 1);
        if (encoded != base + kRetailLadderObjectRva) {
            Report(
                "WARN", "ladder_manager_address_mismatch",
                "LadderMgr's static object is not at the verified offset; "
                "hand climbing stays disabled.");
            return nullptr;
        }
        resolved = encoded;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        resolved = nullptr;
        Report(
            "WARN", "ladder_manager_unreadable",
            "LadderMgr::Instance was not readable; hand climbing stays "
            "disabled.");
    }
    if (resolved != nullptr) {
        Report(
            "INFO", "ladder_manager_resolved",
            "Retail LadderMgr located and verified; hand climbing is "
            "available.");
    }
    return resolved;
}

// `LadderMgr::IsClimbing()`: m_pLadder liegt als erster Member im Objekt.
bool RetailPlayerIsOnLadder() noexcept {
    const void* const manager = ResolveRetailLadderManager();
    if (manager == nullptr) {
        return false;
    }
    __try {
        return *reinterpret_cast<const void* const*>(manager) != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Handklettern an Leitern.
//
// Greifen und Ziehen wertet `climb_grip.h` aus, ohne Headset geprueft. Hier
// kommt die Spielbedingung dazu: Nur waehrend `LadderMgr` eine Leiter fuehrt,
// bekommen die Grabknoepfe ihre Kletterbedeutung — ueberall sonst bleiben sie
// Sprint und Benutzen.
//
// Bewegt wird ueber die Kommandos, die Retail an der Leiter ohnehin auswertet
// (`CMoveMgr`: FORWARD bzw. REVERSE, siehe `UpdateControlFlags`). Die
// Klettergeschwindigkeit ist dort fest; der Zugwert entscheidet nur ueber die
// Richtung. Ein Schreibzugriff auf die Spielerphysik ist nicht noetig.
void UpdateClimbMotion() noexcept {
    if (!g_climbingEnabled) {
        if (g_climbActive || g_climbOnLadder) {
            g_climbActive = false;
            g_climbOnLadder = false;
            g_climbAxis = 0.0F;
            ResetClimbGrip(g_climbGrip);
        }
        return;
    }
    const bool onLadder = RetailPlayerIsOnLadder();
    const ClimbPull pull = UpdateClimbGrip(
        g_climbGrip, g_currentInput, MonotonicNanoseconds(), onLadder);
    // An der Leiter gehoert die Vorwaertsbewegung den Haenden — auch wenn
    // gerade niemand greift. Sonst klettert der Stick weiter mit, und die
    // Einstellung „HANDS" waere nur eine zusaetzliche Moeglichkeit statt
    // einer Entscheidung.
    g_climbOnLadder = onLadder;
    g_climbActive = onLadder && pull.gripping;
    g_climbAxis = g_climbActive ? pull.axis : 0.0F;

    if (g_climbActive != g_climbWasGripping) {
        Report(
            "INFO",
            g_climbActive ? "climb_grip_engaged" : "climb_grip_released",
            g_climbActive
                ? "A hand grabbed the ladder; the grab buttons now climb."
                : "The ladder grab was released.");
        g_climbWasGripping = g_climbActive;
    }
}

// Retails Kameraneigung ueber die linke Handneigung. Sie faellt weg, sobald
// physisch gelehnt wird: Beides zusammen kippte das Bild zusaetzlich zu einer
// Bewegung, die der Spieler ohnehin selbst macht.
bool HandLeanSuppressed(std::uint32_t command) noexcept {
    return g_physicalLeanEnabled &&
           (command == FEARVR_CMD_LEAN_LEFT ||
            command == FEARVR_CMD_LEAN_RIGHT);
}

// Kletterbewegung als Kommandowert. Liefert `false`, wenn dieses Kommando
// gerade nichts mit dem Klettern zu tun hat.
bool ClimbCommandOverride(
    std::uint32_t command, FearVrCommandValue& value) noexcept {
    if (!g_climbingEnabled || !g_climbOnLadder) {
        return false;
    }
    switch (command) {
    case FEARVR_CMD_FORWARD:
        value = {1.0F, g_climbAxis > 0.0F};
        return true;
    case FEARVR_CMD_REVERSE:
        value = {1.0F, g_climbAxis < 0.0F};
        return true;
    case FEARVR_CMD_FORWARD_AXIS:
        value = {g_climbAxis, g_climbAxis != 0.0F};
        return true;
    default:
        return false;
    }
}

// Die einzige Bewegung, die das Kampfsystem synthetisch erzeugt. Beide Pulse
// sind kurz und entstehen nur nach den Guards im reinen Melee-Classifier.
bool SlideKickCommandOverride(
    std::uint32_t command, FearVrCommandValue& value) noexcept {
    const ULONGLONG now = GetTickCount64();
    if (command == FEARVR_CMD_DUCK &&
        now < g_slideDuckPulseUntil) {
        value = {1.0F, true};
        return true;
    }
    if (now >= g_slideForwardPulseUntil) {
        return false;
    }
    if (command == FEARVR_CMD_FORWARD) {
        value = {1.0F, true};
        return true;
    }
    if (command == FEARVR_CMD_FORWARD_AXIS) {
        value = {1.0F, true};
        return true;
    }
    return false;
}

bool IsWeaponSwitchPulse(std::uint32_t command) noexcept {
    const ULONGLONG now = GetTickCount64();
    switch (command) {
    case FEARVR_CMD_NEXT_WEAPON:
        return now < g_weaponSwitchPulseUntil;
    case FEARVR_CMD_RELOAD:
        return now < g_reloadPulseUntil;
    case FEARVR_CMD_THROW_GRENADE:
        return now < g_grenadePulseUntil;
    case FEARVR_CMD_ALT_FIRING:
        return now < g_meleePulseUntil;
    default:
        return false;
    }
}

// Welche Kommandos ihren Zustand aus der Pulslogik beziehen statt aus der
// zustandslosen Zuordnung.
bool IsPulseDrivenCommand(std::uint32_t command) noexcept {
    return command == FEARVR_CMD_NEXT_WEAPON ||
           command == FEARVR_CMD_PREV_WEAPON ||
           command == FEARVR_CMD_RELOAD ||
           command == FEARVR_CMD_THROW_GRENADE ||
           command == FEARVR_CMD_ALT_FIRING;
}

FearVrCommandValue MapVrControllerCommand(
    std::uint32_t command) noexcept {
    if (DualPistolsVrActive()) {
        constexpr float kTriggerThreshold = 0.55F;
        const bool leftActive =
            (g_currentInput.activeHands & FEARVR_HAND_MASK_LEFT) != 0;
        const bool rightActive =
            (g_currentInput.activeHands & FEARVR_HAND_MASK_RIGHT) != 0;
        if (command == FEARVR_CMD_FIRING) {
            const bool firing =
                (leftActive &&
                 g_currentInput.trigger[FEARVR_HAND_LEFT] >=
                     kTriggerThreshold) ||
                (rightActive &&
                 g_currentInput.trigger[FEARVR_HAND_RIGHT] >=
                     kTriggerThreshold);
            return {1.0F, firing};
        }
        if (command == FEARVR_CMD_SLOWMO) {
            return {1.0F, g_dualPistolSlowMoActive};
        }
    }
    FearVrCommandValue mapped = MapControllerCommand(
        g_currentInput, command, g_twoHandedGripActive);
    if (command == FEARVR_CMD_DUCK && g_physicalDuckActive) {
        mapped = {1.0F, true};
    }
    return mapped;
}

// Ein Kommandowert fuer die Injektion: Klettern hat Vorrang, danach die
// Pulsgesten, sonst die zustandslose Zuordnung.
//
// FORWARD und REVERSE werden ausserhalb des Kletterns bewusst nicht
// injiziert. Retail bewegt normal ueber die Achse; ein zusaetzlich gesetztes
// Richtungsbit wuerde aus jedem kleinen Stickausschlag volle Geschwindigkeit
// machen.
FearVrCommandValue ResolveInjectedCommand(
    std::uint32_t command) noexcept {
    FearVrCommandValue value{0.0F, false};
    if (ClimbCommandOverride(command, value)) {
        return value;
    }
    if (SlideKickCommandOverride(command, value)) {
        return value;
    }
    if (HandLeanSuppressed(command)) {
        return {0.0F, false};
    }
    if (command == FEARVR_CMD_FORWARD ||
        command == FEARVR_CMD_REVERSE ||
        command == FEARVR_CMD_STRAFE_LEFT ||
        command == FEARVR_CMD_STRAFE_RIGHT) {
        return {0.0F, false};
    }
    if (IsPulseDrivenCommand(command)) {
        return {1.0F, IsWeaponSwitchPulse(command)};
    }
    return MapVrControllerCommand(command);
}

void LogRegressionCommandTransition(
    std::uint32_t command, bool active) noexcept {
    for (RegressionCommandLog& entry : g_regressionCommands) {
        if (entry.command != command) {
            continue;
        }
        char message[192]{};
        if (active) {
            entry.engagedTick = GetTickCount64();
            ++entry.count;
            if (entry.reportLeanRoll) {
                const double degrees =
                    static_cast<double>(
                        LeftHandLeanRollRadians(g_currentInput)) *
                    (180.0 / 3.14159265358979323846);
                std::snprintf(
                    message, sizeof(message),
                    "%s engaged; occurrence %u, left-hand roll %.1f deg.",
                    entry.label, entry.count, degrees);
            } else {
                std::snprintf(
                    message, sizeof(message),
                    "%s engaged; occurrence %u.",
                    entry.label, entry.count);
            }
            Report("INFO", entry.engagedEvent, message);
        } else {
            const ULONGLONG heldMs =
                entry.engagedTick == 0
                    ? 0
                    : GetTickCount64() - entry.engagedTick;
            std::snprintf(
                message, sizeof(message),
                "%s released after %llu ms; occurrence %u.",
                entry.label,
                static_cast<unsigned long long>(heldMs), entry.count);
            Report("INFO", entry.releasedEvent, message);
        }
        return;
    }
}

void InjectSemanticCommandBits(
    const void* bindManager) noexcept {
    if (g_semanticBitsInjected || bindManager == nullptr ||
        g_disableCommandInjection || DevMenuCapturesControllerInput()) {
        return;
    }
    g_semanticBitsInjected = true;

    // Waehrend Zwischensequenzen, Ladebildschirmen und Menues steht das
    // Weapon-Manager-Update still; Retail verarbeitet Spielkommandos dann
    // nicht im normalen Ablauf. Weiter Bits in den CBindMgr zu schreiben hat
    // das Spiel an einer geskripteten Szene reproduzierbar zum Absturz
    // gebracht: Das gehaltene Benutzen-Kommando feuerte in die gerade
    // ablaufende Szene hinein. Nachgewiesen dadurch, dass die Szene ohne den
    // Client-Input-Hook durchlaeuft und mit ihm nicht.
    //
    // Die Menuenavigation braucht diesen Pfad nicht: Sie laeuft ueber
    // synthetische Tastendruecke, nicht ueber die Kommandobits.
    const ULONGLONG nowTick = GetTickCount64();
    const bool playingFrameFresh =
        g_lastWeaponManagerUpdateTick != 0 &&
        nowTick - g_lastWeaponManagerUpdateTick <=
            kPlayingFrameFreshMilliseconds;
    if (!playingFrameFresh) {
        // Gehaltene Kommandos gelten als losgelassen, damit beim naechsten
        // Spielframe kein Tastendruck vorgetaeuscht wird, der nie endete.
        for (bool& active : g_injectedCommandActive) {
            active = false;
        }
        if (InterlockedCompareExchange(
                &g_commandInjectionSuspendedLogged, 1, 0) == 0) {
            Report(
                "INFO", "command_injection_suspended",
                "No playing frame: controller commands are no longer written "
                "into the Retail bind manager until gameplay resumes.");
        }
        return;
    }

    // Retail 1.08's verified VC7.1 vector<bool> layout stores the bit count
    // at CBindMgr+0x10 and its uint32 word array at CBindMgr+0x18.
    constexpr std::uint32_t kDigitalCommands[] = {
        FEARVR_CMD_YAW_POS,
        FEARVR_CMD_YAW_NEG,
        // Nur fuer das Klettern an Leitern; sonst liefert
        // ResolveInjectedCommand fuer beide dauerhaft inaktiv.
        FEARVR_CMD_FORWARD,
        FEARVR_CMD_REVERSE,
        FEARVR_CMD_STRAFE_LEFT,
        FEARVR_CMD_STRAFE_RIGHT,
        FEARVR_CMD_DUCK,
        FEARVR_CMD_JUMP,
        FEARVR_CMD_RUN,
        FEARVR_CMD_FIRING,
        FEARVR_CMD_FOCUS,
        FEARVR_CMD_PREV_WEAPON,
        FEARVR_CMD_NEXT_WEAPON,
        FEARVR_CMD_ACTIVATE,
        FEARVR_CMD_RELOAD,
        FEARVR_CMD_THROW_GRENADE,
        FEARVR_CMD_ALT_FIRING,
        FEARVR_CMD_SLOWMO,
        FEARVR_CMD_MEDKIT,
        FEARVR_CMD_LEAN_LEFT,
        FEARVR_CMD_LEAN_RIGHT
    };
    __try {
        const auto* const bytes =
            static_cast<const unsigned char*>(bindManager);
        const std::uint32_t bitCount =
            *reinterpret_cast<const std::uint32_t*>(
                bytes + 0x10);
        auto* const words =
            *reinterpret_cast<std::uint32_t* const*>(
                bytes + 0x18);
        if (words == nullptr) {
            return;
        }

        for (const std::uint32_t command : kDigitalCommands) {
            const FearVrCommandValue controller =
                ResolveInjectedCommand(command);
            if (command < sizeof(g_injectedCommandActive) /
                              sizeof(g_injectedCommandActive[0])) {
                bool& wasActive =
                    g_injectedCommandActive[command];
                if (controller.active && !wasActive) {
                    char message[80]{};
                    std::snprintf(
                        message, sizeof(message),
                        "command=%u bit_count=%u",
                        command, bitCount);
                    Report(
                        "INFO", "controller_command_injected",
                        message);
                }
                if (controller.active != wasActive) {
                    LogRegressionCommandTransition(
                        command, controller.active);
                }
                wasActive = controller.active;
            }
            if (controller.active && command < bitCount) {
                words[command >> 5] |=
                    1U << (command & 31U);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Report(
            "ERROR", "controller_command_injection_failed",
            "Retail CBindMgr command-state storage was not readable.");
    }
}

float __fastcall HookRetailGetBindingValue(
    const void* bindManager, void* ignoredEdx,
    const RetailBinding* binding,
    bool returnDefaultOnDisabled) {
    (void)ignoredEdx;
    const float original = g_retailGetBindingValue(
        bindManager, binding, returnDefaultOnDisabled);
    InjectSemanticCommandBits(bindManager);
    if (binding == nullptr) {
        return original;
    }
    // The default mouse bindings feed COMMAND_ID_PITCH/YAW directly into
    // CPlayerCamera's persistent local rotation. Suppress only those legacy
    // look paths while the native 3D world is actually active. Flat menus,
    // host loss, F8 fallback and -NoStereo retain normal mouse control.
    if ((IsLegacyVrPitchCommand(binding->command) ||
         IsLegacyVrMouseYawCommand(binding->command)) &&
        IsNativeVrWorldActive()) {
        return 0.0F;
    }
    if (DevMenuCapturesControllerInput()) {
        return original;
    }
    // Menu is handled once on the left menu-button edge through
    // IClientShell::OnKeyDown(VK_ESCAPE). Injecting COMMAND_ID_MENU too
    // would open and close the pause menu within the same update.
    if (binding->command == FEARVR_CMD_MENU) {
        return original;
    }

    if (binding->command == FEARVR_CMD_FORWARD_AXIS) {
        g_seenForwardAxisBinding = true;
    } else if (binding->command == FEARVR_CMD_STRAFE_AXIS) {
        g_seenStrafeAxisBinding = true;
    }

    // An der Leiter ziehen die Haende; beim Slide Kick haelt der kurze
    // Sequencer DUCK/FORWARD; sonst gilt die gewohnte Zuordnung inklusive der
    // normalen, analogen Stickbewegung.
    FearVrCommandValue controller{0.0F, false};
    if (HandLeanSuppressed(binding->command)) {
        controller = {0.0F, false};
    } else if (
        !ClimbCommandOverride(binding->command, controller) &&
        !SlideKickCommandOverride(binding->command, controller)) {
        controller =
            IsPulseDrivenCommand(binding->command)
                ? FearVrCommandValue{
                      1.0F, IsWeaponSwitchPulse(binding->command)}
                : MapVrControllerCommand(binding->command);
    }
    if (binding->command <
        sizeof(g_controllerCommandActive) /
            sizeof(g_controllerCommandActive[0])) {
        bool& wasActive =
            g_controllerCommandActive[binding->command];
        if (controller.active && !wasActive) {
            char message[160]{};
            std::snprintf(
                message, sizeof(message),
                "command=%u value=%.3f range=[%.3f,%.3f]",
                binding->command, controller.value,
                binding->commandMin, binding->commandMax);
            Report(
                "INFO", "controller_command_activated", message);
        }
        wasActive = controller.active;
    }
    if (!controller.active) {
        return original;
    }

    // Prefer analog motion when that command exists. Digital movement
    // remains a fallback for profiles without Pad0 axis bindings.
    if ((binding->command == FEARVR_CMD_FORWARD ||
         binding->command == FEARVR_CMD_REVERSE) &&
        g_seenForwardAxisBinding) {
        return original;
    }
    if ((binding->command == FEARVR_CMD_STRAFE_LEFT ||
         binding->command == FEARVR_CMD_STRAFE_RIGHT) &&
        g_seenStrafeAxisBinding) {
        return original;
    }
    if (binding->command == FEARVR_CMD_FORWARD_AXIS ||
        binding->command == FEARVR_CMD_STRAFE_AXIS ||
        binding->command == FEARVR_CMD_YAW_ACCEL) {
        return std::fabs(controller.value) > std::fabs(original)
            ? controller.value
            : original;
    }
    return ActiveBindingValue(*binding);
}

bool ResolveRetailWeaponTargets(
    void*& weaponManagerUpdate, void*& setWeaponTransform,
    void*& setWeaponVisible, void*& startMuzzleFlash,
    void*& getFireVectors,
    void*& setTrackedTarget,
    void*& vectorObjectFilter) noexcept {
    weaponManagerUpdate = nullptr;
    setWeaponTransform = nullptr;
    setWeaponVisible = nullptr;
    startMuzzleFlash = nullptr;
    getFireVectors = nullptr;
    setTrackedTarget = nullptr;
    vectorObjectFilter = nullptr;

    HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return false;
    }
    auto* const base = reinterpret_cast<unsigned char*>(module);
    __try {
        const auto* const dos =
            reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
            dos->e_lfanew <= 0) {
            return false;
        }
        const auto* const nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(
                base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->FileHeader.TimeDateStamp !=
                kRetailGameClientTimeDateStamp ||
            nt->OptionalHeader.SizeOfImage !=
                kRetailGameClientSizeOfImage) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    auto* const update = base + kRetailWeaponManagerUpdateRva;
    auto* const setTransform = base + kRetailSetWeaponTransformRva;
    auto* const setVisible = base + kRetailSetWeaponVisibleRva;
    auto* const muzzleFlash = base + kRetailStartMuzzleFlashRva;
    auto* const fireVectors = base + kRetailGetFireVectorsRva;
    auto* const trackedTarget = base + kRetailSetTrackedTargetRva;
    auto* const vectorFilter =
        base + kRetailVectorObjectFilterRva;
    constexpr unsigned char kUpdatePrefix[] = {
        0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x08, 0xFF, 0x75,
        0x50, 0x8B, 0x46, 0x0C, 0x85, 0xC0, 0x74, 0x49};
    constexpr unsigned char kSetTransformPrefix[] = {
        0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x1C, 0x85, 0xC0,
        0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x74, 0x0D};
    constexpr unsigned char kSetVisiblePrefix[] = {
        0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x1C, 0x85, 0xC0,
        0x75, 0x0E, 0x8B, 0x86, 0xD4, 0x00, 0x00, 0x00};
    // `mov al, byte ptr [esi+0x223]` in SetVisible, direkt gefolgt vom
    // Ruecksprung: das ist `m_bVisible = bVis; if (m_bDisabled) return;`.
    // Damit ist der Ort des Deaktiviert-Flags belegt.
    constexpr unsigned char kSetVisibleDisabledProbe[] = {
        0x8A, 0x86, 0x23, 0x02, 0x00, 0x00};
    // CWeaponModelData::StartMuzzleFlash begins by reserving its ClientFX
    // create structure and loading m_MuzzleFlashFX from +0x50.
    constexpr unsigned char kStartMuzzleFlashPrefix[] = {
        0x81, 0xEC, 0xC4, 0x00, 0x00, 0x00, 0x56, 0x8B,
        0xF1, 0x8B, 0x46, 0x50, 0x85, 0xC0, 0x57, 0x74,
        0x22};
    constexpr unsigned char kFireVectorsPrefix[] = {
        0x83, 0xEC, 0x58, 0xA1};
    constexpr unsigned char kFireVectorsBody[] = {
        0x57, 0xC7, 0x44, 0x24, 0x2C, 0x00, 0x00, 0x00, 0x00};
    constexpr unsigned char kFireVectorsFireHandProbe[] = {
        0x8A, 0x87, 0xE3, 0x01, 0x00, 0x00};
    constexpr unsigned char kTrackedTargetPrefix[] = {
        0x8B, 0x44, 0x24, 0x04, 0x83, 0xF8, 0xFF, 0x74,
        0x71, 0x8B, 0x54, 0x24, 0x08, 0x3B, 0x51, 0x10};
    constexpr unsigned char kVectorFilterPrefix[] = {
        0x8B, 0x48, 0x14, 0x8B, 0x41, 0x10, 0x56,
        0x8B, 0x74, 0x24, 0x08, 0x3B, 0xF0, 0x75, 0x04,
        0x32, 0xC0, 0x5E, 0xC3};
    if (!MatchesCode(
            update, kUpdatePrefix, sizeof(kUpdatePrefix)) ||
        !MatchesCode(
            setTransform, kSetTransformPrefix,
            sizeof(kSetTransformPrefix)) ||
        !MatchesCode(
            setVisible, kSetVisiblePrefix,
            sizeof(kSetVisiblePrefix)) ||
        !MatchesCode(
            setVisible + kRetailSetWeaponVisibleDisabledProbeOffset,
            kSetVisibleDisabledProbe,
            sizeof(kSetVisibleDisabledProbe)) ||
        !MatchesCode(
            muzzleFlash, kStartMuzzleFlashPrefix,
            sizeof(kStartMuzzleFlashPrefix)) ||
        !MatchesCode(
            fireVectors, kFireVectorsPrefix,
            sizeof(kFireVectorsPrefix)) ||
        !MatchesCode(
            fireVectors + 8, kFireVectorsBody,
            sizeof(kFireVectorsBody)) ||
        !MatchesCode(
            fireVectors + kRetailGetFireVectorsFireHandProbeOffset,
            kFireVectorsFireHandProbe,
            sizeof(kFireVectorsFireHandProbe)) ||
        !MatchesCode(
            trackedTarget, kTrackedTargetPrefix,
            sizeof(kTrackedTargetPrefix)) ||
        !MatchesCode(
            vectorFilter + 5, kVectorFilterPrefix,
            sizeof(kVectorFilterPrefix))) {
        return false;
    }

    weaponManagerUpdate = update;
    setWeaponTransform = setTransform;
    setWeaponVisible = setVisible;
    startMuzzleFlash = muzzleFlash;
    getFireVectors = fireVectors;
    setTrackedTarget = trackedTarget;
    vectorObjectFilter = vectorFilter;
    return true;
}

bool BuildTrackedHandTransform(
    const LTRotation& baseRotation, const LTVector& basePosition,
    std::uint32_t validHands, std::uint32_t requiredHand,
    const FearVrPose& pose,
    LTRigidTransform& transform) noexcept {
    if (!g_headTracking.centered || g_headTracking.trackingLost ||
         (g_currentInput.flags & FEARVR_IF_FOCUSED) == 0 ||
        (validHands & requiredHand) == 0) {
        return false;
    }
    // Positions must remain relative to the current HMD, since the Retail
    // camera/body does not follow physical head translation. Keep the
    // recenter orientation as the stable room-to-game basis.
    FearVrPose positionReference = g_headTracking.recenter;
    positionReference.px = g_headTracking.currentCenter.px;
    positionReference.py = g_headTracking.currentCenter.py;
    positionReference.pz = g_headTracking.currentCenter.pz;
    const RelativeEyePose relative =
        TrackedPoseRelativeToRecenter(positionReference, pose);
    if (!relative.valid) {
        return false;
    }

    // Derselbe Versatz wie beim Blickpunkt: Die Handpose ist relativ zum HMD
    // gerechnet, haengt aber an der Spielerposition. Ohne diesen Ausgleich
    // klafft beim Lehnen genau die Luecke zwischen Kopf und Koerper, und die
    // Waffe scheint aus der Hand zu wandern.
    const LTVector localPosition(
        relative.positionMeters.x * kGameUnitsPerMeter +
            g_leanViewOffsetUnits.x,
        relative.positionMeters.y * kGameUnitsPerMeter +
            g_leanViewOffsetUnits.y,
        relative.positionMeters.z * kGameUnitsPerMeter +
            g_leanViewOffsetUnits.z);
    const LTRotation localRotation(
        relative.rotation.x, relative.rotation.y,
        relative.rotation.z, relative.rotation.w);
    transform.m_vPos =
        basePosition + baseRotation.RotateVector(localPosition);
    transform.m_rRot = baseRotation * localRotation;
    return true;
}

// Retail reicht dem Weapon-Manager CPlayerCamera::m_vPos weiter. Dieser Wert
// entsteht vor der abschliessenden Hoehen-, Effekt- und Kollisionskorrektur,
// waehrend die VR-Augen das danach gesetzte Kameraobjekt rendern. Besonders
// bei Sprung und Ducken laufen beide Y-Positionen kurz auseinander. Fuer
// OpenXR-Haende und die daran verriegelte Waffe wird deshalb in nativer
// 3D-VR normalerweise die finale Objektposition verwendet. Treppab und in der
// Luft wird lediglich die visuelle Y-Achse auf Retails Rohhoehe umgeschaltet,
// bis der Filter aufgeholt hat. Retails eigener Updateaufruf erhaelt weiterhin
// unveraendert seine urspruengliche Basis.
LTVector ResolveTrackedHandBasePosition(
    const LTVector& retailBasePosition,
    const LTRotation& retailBaseRotation,
    LTRotation& resolvedBaseRotation) noexcept {
    resolvedBaseRotation = retailBaseRotation;
    if (g_client == nullptr ||
        g_playerCameraObject == nullptr ||
        g_isStereoEnabled == nullptr ||
        g_isFlatPanelActive == nullptr) {
        ResetVerticalCameraHeight(g_verticalCameraHeight);
        g_visualCameraHeightValid = false;
        return retailBasePosition;
    }

    __try {
        if (g_isStereoEnabled() == FALSE ||
            g_isFlatPanelActive() != FALSE) {
            ResetVerticalCameraHeight(g_verticalCameraHeight);
            g_visualCameraHeightValid = false;
            return retailBasePosition;
        }
        LTRigidTransform cameraTransform;
        if (g_client->GetObjectTransform(
                g_playerCameraObject, &cameraTransform) != LT_OK) {
            ResetVerticalCameraHeight(g_verticalCameraHeight);
            g_visualCameraHeightValid = false;
            return retailBasePosition;
        }
        if (g_retailPersistentUnsupported &&
            g_cutsceneCameraState) {
            // The eyes render from the requested pre-collision camera basis
            // during a moving-carrier sequence. Build both controller hands
            // and the weapon from that exact same basis; otherwise the
            // collision-corrected Retail camera keeps shaking them below an
            // already stable headset view.
            ResetVerticalCameraHeight(g_verticalCameraHeight);
            g_visualCameraHeightValid = false;
            g_leanViewOffsetUnits = LTVector(0.0F, 0.0F, 0.0F);
            const float blend =
                CurrentCutsceneCameraBlend(GetTickCount64());
            resolvedBaseRotation =
                ResolveCutsceneCameraBaseRotation(
                    cameraTransform.m_rRot, blend);
            LTVector resolvedPosition = cameraTransform.m_vPos;
            LTVector desiredPosition;
            if (ReadRetailDesiredCameraPosition(desiredPosition)) {
                resolvedPosition +=
                    (desiredPosition - cameraTransform.m_vPos) * blend;
            }
            return resolvedPosition;
        }
        // Keep tracked hands, weapon sockets and firing presentation on the
        // same yaw-only gameplay basis as the stereo eyes. Otherwise an old
        // Retail mouse pitch could disappear from the view yet remain baked
        // into the controller/weapon transforms until another game update.
        LTRotation yawOnlyRotation;
        if (ResolveYawOnlyRotation(
                cameraTransform.m_rRot, yawOnlyRotation) ||
            ResolveYawOnlyRotation(
                retailBaseRotation, yawOnlyRotation)) {
            resolvedBaseRotation = yawOnlyRotation;
        }
        const bool ducking =
            g_retailMovement.available &&
            (g_retailMovement.controlFlags &
             kRetailControlFlagDuck) != 0;
        // Beim Zweihandgriff verschieben Lauf-, Schuss- und Sprunganimationen
        // den animierten PlayerCamera-Socket. Die visuelle Augenhoehe wird
        // deshalb waehrend des gesamten Griffs aus dem physischen PlayerBody
        // rekonstruiert. Ducken sowie echte Kollisionskorrekturen behalten
        // Vorrang.
        const bool stabilizeTwoHandedCamera =
            g_twoHandedGripActive &&
            g_retailMovement.available &&
            !ducking;
        LTRigidTransform playerBodyTransform;
        float playerBodyHeight = 0.0F;
        bool playerBodyHeightValid = false;
        if (g_playerBodyObject != nullptr) {
            if (g_client->GetObjectTransform(
                    g_playerBodyObject,
                    &playerBodyTransform) == LT_OK &&
                std::isfinite(playerBodyTransform.m_vPos.y)) {
                playerBodyHeight = playerBodyTransform.m_vPos.y;
                playerBodyHeightValid = true;
            }
        }
        const TwoHandedJumpCameraOutput jumpHeight =
            UpdateTwoHandedJumpCamera(
                g_twoHandedJumpCamera,
                playerBodyHeight,
                playerBodyHeightValid,
                cameraTransform.m_vPos.y,
                g_retailMovement.onGround,
                g_retailMovement.airborne,
                ducking,
                g_twoHandedGripActive,
                g_retailMovement.available);
        // Remove the complete local camera bob, not just its world Y value.
        // Head-bob offsets are authored in camera-local space; after yaw/pitch
        // projection their X/Z components can appear as a vertical weapon
        // shift in the headset. Learn the stable body-to-camera offset outside
        // a two-hand grip, then carry the physical PlayerBody with that fixed
        // local offset for the whole grip.
        bool positionAnchorActive = false;
        LTVector anchoredCameraPosition = cameraTransform.m_vPos;
        if (playerBodyHeightValid &&
            RotationIsFiniteAndUsable(retailBaseRotation)) {
            const bool learnPositionAnchor =
                g_retailMovement.onGround &&
                !g_retailMovement.airborne && !ducking &&
                !g_twoHandedGripActive;
            if (learnPositionAnchor) {
                LTRotation inverseBase = retailBaseRotation.Conjugate();
                inverseBase.Normalize();
                const LTVector sample = inverseBase.RotateVector(
                    cameraTransform.m_vPos - playerBodyTransform.m_vPos);
                if (std::isfinite(sample.x) &&
                    std::isfinite(sample.y) &&
                    std::isfinite(sample.z) &&
                    sample.MagSqr() < 1000000.0F) {
                    if (!g_twoHandedCameraPosition.valid) {
                        g_twoHandedCameraPosition.localCameraOffset = sample;
                        g_twoHandedCameraPosition.valid = true;
                    } else {
                        constexpr float kStableOffsetLearningRate = 0.08F;
                        g_twoHandedCameraPosition.localCameraOffset +=
                            (sample -
                             g_twoHandedCameraPosition.localCameraOffset) *
                            kStableOffsetLearningRate;
                    }
                }
            }
            if (stabilizeTwoHandedCamera &&
                g_twoHandedCameraPosition.valid) {
                anchoredCameraPosition =
                    playerBodyTransform.m_vPos +
                    retailBaseRotation.RotateVector(
                        g_twoHandedCameraPosition.localCameraOffset);
                const LTVector correction =
                    anchoredCameraPosition - cameraTransform.m_vPos;
                constexpr float kPositionCollisionGuardUnits = 35.0F;
                positionAnchorActive =
                    std::isfinite(anchoredCameraPosition.x) &&
                    std::isfinite(anchoredCameraPosition.y) &&
                    std::isfinite(anchoredCameraPosition.z) &&
                    correction.MagSqr() <=
                        kPositionCollisionGuardUnits *
                        kPositionCollisionGuardUnits;
            }
        }
        const float stabilizedCameraHeight = positionAnchorActive
            ? anchoredCameraPosition.y
            : jumpHeight.visualHeight;
        const VerticalCameraHeightOutput height =
            UpdateVerticalCameraHeight(
                g_verticalCameraHeight,
                retailBasePosition.y,
                stabilizedCameraHeight,
                g_retailMovement.airborne,
                ducking,
                g_retailMovement.available,
                stabilizeTwoHandedCamera);
        LTVector resolvedPosition = positionAnchorActive
            ? anchoredCameraPosition
            : cameraTransform.m_vPos;
        resolvedPosition.y = height.visualHeight;
        g_visualCameraPosition = resolvedPosition;
        g_visualCameraHeightSampleTick = GetTickCount64();
        g_visualCameraHeightValid = true;
        if (positionAnchorActive &&
            InterlockedCompareExchange(
                &g_twoHandedCameraPositionStabilizedLogged, 1, 0) == 0) {
            const LTVector correction =
                resolvedPosition - cameraTransform.m_vPos;
            char message[224]{};
            std::snprintf(
                message, sizeof(message),
                "Full PlayerBody camera anchor active; removed local "
                "animation offset (%.2f, %.2f, %.2f) game units.",
                static_cast<double>(correction.x),
                static_cast<double>(correction.y),
                static_cast<double>(correction.z));
            Report(
                "INFO", "two_handed_camera_position_stabilized",
                message);
        }
        if (InterlockedCompareExchange(
                &g_weaponCameraBaseSyncActiveLogged, 1, 0) == 0) {
            Report(
                "INFO", "weapon_camera_base_sync_active",
                "Tracked hands and weapon use the final rendered camera "
                "position after Retail height and collision correction.");
        }
        if (height.bypassActive &&
            InterlockedCompareExchange(
                &g_directionalCameraHeightBypassLogged, 1, 0) == 0) {
            Report(
                "INFO", "directional_camera_height_bypass_active",
                "Descending stairs and airborne motion use Retail's raw "
                "visual height until its smoothing filter catches up.");
        }
        if (stabilizeTwoHandedCamera &&
            InterlockedCompareExchange(
                &g_twoHandedCameraHeightStabilizedLogged, 1, 0) == 0) {
            char message[320]{};
            std::snprintf(
                message, sizeof(message),
                "%s raw_y=%.2f final_y=%.2f body_y=%.2f visual_y=%.2f "
                "body_valid=%d anchor_valid=%d collision_guard=%d.",
                jumpHeight.anchorActive
                    ? "Physical PlayerBody height anchor active."
                    : "PlayerBody anchor unavailable; final camera fallback.",
                static_cast<double>(retailBasePosition.y),
                static_cast<double>(cameraTransform.m_vPos.y),
                static_cast<double>(playerBodyHeight),
                static_cast<double>(height.visualHeight),
                playerBodyHeightValid ? 1 : 0,
                g_twoHandedJumpCamera.groundedEyeOffsetValid ? 1 : 0,
                jumpHeight.collisionGuarded ? 1 : 0);
            Report(
                "INFO", "two_handed_camera_height_stabilized",
                message);
        }
        return resolvedPosition;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ResetVerticalCameraHeight(g_verticalCameraHeight);
        ResetTwoHandedJumpCamera(g_twoHandedJumpCamera);
        g_twoHandedCameraPosition = TwoHandedCameraPositionState{};
        g_visualCameraHeightValid = false;
        return retailBasePosition;
    }
}

bool BuildStableTrackedHandTransform(
    const LTRotation& baseRotation, const LTVector& basePosition,
    std::uint32_t validHands, std::uint32_t requiredHand,
    const FearVrPose& pose, TrackedPoseCache& cache,
    LTRigidTransform& transform) noexcept {
    const ULONGLONG now = GetTickCount64();
    const LONG resetGeneration = InterlockedCompareExchange(
        &g_trackingResetGeneration, 0, 0);
    if ((validHands & requiredHand) != 0 &&
        BuildTrackedHandTransform(
            baseRotation, basePosition, validHands, requiredHand,
            pose, transform)) {
        cache.pose = pose;
        cache.lastValidTick = now;
        cache.resetGeneration = resetGeneration;
        cache.valid = true;
        return true;
    }

    if (!cache.valid ||
        cache.resetGeneration != resetGeneration ||
        now - cache.lastValidTick >
            kTrackedPoseGapGraceMilliseconds) {
        cache.valid = false;
        return false;
    }
    if (!BuildTrackedHandTransform(
            baseRotation, basePosition, requiredHand, requiredHand,
            cache.pose, transform)) {
        return false;
    }
    if (InterlockedCompareExchange(
            &g_handPoseGapBridgedLogged, 1, 0) == 0) {
        Report(
            "INFO", "hand_pose_gap_bridged",
            "A transient OpenXR hand-pose gap used the last valid pose; "
            "the Retail weapon socket remained controller-driven.");
    }
    return true;
}

void* CurrentRetailWeapon(void* weaponManager) noexcept {
    void* weapon = nullptr;
    __try {
        weapon = *reinterpret_cast<void**>(
            static_cast<unsigned char*>(weaponManager) +
            kRetailCurrentWeaponOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        weapon = nullptr;
    }
    return weapon;
}

// Ist die Waffe von Retail abgeschaltet? Dann ist ihr Modell unsichtbar —
// an der Leiter, in Zwischensequenzen und am Geschuetz.
bool RetailWeaponIsDisabled(const void* weapon) noexcept {
    if (weapon == nullptr) {
        return false;
    }
    __try {
        return *(static_cast<const unsigned char*>(weapon) +
                 kRetailWeaponDisabledOffset) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Das sichtbare Modell der rechten (bei Doppelwaffen: der ersten) Waffe.
HOBJECT RetailRightWeaponModel(const void* weapon) noexcept {
    if (weapon == nullptr) {
        return nullptr;
    }
    HOBJECT model = nullptr;
    __try {
        model = *reinterpret_cast<HOBJECT const*>(
            static_cast<const unsigned char*>(weapon) +
            kRetailRightWeaponModelObjectOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return model;
}

bool RetailRightWeaponModelIsVisible(const void* weapon) noexcept {
    if (g_client == nullptr) {
        return false;
    }
    HOBJECT const model = RetailRightWeaponModel(weapon);
    if (model == nullptr) {
        return false;
    }
    __try {
        ILTCommon* const common = g_client->Common();
        uint32 flags = 0;
        return common != nullptr &&
               common->GetObjectFlags(
                   model, OFT_Flags, flags) == LT_OK &&
               (flags & FLAG_VISIBLE) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Keep the carried model renderable without calling CClientWeapon::SetVisible.
// Retail's full visibility routine always hides m_MuzzleFlashFX, even when it
// is asked to show the weapon, so it must not run after the weapon update has
// made a firing flash visible.
void ForceRetailRightWeaponModelVisible(const void* weapon) noexcept {
    static volatile LONG fixActiveLogged = 0;
    if (g_client == nullptr) {
        return;
    }
    HOBJECT const model = RetailRightWeaponModel(weapon);
    if (model == nullptr) {
        return;
    }
    __try {
        ILTCommon* const common = g_client->Common();
        if (common != nullptr &&
            common->SetObjectFlags(
                model, OFT_Flags, FLAG_VISIBLE,
                FLAG_VISIBLE) == LT_OK &&
            InterlockedCompareExchange(
                &fixActiveLogged, 1, 0) == 0) {
            Report(
                "INFO", "muzzle_flash_visibility_fix_active",
                "Post-update weapon visibility now changes only the model "
                "flag, preserving Retail muzzle-flash FX state.");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

WeaponWeightPose ToWeaponWeightPose(const FearVrPose& pose) noexcept {
    return {
        {pose.px, pose.py, pose.pz},
        {pose.qx, pose.qy, pose.qz, pose.qw}};
}

FearVrPose FromWeaponWeightPose(const WeaponWeightPose& pose) noexcept {
    FearVrPose result{};
    result.px = pose.position.x;
    result.py = pose.position.y;
    result.pz = pose.position.z;
    result.qx = pose.orientation.x;
    result.qy = pose.orientation.y;
    result.qz = pose.orientation.z;
    result.qw = pose.orientation.w;
    return result;
}

WeaponWeightProfile LoadWeaponWeightProfile(
    const void* weapon, char (&profileName)[96]) noexcept {
    profileName[0] = '\0';
    bool namedProfile = false;
    if (g_client != nullptr && weapon != nullptr) {
        HOBJECT modelObject = RetailRightWeaponModel(weapon);
        ILTModel* const model = g_client->GetModelLT();
        char modelFile[192]{};
        if (modelObject != nullptr && model != nullptr &&
            model->GetModelFilename(
                modelObject, modelFile,
                static_cast<std::uint32_t>(sizeof(modelFile))) == LT_OK &&
            modelFile[0] != '\0') {
            const char* base = modelFile;
            for (const char* cursor = modelFile; *cursor != '\0'; ++cursor) {
                if (*cursor == '\\' || *cursor == '/') {
                    base = cursor + 1;
                }
            }
            std::size_t used = 0;
            for (; base[used] != '\0' && base[used] != '.' &&
                   used + 1 < sizeof(profileName); ++used) {
                const char value = base[used];
                profileName[used] =
                    value >= 'A' && value <= 'Z'
                    ? static_cast<char>(value - 'A' + 'a')
                    : ((value >= 'a' && value <= 'z') ||
                       (value >= '0' && value <= '9'))
                        ? value : '_';
            }
            profileName[used] = '\0';
            namedProfile = used != 0;
        }
    }

    if (!namedProfile) {
        std::snprintf(profileName, sizeof(profileName), "default");
        return g_defaultWeaponWeightProfile;
    }

    wchar_t section[128] = L"WeaponWeight.";
    std::size_t sectionUsed = std::wcslen(section);
    for (std::size_t index = 0;
         profileName[index] != '\0' && sectionUsed + 1 < std::size(section);
         ++index) {
        section[sectionUsed++] =
            static_cast<unsigned char>(profileName[index]);
    }
    section[sectionUsed] = L'\0';
    return SanitizeWeaponWeightProfile({
        ReadVrFloat(
            section, L"Weight", g_defaultWeaponWeightProfile.weight),
        ReadVrFloat(
            section, L"PositionalFollow",
            g_defaultWeaponWeightProfile.positionalFollow),
        ReadVrFloat(
            section, L"RotationalFollow",
            g_defaultWeaponWeightProfile.rotationalFollow),
        ReadVrFloat(
            section, L"CatchUpStrength",
            g_defaultWeaponWeightProfile.catchUpStrength)});
}

void SaveActiveWeaponWeightProfile() noexcept {
    const WeightedWeaponInputState& weighted = g_weightedWeaponInput;
    if (weighted.profileName[0] == '\0') {
        return;
    }
    wchar_t section[128] = L"WeaponWeight.";
    std::size_t used = std::wcslen(section);
    for (std::size_t index = 0;
         weighted.profileName[index] != '\0' &&
         used + 1 < std::size(section); ++index) {
        section[used++] =
            static_cast<unsigned char>(weighted.profileName[index]);
    }
    section[used] = L'\0';
    WriteVrFloat(section, L"Weight", weighted.profile.weight);
    WriteVrFloat(
        section, L"PositionalFollow", weighted.profile.positionalFollow);
    WriteVrFloat(
        section, L"RotationalFollow", weighted.profile.rotationalFollow);
    WriteVrFloat(
        section, L"CatchUpStrength", weighted.profile.catchUpStrength);
}

WeaponRecoilProfile LoadWeaponRecoilProfile(
    const char* profileName, bool& explicitProfile) noexcept {
    explicitProfile = false;
    if (profileName == nullptr || profileName[0] == '\0') {
        return g_weaponRecoilProfile;
    }
    wchar_t section[128] = L"WeaponRecoil.";
    std::size_t used = std::wcslen(section);
    for (std::size_t index = 0;
         profileName[index] != '\0' && used + 1 < std::size(section);
         ++index) {
        section[used++] =
            static_cast<unsigned char>(profileName[index]);
    }
    section[used] = L'\0';

    wchar_t value[32]{};
    const auto hasValue = [&](const wchar_t* name) noexcept {
        value[0] = L'\0';
        return g_vrSettingsFilePresent &&
               g_vrSettingsPath[0] != L'\0' &&
               GetPrivateProfileStringW(
                   section, name, L"", value,
                   static_cast<DWORD>(std::size(value)),
                   g_vrSettingsPath) != 0;
    };
    explicitProfile =
        hasValue(L"Strength") || hasValue(L"MuzzleRise") ||
        hasValue(L"Recovery");
    return SanitizeWeaponRecoilProfile({
        ReadVrFloat(
            section, L"Strength", g_weaponRecoilProfile.strength),
        ReadVrFloat(
            section, L"MuzzleRise", g_weaponRecoilProfile.muzzleRise),
        ReadVrFloat(
            section, L"Recovery", g_weaponRecoilProfile.recovery)});
}

void SaveActiveWeaponRecoilProfile() noexcept {
    const WeightedWeaponInputState& weighted = g_weightedWeaponInput;
    if (!weighted.recoilProfileExplicit ||
        !HasCurrentWeaponWeightProfile()) {
        return;
    }
    wchar_t section[128] = L"WeaponRecoil.";
    std::size_t used = std::wcslen(section);
    for (std::size_t index = 0;
         weighted.profileName[index] != '\0' &&
         used + 1 < std::size(section); ++index) {
        section[used++] =
            static_cast<unsigned char>(weighted.profileName[index]);
    }
    section[used] = L'\0';
    WriteVrFloat(
        section, L"Strength", weighted.recoilProfile.strength);
    WriteVrFloat(
        section, L"MuzzleRise", weighted.recoilProfile.muzzleRise);
    WriteVrFloat(
        section, L"Recovery", weighted.recoilProfile.recovery);
}

WeaponCollisionProfile LoadWeaponCollisionProfile(
    const char* profileName, bool& explicitProfile) noexcept {
    explicitProfile = false;
    if (profileName == nullptr || profileName[0] == '\0' ||
        std::strcmp(profileName, "default") == 0) {
        return g_defaultWeaponCollisionProfile;
    }
    wchar_t section[128] = L"WeaponCollision.";
    std::size_t used = std::wcslen(section);
    for (std::size_t index = 0;
         profileName[index] != '\0' && used + 1 < std::size(section);
         ++index) {
        section[used++] = static_cast<unsigned char>(profileName[index]);
    }
    section[used] = L'\0';

    wchar_t value[32]{};
    const auto hasValue = [&](const wchar_t* name) noexcept {
        value[0] = L'\0';
        return g_vrSettingsFilePresent &&
               g_vrSettingsPath[0] != L'\0' &&
               GetPrivateProfileStringW(
                   section, name, L"", value,
                   static_cast<DWORD>(std::size(value)),
                   g_vrSettingsPath) != 0;
    };
    explicitProfile =
        hasValue(L"LengthScale") || hasValue(L"Width") ||
        hasValue(L"Height") || hasValue(L"ForwardOffset");
    return SanitizeWeaponCollisionProfile({
        ReadVrFloat(
            section, L"LengthScale",
            g_defaultWeaponCollisionProfile.lengthScale),
        ReadVrFloat(
            section, L"Width",
            g_defaultWeaponCollisionProfile.widthMeters),
        ReadVrFloat(
            section, L"Height",
            g_defaultWeaponCollisionProfile.heightMeters),
        ReadVrFloat(
            section, L"ForwardOffset",
            g_defaultWeaponCollisionProfile.forwardOffsetMeters)});
}

void SaveActiveWeaponCollisionProfile() noexcept {
    const WeightedWeaponInputState& weighted = g_weightedWeaponInput;
    if (!weighted.collisionProfileExplicit ||
        !HasCurrentWeaponWeightProfile()) {
        return;
    }
    wchar_t section[128] = L"WeaponCollision.";
    std::size_t used = std::wcslen(section);
    for (std::size_t index = 0;
         weighted.profileName[index] != '\0' &&
         used + 1 < std::size(section); ++index) {
        section[used++] =
            static_cast<unsigned char>(weighted.profileName[index]);
    }
    section[used] = L'\0';
    WriteVrFloat(
        section, L"LengthScale",
        weighted.collisionProfile.lengthScale);
    WriteVrFloat(
        section, L"Width", weighted.collisionProfile.widthMeters);
    WriteVrFloat(
        section, L"Height", weighted.collisionProfile.heightMeters);
    WriteVrFloat(
        section, L"ForwardOffset",
        weighted.collisionProfile.forwardOffsetMeters);
}

void PrepareWeightedWeaponPoses(
    const void* weapon, FearVrPose& aimPose,
    std::uint32_t& aimValidHands, FearVrPose& gripPose,
    std::uint32_t& gripValidHands) noexcept {
    aimPose = g_currentInput.handAimPose[FEARVR_HAND_RIGHT];
    gripPose = g_currentInput.handGripPose[FEARVR_HAND_RIGHT];
    aimValidHands = g_currentInput.aimPoseValidHands;
    gripValidHands = g_currentInput.gripPoseValidHands;

    WeightedWeaponInputState& weighted = g_weightedWeaponInput;
    const LONG resetGeneration = InterlockedCompareExchange(
        &g_trackingResetGeneration, 0, 0);
    if (weighted.weapon != weapon) {
        // Release the outgoing profile without file I/O on this latency-
        // sensitive update path, then load an explicit incoming profile.
        ResetWeaponWeightPair(
            weighted.filters, WeaponWeightResetReason::weaponChanged);
        ResetWeaponRecoil(weighted.recoil);
        ResetWeaponCollisionRuntime(weighted.collision);
        weighted.collisionObstructed = false;
        InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
        g_lastWeaponRecoilTick = 0;
        weighted.weapon = weapon;
        weighted.lastSampleId = 0;
        weighted.lastAimValidTick = 0;
        weighted.lastGripValidTick = 0;
        weighted.profile = LoadWeaponWeightProfile(
            weapon, weighted.profileName);
        weighted.recoilProfile = LoadWeaponRecoilProfile(
            weighted.profileName, weighted.recoilProfileExplicit);
        weighted.collisionProfile = LoadWeaponCollisionProfile(
            weighted.profileName, weighted.collisionProfileExplicit);
    }
    if (weighted.resetGeneration != resetGeneration) {
        ResetWeaponWeightPair(
            weighted.filters,
            weighted.resetGeneration < 0
                ? WeaponWeightResetReason::referenceSpaceChanged
                : WeaponWeightResetReason::teleportedOrRecentered);
        ResetWeaponRecoil(weighted.recoil);
        ResetWeaponCollisionRuntime(weighted.collision);
        weighted.collisionObstructed = false;
        weighted.resetGeneration = resetGeneration;
        weighted.lastSampleId = 0;
        weighted.lastAimValidTick = 0;
        weighted.lastGripValidTick = 0;
    }
    if (!g_weaponWeightEnabled) {
        if (weighted.enabledOnLastUpdate) {
            ResetWeaponWeightPair(
                weighted.filters, WeaponWeightResetReason::enabledChanged);
        }
        weighted.enabledOnLastUpdate = false;
        weighted.recoilOffset = {};
        const LONG pendingRecoilShots =
            InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
        const bool rawAimValid =
            (aimValidHands & FEARVR_HAND_MASK_RIGHT) != 0;
        const bool rawGripValid =
            (gripValidHands & FEARVR_HAND_MASK_RIGHT) != 0;
        if (rawAimValid && rawGripValid && g_weaponRecoilEnabled) {
            UpdateWeaponRecoil(
                weighted.recoil, MonotonicNanoseconds(), true,
                weighted.profile, ActiveWeaponRecoilProfile(),
                pendingRecoilShots > 0
                    ? static_cast<std::uint32_t>(pendingRecoilShots) : 0U,
                weighted.recoilOffset);
            WeaponWeightPose recoilingAim = ToWeaponWeightPose(aimPose);
            WeaponWeightPose recoilingGrip = ToWeaponWeightPose(gripPose);
            ApplyWeaponRecoil(
                weighted.recoilOffset, recoilingAim, recoilingGrip);
            aimPose = FromWeaponWeightPose(recoilingAim);
            gripPose = FromWeaponWeightPose(recoilingGrip);
        } else {
            ResetWeaponRecoil(weighted.recoil);
        }
        return;
    }
    if (!weighted.enabledOnLastUpdate) {
        ResetWeaponWeightPair(
            weighted.filters, WeaponWeightResetReason::enabledChanged);
        weighted.lastSampleId = 0;
    }
    weighted.enabledOnLastUpdate = true;
    const WeaponWeightProfile effectiveProfile = ApplyWeaponWeightPreset(
        weighted.profile, g_weaponWeightPreset);

    const ULONGLONG now = GetTickCount64();
    if (g_currentInput.sampleId != 0 &&
        g_currentInput.sampleId != weighted.lastSampleId) {
        const std::uint64_t timestampNs =
            g_currentInput.predictedDisplayTimeNs != 0
                ? g_currentInput.predictedDisplayTimeNs
                : MonotonicNanoseconds();
        const bool rawAimValid =
            (aimValidHands & FEARVR_HAND_MASK_RIGHT) != 0;
        const bool rawGripValid =
            (gripValidHands & FEARVR_HAND_MASK_RIGHT) != 0;
        WeaponWeightPose filtered;
        if (rawAimValid && UpdateWeaponWeightFilter(
                weighted.filters.aim, ToWeaponWeightPose(aimPose), true,
                timestampNs, true, effectiveProfile, filtered,
                &weighted.aimDiagnostics)) {
            weighted.aimPose = FromWeaponWeightPose(filtered);
            weighted.aimValid = true;
            weighted.lastAimValidTick = now;
        }
        if (rawGripValid && UpdateWeaponWeightFilter(
                weighted.filters.grip, ToWeaponWeightPose(gripPose), true,
                timestampNs, true, effectiveProfile, filtered,
                &weighted.gripDiagnostics)) {
            weighted.gripPose = FromWeaponWeightPose(filtered);
            weighted.gripValid = true;
            weighted.lastGripValidTick = now;
        }
        weighted.lastSampleId = g_currentInput.sampleId;
    }

    const bool aimFresh = weighted.aimValid &&
        now - weighted.lastAimValidTick <= kTrackedPoseGapGraceMilliseconds;
    const bool gripFresh = weighted.gripValid &&
        now - weighted.lastGripValidTick <= kTrackedPoseGapGraceMilliseconds;
    if (aimFresh) {
        aimPose = weighted.aimPose;
        aimValidHands |= FEARVR_HAND_MASK_RIGHT;
    } else {
        aimValidHands &= ~FEARVR_HAND_MASK_RIGHT;
        if (weighted.aimValid) {
            ClearWeaponWeightFilter(
                weighted.filters.aim,
                WeaponWeightResetReason::trackingLost);
            weighted.aimValid = false;
        }
    }
    if (gripFresh) {
        gripPose = weighted.gripPose;
        gripValidHands |= FEARVR_HAND_MASK_RIGHT;
    } else {
        gripValidHands &= ~FEARVR_HAND_MASK_RIGHT;
        if (weighted.gripValid) {
            ClearWeaponWeightFilter(
                weighted.filters.grip,
                WeaponWeightResetReason::trackingLost);
            weighted.gripValid = false;
        }
    }

    const LONG pendingRecoilShots =
        InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
    weighted.recoilOffset = {};
    if (aimFresh && gripFresh) {
        UpdateWeaponRecoil(
            weighted.recoil, MonotonicNanoseconds(),
            g_weaponRecoilEnabled, weighted.profile,
            ActiveWeaponRecoilProfile(),
            pendingRecoilShots > 0
                ? static_cast<std::uint32_t>(pendingRecoilShots) : 0U,
            weighted.recoilOffset);
        WeaponWeightPose recoilingAim = ToWeaponWeightPose(aimPose);
        WeaponWeightPose recoilingGrip = ToWeaponWeightPose(gripPose);
        ApplyWeaponRecoil(
            weighted.recoilOffset, recoilingAim, recoilingGrip);
        aimPose = FromWeaponWeightPose(recoilingAim);
        gripPose = FromWeaponWeightPose(recoilingGrip);
    } else {
        ResetWeaponRecoil(weighted.recoil);
    }

    if (g_weaponWeightDiagnosticsEnabled &&
        (weighted.lastDiagnosticTick == 0 ||
         now - weighted.lastDiagnosticTick >= 1000)) {
        weighted.lastDiagnosticTick = now;
        char message[448]{};
        std::snprintf(
            message, sizeof(message),
            "profile=%s valid=%d pos_error=%.2fcm angle_error=%.2fdeg "
            "position_omega=%.2f rotation_omega=%.2f "
            "linear_velocity=(%.3f,%.3f,%.3f) "
            "angular_velocity=(%.3f,%.3f,%.3f) "
            "recoil=(%.2fcm,%.2fdeg) reset=%u",
            weighted.profileName, aimFresh && gripFresh ? 1 : 0,
            weighted.aimDiagnostics.positionalErrorMeters * 100.0F,
            weighted.aimDiagnostics.angularErrorRadians *
                (180.0F / 3.14159265358979323846F),
            weighted.aimDiagnostics.effectivePositionOmega,
            weighted.aimDiagnostics.effectiveRotationOmega,
            weighted.aimDiagnostics.linearVelocity.x,
            weighted.aimDiagnostics.linearVelocity.y,
            weighted.aimDiagnostics.linearVelocity.z,
            weighted.aimDiagnostics.angularVelocity.x,
            weighted.aimDiagnostics.angularVelocity.y,
            weighted.aimDiagnostics.angularVelocity.z,
            weighted.recoilOffset.backwardMeters * 100.0F,
            weighted.recoilOffset.pitchRadians *
                (180.0F / 3.14159265358979323846F),
            static_cast<unsigned>(weighted.aimDiagnostics.resetReason));
        Report("INFO", "weapon_weight_diagnostics", message);
    }
}
// Nur "Dual Pistols" besitzt in der ausgelieferten Datenbank ein linkes
// HHModel. Normale Einhandwaffen liefern hier nullptr.
HOBJECT RetailLeftWeaponModel(const void* weapon) noexcept {
    if (weapon == nullptr) {
        return nullptr;
    }
    HOBJECT model = nullptr;
    __try {
        model = *reinterpret_cast<HOBJECT const*>(
            static_cast<const unsigned char*>(weapon) +
            kRetailLeftWeaponModelObjectOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return model;
}

bool RetailDualPistolsActive(const void* weapon) noexcept {
    const HOBJECT right = RetailRightWeaponModel(weapon);
    const HOBJECT left = RetailLeftWeaponModel(weapon);
    return right != nullptr && left != nullptr && left != right;
}

bool RetailWeaponFiresLeft(const void* weapon) noexcept {
    if (!RetailDualPistolsActive(weapon)) {
        return false;
    }
    __try {
        return *(static_cast<const unsigned char*>(weapon) +
                 kRetailWeaponFireLeftHandOffset) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void UpdateRetailDualPistolState(const void* weapon) noexcept {
    HOBJECT leftModel = nullptr;
    if (RetailDualPistolsActive(weapon)) {
        leftModel = RetailLeftWeaponModel(weapon);
    }
    if (leftModel == g_weaponAim.leftWeaponModel) {
        return;
    }

    g_weaponAim.leftWeaponModel = leftModel;
    g_weaponAim.leftWeaponTransformValid = false;
    g_weaponAim.leftMuzzleValid = false;
    g_weaponAim.leftMuzzleLocalValid = false;
    if (leftModel != nullptr &&
        InterlockedCompareExchange(
            &g_dualPistolsActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "dual_pistols_vr_active",
            "Retail Dual Pistols use independent weapon and support-hand "
            "poses, muzzle origins, aim guides and firing-hand haptics.");
    }
}

#if 0
// Removed custom prop interaction. Physically attaching or pushing Retail
// level props from tracked hands/weapons caused unstable rotations and could
// trigger breakable glass. Keep the retired implementation excluded from the
// binary until the surrounding Retail-physics reverse engineering is removed.
constexpr float kHandPhysicsContactRadiusUnits = 6.0F;
constexpr float kHandPhysicsPalmForwardUnits = 4.0F;
constexpr float kHandPhysicsColliderRadiusUnits = 4.5F;
constexpr float kHandPhysicsColliderBackUnits = 2.0F;
constexpr float kHandPhysicsColliderForwardUnits = 7.0F;
// Retail's legacy values are deliberately much larger than real kilograms:
// the tested drinks can reports 30. A movable hammer can therefore sit well
// above the previous 50/120 limits even though bullets move it easily.
constexpr float kHandPhysicsMaximumContactMassKg = 5000.0F;
// Several compact bullet-reactive tools use unusually high Havok mass
// values. Their size/mobility is represented by an unpinned solid body, so
// accepting that body is safer than treating the raw number as kilograms.
constexpr float kHandPhysicsMaximumRigidBodyMassKg = 100000.0F;
constexpr ULONGLONG kHandPhysicsContactPulseGapMs = 65;
// Retail configures this user group for the local melee collider. It collides
// with debris, pickups, ragdolls and world geometry without affecting the
// player controller.
constexpr EPhysicsGroup kHandPhysicsCollisionGroup =
    static_cast<EPhysicsGroup>(ePhysicsGroup_UserStart + 7);
constexpr std::uint32_t kFearUserFlagMoveable = 1U << 5;
constexpr std::uint32_t kFearUserFlagCharacter = 1U << 7;
constexpr std::uint32_t kFearUserFlagHitbox = 1U << 11;

struct HandPhysicsQuery {
    ILTPhysicsSim* physics{nullptr};
    LTVector center;
    float maximumMassKg{0.0F};
    std::uint32_t hand{FEARVR_HAND_LEFT};
    HPHYSICSRIGIDBODY body{INVALID_PHYSICS_RIGID_BODY};
    HOBJECT object{nullptr};
    LTVector centerOfMass;
    LTVector closestPoint;
    float massKg{0.0F};
    float distanceSquared{(std::numeric_limits<float>::max)()};
    bool legacyObject{false};
};

class HandPhysicsShapeDistance final : public IShapeTraversalCallback {
public:
    explicit HandPhysicsShapeDistance(
        const LTVector& point) noexcept
        : point_(point) {}

    void HandleSphere(
        const LTVector& center, float radius, float, float) override {
        LTVector direction = point_ - center;
        const float length = direction.Mag();
        if (!std::isfinite(length) || !std::isfinite(radius) ||
            radius < 0.0F) {
            return;
        }
        if (length <= radius) {
            Consider(point_);
            return;
        }
        direction /= length;
        Consider(center + direction * radius);
    }

    void HandleCapsule(
        const LTVector& point1, const LTVector& point2, float radius,
        float, float) override {
        const LTVector segment = point2 - point1;
        const float segmentLengthSquared = segment.MagSqr();
        float along = 0.0F;
        if (segmentLengthSquared > 0.0001F) {
            along = std::clamp(
                (point_ - point1).Dot(segment) /
                    segmentLengthSquared,
                0.0F, 1.0F);
        }
        const LTVector axisPoint = point1 + segment * along;
        LTVector direction = point_ - axisPoint;
        const float length = direction.Mag();
        if (!std::isfinite(length) || !std::isfinite(radius) ||
            radius < 0.0F) {
            return;
        }
        if (length <= radius) {
            Consider(point_);
            return;
        }
        direction /= length;
        Consider(axisPoint + direction * radius);
    }

    void HandleOBB(
        const LTRigidTransform& transform, const LTVector& halfDims,
        float, float) override {
        const LTRotation inverse =
            transform.m_rRot.Conjugate();
        const LTVector local = inverse.RotateVector(
            point_ - transform.m_vPos);
        const LTVector closestLocal(
            std::clamp(local.x, -std::fabs(halfDims.x),
                       std::fabs(halfDims.x)),
            std::clamp(local.y, -std::fabs(halfDims.y),
                       std::fabs(halfDims.y)),
            std::clamp(local.z, -std::fabs(halfDims.z),
                       std::fabs(halfDims.z)));
        Consider(
            transform.m_vPos +
            transform.m_rRot.RotateVector(closestLocal));
    }

    bool valid() const noexcept {
        return valid_;
    }

    float distanceSquared() const noexcept {
        return distanceSquared_;
    }

    const LTVector& closestPoint() const noexcept {
        return closestPoint_;
    }

private:
    void Consider(const LTVector& candidate) noexcept {
        const float distanceSquared =
            (candidate - point_).MagSqr();
        if (!std::isfinite(distanceSquared) ||
            distanceSquared >= distanceSquared_) {
            return;
        }
        closestPoint_ = candidate;
        distanceSquared_ = distanceSquared;
        valid_ = true;
    }

    LTVector point_;
    LTVector closestPoint_;
    float distanceSquared_{
        (std::numeric_limits<float>::max)()};
    bool valid_{false};
};

bool MeasureHandPhysicsShapeDistance(
    ILTPhysicsSim* physics, HPHYSICSRIGIDBODY body,
    HPHYSICSSHAPE shape, const LTVector& center,
    LTVector& closestPoint, float& distanceSquared) noexcept {
    if (physics == nullptr ||
        body == INVALID_PHYSICS_RIGID_BODY ||
        shape == INVALID_PHYSICS_SHAPE) {
        return false;
    }
    LTRigidTransform bodyTransform;
    HandPhysicsShapeDistance shapeDistance(center);
    if (physics->GetRigidBodyTransform(
            body, bodyTransform) != LT_OK ||
        physics->TraverseShapeHeirarchy(
            shape, &shapeDistance, bodyTransform) != LT_OK ||
        !shapeDistance.valid()) {
        return false;
    }
    closestPoint = shapeDistance.closestPoint();
    distanceSquared = shapeDistance.distanceSquared();
    return true;
}

bool IsHandPhysicsObjectExcluded(HOBJECT object) noexcept {
    if (object == nullptr || object == g_playerBodyObject ||
        object == g_playerCameraObject || object == g_flashlightModel ||
        object == g_flashlightLight ||
        object == RetailRightWeaponModel(g_weaponAim.retailWeapon) ||
        object == g_weaponAim.leftWeaponModel) {
        return true;
    }
    return false;
}

void ConsiderHandPhysicsBody(
    HandPhysicsQuery& query, HOBJECT object,
    HPHYSICSRIGIDBODY body) noexcept {
    if (body == INVALID_PHYSICS_RIGID_BODY ||
        query.physics == nullptr) {
        return;
    }

    bool keepReference = false;
    __try {
        bool pinned = true;
        bool solid = false;
        if (query.physics->IsRigidBodyPinned(body, pinned) != LT_OK ||
            pinned ||
            query.physics->GetRigidBodySolid(body, solid) != LT_OK ||
            !solid) {
            query.physics->ReleaseRigidBody(body);
            return;
        }

        HPHYSICSSHAPE shape = INVALID_PHYSICS_SHAPE;
        float massKg = 0.0F;
        if (query.physics->GetRigidBodyShape(body, shape) != LT_OK ||
            shape == INVALID_PHYSICS_SHAPE) {
            query.physics->ReleaseRigidBody(body);
            return;
        }
        const LTRESULT massResult =
            query.physics->GetShapeMass(shape, massKg);
        if (massResult != LT_OK || !std::isfinite(massKg) ||
            massKg <= 0.0F ||
            massKg > kHandPhysicsMaximumRigidBodyMassKg) {
            query.physics->ReleaseShape(shape);
            query.physics->ReleaseRigidBody(body);
            return;
        }

        LTVector centerOfMass;
        if (query.physics->GetRigidBodyCenterOfMassInWorld(
                body, centerOfMass) != LT_OK) {
            query.physics->ReleaseShape(shape);
            query.physics->ReleaseRigidBody(body);
            return;
        }
        LTVector closestPoint = centerOfMass;
        float distanceSquared =
            (centerOfMass - query.center).MagSqr();
        MeasureHandPhysicsShapeDistance(
            query.physics, body, shape, query.center,
            closestPoint, distanceSquared);
        query.physics->ReleaseShape(shape);
        if (!std::isfinite(distanceSquared) ||
            distanceSquared >= query.distanceSquared) {
            query.physics->ReleaseRigidBody(body);
            return;
        }

        if (query.body != INVALID_PHYSICS_RIGID_BODY) {
            query.physics->ReleaseRigidBody(query.body);
        }
        query.body = body;
        query.object = object;
        query.centerOfMass = centerOfMass;
        query.closestPoint = closestPoint;
        // Use a gameplay interaction mass for the hand impulse. The physical
        // body keeps its real Havok mass; this value only prevents a compact
        // high-mass tool from being treated like immovable furniture.
        query.massKg = std::clamp(massKg, 1.0F, 250.0F);
        query.distanceSquared = distanceSquared;
        query.legacyObject = false;
        keepReference = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    if (!keepReference) {
        // Every normal rejection path released above. Reaching this point by
        // structured exception means the engine handle may already be stale;
        // invoking it again would turn a recoverable world transition into a
        // crash, so deliberately forget that transient reference.
    }
}

void ConsiderLegacyHandPhysicsObject(
    HandPhysicsQuery& query, HOBJECT object,
    bool explicitlyMoveable) noexcept {
    if (g_client == nullptr || object == nullptr ||
        (query.object == object &&
         query.body != INVALID_PHYSICS_RIGID_BODY)) {
        return;
    }

    __try {
        ILTPhysics* const physics = g_client->Physics();
        if (physics == nullptr) {
            return;
        }
        LTVector position;
        LTVector halfDims;
        float massKg = 0.0F;
        if (g_client->GetObjectPos(object, &position) != LT_OK) {
            return;
        }

        bool dimsValid =
            physics->GetObjectDims(object, &halfDims) == LT_OK &&
            std::isfinite(halfDims.x) &&
            std::isfinite(halfDims.y) &&
            std::isfinite(halfDims.z) &&
            halfDims.MagSqr() > 0.01F;
        if (!dimsValid && explicitlyMoveable) {
            ILTModel* const model = g_client->GetModelLT();
            if (model != nullptr) {
                dimsValid = model->GetModelAnimUserDims(
                                object,
                                g_client->GetModelAnimation(object),
                                &halfDims) == LT_OK &&
                            std::isfinite(halfDims.x) &&
                            std::isfinite(halfDims.y) &&
                            std::isfinite(halfDims.z) &&
                            halfDims.MagSqr() > 0.01F;
            }
        }
        if (!dimsValid) {
            return;
        }

        const LTRESULT massResult =
            physics->GetMass(object, &massKg);
        if (massResult != LT_OK || !std::isfinite(massKg) ||
            massKg <= 0.0F) {
            if (!explicitlyMoveable) {
                return;
            }
            // Some old Prop records expose their bullet-reactive MOVEABLE
            // state but no useful client mass. A hammer is one such case.
            massKg = 50.0F;
        }
        const float maximumHalfExtent = (std::max)(
            (std::max)(
                std::fabs(halfDims.x),
                std::fabs(halfDims.y)),
            std::fabs(halfDims.z));
        constexpr float kSmallMoveableMaximumHalfExtent = 64.0F;
        constexpr float kSmallMoveableMaximumLegacyMass = 100000.0F;
        if (massKg > query.maximumMassKg &&
            (!explicitlyMoveable ||
             maximumHalfExtent >
                 kSmallMoveableMaximumHalfExtent ||
             massKg > kSmallMoveableMaximumLegacyMass)) {
            return;
        }

        LTRigidTransform transform;
        if (g_client->GetObjectTransform(
                object, &transform) != LT_OK) {
            transform.m_vPos = position;
            transform.m_rRot.Init();
        }
        const LTVector local =
            transform.m_rRot.Conjugate().RotateVector(
                query.center - transform.m_vPos);
        const LTVector closestLocal(
            std::clamp(
                local.x, -std::fabs(halfDims.x),
                std::fabs(halfDims.x)),
            std::clamp(
                local.y, -std::fabs(halfDims.y),
                std::fabs(halfDims.y)),
            std::clamp(
                local.z, -std::fabs(halfDims.z),
                std::fabs(halfDims.z)));
        const LTVector closestPoint =
            transform.m_vPos +
            transform.m_rRot.RotateVector(closestLocal);
        const float distanceSquared =
            (closestPoint - query.center).MagSqr();
        if (!std::isfinite(distanceSquared) ||
            distanceSquared >= query.distanceSquared) {
            return;
        }

        if (query.body != INVALID_PHYSICS_RIGID_BODY) {
            query.physics->ReleaseRigidBody(query.body);
            query.body = INVALID_PHYSICS_RIGID_BODY;
        }
        query.object = object;
        query.centerOfMass = position;
        query.closestPoint = closestPoint;
        // Legacy mass values are gameplay-scaled. Cap only the interaction
        // mass so a small but numerically heavy hammer still follows a hand
        // impulse; the size limit above prevents furniture from qualifying.
        query.massKg = std::clamp(massKg, 1.0F, 250.0F);
        query.distanceSquared = distanceSquared;
        query.legacyObject = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void QueryHandPhysicsObject(
    HOBJECT object, void* userData) {
    auto* const query = static_cast<HandPhysicsQuery*>(userData);
    if (query == nullptr || query->physics == nullptr ||
        IsHandPhysicsObjectExcluded(object) || g_client == nullptr) {
        return;
    }

    __try {
        ILTCommon* const common = g_client->Common();
        if (common == nullptr) {
            return;
        }

        std::uint32_t userFlags = 0;
        if (common->GetObjectFlags(
                object, OFT_User, userFlags) == LT_OK &&
            (userFlags &
             (kFearUserFlagCharacter | kFearUserFlagHitbox)) != 0) {
            return;
        }
        const bool legacyMoveable =
            (userFlags & kFearUserFlagMoveable) != 0;

        std::uint32_t objectType = OT_NORMAL;
        if (common->GetObjectType(object, &objectType) != LT_OK) {
            return;
        }
        if (objectType == OT_WORLDMODEL) {
            HPHYSICSRIGIDBODY body = INVALID_PHYSICS_RIGID_BODY;
            if (query->physics->GetWorldModelRigidBody(
                    object, body) == LT_OK) {
                ConsiderHandPhysicsBody(*query, object, body);
            }
            if (legacyMoveable) {
                ConsiderLegacyHandPhysicsObject(
                    *query, object, true);
            }
            return;
        }
        if (objectType != OT_MODEL) {
            return;
        }

        std::uint32_t bodyCount = 0;
        const bool rigidBodyQuerySucceeded =
            query->physics->GetNumModelRigidBodies(
                object, bodyCount) == LT_OK;
        if (rigidBodyQuerySucceeded) {
            bodyCount = (std::min)(bodyCount, 32U);
            for (std::uint32_t index = 0; index < bodyCount; ++index) {
                HPHYSICSRIGIDBODY body = INVALID_PHYSICS_RIGID_BODY;
                if (query->physics->GetModelRigidBody(
                        object, index, body) == LT_OK) {
                    ConsiderHandPhysicsBody(*query, object, body);
                }
            }
        }
        // A number of ordinary level props (including some tools) predate
        // Havok and omit USRFLG_MOVEABLE even though they have mass, solid
        // dimensions and react to bullets. Treat model objects without any
        // Havok body as legacy physics candidates; the mass/type/character
        // checks above still reject scenery, actors and hitboxes.
        if (legacyMoveable ||
            !rigidBodyQuerySucceeded || bodyCount == 0) {
            ConsiderLegacyHandPhysicsObject(
                *query, object, legacyMoveable);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool FindNearestHandPhysicsBody(
    std::uint32_t hand, const LTVector& position, float radius,
    float maximumMassKg, HandPhysicsQuery& result) noexcept {
    if (g_client == nullptr || hand >= FEARVR_HAND_COUNT) {
        return false;
    }
    __try {
        result.physics = g_client->PhysicsSim();
        result.center = position;
        result.maximumMassKg = maximumMassKg;
        result.hand = hand;
        result.distanceSquared = radius * radius;
        if (result.physics == nullptr ||
            g_client->FindObjectsInBox(
                position, LTVector(radius, radius, radius),
                &QueryHandPhysicsObject, &result) != LT_OK) {
            if (result.physics != nullptr &&
                result.body != INVALID_PHYSICS_RIGID_BODY) {
                result.physics->ReleaseRigidBody(result.body);
                result.body = INVALID_PHYSICS_RIGID_BODY;
            }
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.body = INVALID_PHYSICS_RIGID_BODY;
        return false;
    }
    return result.body != INVALID_PHYSICS_RIGID_BODY ||
           result.legacyObject && result.object != nullptr;
}

void RequestHandPhysicsHaptic(
    std::uint32_t hand, float amplitude,
    std::uint64_t durationNanoseconds) noexcept {
    if (hand >= FEARVR_HAND_COUNT ||
        g_submitHapticRequest == nullptr ||
        !g_controllerHapticsEnabled) {
        return;
    }
    FearVrHapticRequest request{};
    request.requestId = ++g_hapticRequestId;
    request.durationNs = durationNanoseconds;
    request.amplitude = std::clamp(amplitude, 0.0F, 1.0F);
    request.frequency = 0.0F;
    const bool semanticLeft = hand == FEARVR_HAND_LEFT;
    const bool physicalLeft =
        g_leftHandedBindings ? !semanticLeft : semanticLeft;
    request.handMask = physicalLeft
        ? FEARVR_HAND_MASK_LEFT
        : FEARVR_HAND_MASK_RIGHT;
    request.flags = FEARVR_HF_VALID;
    g_submitHapticRequest(&request);
}

void ResetHandPhysicsState(
    HandPhysicsState& state) noexcept {
    state.contactBody = INVALID_PHYSICS_RIGID_BODY;
    state.lastPosition.Init();
    state.velocity.Init();
    state.lastPoseTick = 0;
    state.lastContactHapticTick = 0;
    state.contactBodyRetryAt = 0;
    state.poseValid = false;
}

void ReleaseHandPhysicsContactBody(
    std::uint32_t hand) noexcept {
    if (hand >= FEARVR_HAND_COUNT) {
        return;
    }
    HandPhysicsState& state = g_handPhysics[hand];
    const HPHYSICSRIGIDBODY body = state.contactBody;
    state.contactBody = INVALID_PHYSICS_RIGID_BODY;
    if (body == INVALID_PHYSICS_RIGID_BODY || g_client == nullptr) {
        return;
    }
    __try {
        ILTPhysicsSim* const physics = g_client->PhysicsSim();
        if (physics != nullptr) {
            physics->ReleaseRigidBody(body);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool UpdateHandPhysicsContactBody(
    HandPhysicsState& state,
    const LTRigidTransform& handTransform,
    ULONGLONG now) noexcept {
    if (g_client == nullptr || now < state.contactBodyRetryAt) {
        return state.contactBody != INVALID_PHYSICS_RIGID_BODY;
    }
    __try {
        ILTPhysicsSim* const physics = g_client->PhysicsSim();
        if (physics == nullptr) {
            state.contactBodyRetryAt = now + 500;
            return false;
        }
        if (state.contactBody == INVALID_PHYSICS_RIGID_BODY) {
            HPHYSICSSHAPE shape = physics->CreateCapsuleShape(
                LTVector(
                    0.0F, 0.0F,
                    -kHandPhysicsColliderBackUnits),
                LTVector(
                    0.0F, 0.0F,
                    kHandPhysicsColliderForwardUnits),
                kHandPhysicsColliderRadiusUnits,
                1.0F, 1000.0F);
            if (shape == INVALID_PHYSICS_SHAPE) {
                state.contactBodyRetryAt = now + 500;
                return false;
            }
            const HPHYSICSRIGIDBODY body =
                physics->CreateRigidBody(
                    shape, handTransform, true,
                    kHandPhysicsCollisionGroup, 0,
                    0.65F, 0.0F);
            physics->ReleaseShape(shape);
            if (body == INVALID_PHYSICS_RIGID_BODY) {
                state.contactBodyRetryAt = now + 500;
                return false;
            }
            state.contactBody = body;
            physics->EnableRigidBodyPinnedCollisions(body, true);
            if (InterlockedCompareExchange(
                    &g_handPhysicsColliderActiveLogged, 1, 0) == 0) {
                Report(
                    "INFO", "hand_physics_colliders_active",
                    "Pinned palm/finger capsules follow both OpenXR grip "
                    "poses and physically collide with movable Havok props.");
            }
            return true;
        }

        const float frameSeconds =
            std::clamp(g_client->GetFrameTime(), 0.005F, 0.05F);
        if (physics->KeyframeRigidBody(
                state.contactBody, handTransform,
                frameSeconds) == LT_OK) {
            return true;
        }
        physics->ReleaseRigidBody(state.contactBody);
        state.contactBody = INVALID_PHYSICS_RIGID_BODY;
        state.contactBodyRetryAt = now + 250;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // A level transition can invalidate the whole client simulation.
        // Forget the old-world handle and create a fresh collider later.
        state.contactBody = INVALID_PHYSICS_RIGID_BODY;
        state.contactBodyRetryAt = now + 250;
    }
    return false;
}

void ClearLegacyPropSimulation(
    LegacyPropSimulationState& state) noexcept {
    state = LegacyPropSimulationState{};
}

void ForgetAllLegacyPropSimulations() noexcept {
    for (LegacyPropSimulationState& state :
         g_legacyPropSimulation) {
        ClearLegacyPropSimulation(state);
    }
}

LTVector LegacyPropRollingAngularVelocity(
    HOBJECT object, const LTVector& linearVelocity) noexcept {
    if (g_client == nullptr || object == nullptr) {
        return LTVector(0.0F, 0.0F, 0.0F);
    }
    float radius = 4.0F;
    __try {
        ILTPhysics* const physics = g_client->Physics();
        LTVector halfDims;
        if (physics != nullptr &&
            physics->GetObjectDims(object, &halfDims) == LT_OK) {
            radius = (std::max)(
                2.0F, (std::min)(
                          std::fabs(halfDims.x),
                          std::fabs(halfDims.z)));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return LTVector(
        linearVelocity.z / radius, 0.0F,
        -linearVelocity.x / radius);
}

void WakeLegacyPropSimulation(
    HOBJECT object, const LTVector& angularVelocity,
    ULONGLONG now, bool replaceAngularVelocity) noexcept {
    if (g_client == nullptr || object == nullptr) {
        return;
    }

    LegacyPropSimulationState* selected = nullptr;
    LegacyPropSimulationState* oldestResting = nullptr;
    LegacyPropSimulationState* oldest = nullptr;
    for (LegacyPropSimulationState& state :
         g_legacyPropSimulation) {
        if (state.object == object) {
            selected = &state;
            break;
        }
        if (state.object == nullptr && selected == nullptr) {
            selected = &state;
        }
        if (state.resting &&
            (oldestResting == nullptr ||
             state.lastTick < oldestResting->lastTick)) {
            oldestResting = &state;
        }
        if (oldest == nullptr || state.lastTick < oldest->lastTick) {
            oldest = &state;
        }
    }

    if (selected != nullptr && selected->object == object) {
        selected->angularVelocity = replaceAngularVelocity
            ? angularVelocity
            : selected->angularVelocity * 0.35F +
                  angularVelocity * 0.65F;
        selected->resting = false;
        selected->restFrames = 0;
        selected->lastTick = now;
        return;
    }
    if (selected == nullptr) {
        selected = oldestResting != nullptr ? oldestResting : oldest;
    }
    if (selected == nullptr) {
        return;
    }

    LTRigidTransform transform;
    bool valid = false;
    __try {
        valid =
            g_client->GetObjectTransform(object, &transform) == LT_OK;
        ILTCommon* const common = g_client->Common();
        if (valid && common != nullptr) {
            // MoveObject only resolves contacts for solid client objects.
            // Preserve every other flag and make just this touched prop solid.
            common->SetObjectFlags(
                object, OFT_Flags, FLAG_SOLID, FLAG_SOLID);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        valid = false;
    }
    if (!valid) {
        return;
    }

    ClearLegacyPropSimulation(*selected);
    selected->object = object;
    selected->position = transform.m_vPos;
    selected->rotation = transform.m_rRot;
    selected->angularVelocity = angularVelocity;
    selected->lastTick = now;
}

void UpdateLegacyPropSimulations(ULONGLONG now) noexcept {
    if (g_client == nullptr) {
        return;
    }

    __try {
        ILTClientPhysics* const physics =
            static_cast<ILTClientPhysics*>(g_client->Physics());
        if (physics == nullptr) {
            return;
        }

        for (LegacyPropSimulationState& state :
             g_legacyPropSimulation) {
            if (state.object == nullptr) {
                continue;
            }

            LTRigidTransform observed;
            LTVector observedVelocity;
            if (g_client->GetObjectTransform(
                    state.object, &observed) != LT_OK ||
                physics->GetVelocity(
                    state.object, &observedVelocity) != LT_OK) {
                ClearLegacyPropSimulation(state);
                continue;
            }

            // A resting object may still receive a native bullet/explosion
            // impulse. Adopt that new motion, but otherwise keep our stored
            // client pose so ordinary server snapshots cannot snap a thrown
            // prop back to where it was grabbed.
            if (state.resting &&
                observedVelocity.MagSqr() > 16.0F) {
                state.position = observed.m_vPos;
                state.rotation = observed.m_rRot;
                state.angularVelocity =
                    LegacyPropRollingAngularVelocity(
                        state.object, observedVelocity);
                state.resting = false;
                state.restFrames = 0;
            }

            if (physics->MoveObject(
                    state.object, state.position,
                    MOVEOBJECT_TELEPORT) != LT_OK ||
                g_client->SetObjectRotation(
                    state.object, state.rotation) != LT_OK) {
                ClearLegacyPropSimulation(state);
                continue;
            }

            if (state.resting) {
                observedVelocity.Init();
                physics->SetVelocity(
                    state.object, observedVelocity);
                state.lastTick = now;
                continue;
            }

            float frameSeconds = g_client->GetFrameTime();
            if (state.lastTick != 0 && now > state.lastTick) {
                frameSeconds = static_cast<float>(
                    now - state.lastTick) * 0.001F;
            }
            frameSeconds =
                std::clamp(frameSeconds, 0.005F, 0.05F);
            state.lastTick = now;

            std::uint32_t objectFlags = 0;
            bool engineAppliesGravity = false;
            ILTCommon* const common = g_client->Common();
            if (common != nullptr &&
                common->GetObjectFlags(
                    state.object, OFT_Flags,
                    objectFlags) == LT_OK) {
                engineAppliesGravity =
                    (objectFlags & FLAG_GRAVITY) != 0;
            }
            if (!engineAppliesGravity) {
                LTVector gravity;
                LTVector velocity;
                if (physics->GetGlobalForce(gravity) == LT_OK &&
                    physics->GetVelocity(
                        state.object, &velocity) == LT_OK) {
                    velocity += gravity * frameSeconds;
                    physics->SetVelocity(state.object, velocity);
                }
            }

            MoveInfo movement{};
            movement.m_hObject = state.object;
            movement.m_dt = frameSeconds;
            if (physics->UpdateMovement(&movement) != LT_OK) {
                ClearLegacyPropSimulation(state);
                continue;
            }

            const LTVector targetPosition =
                state.position + movement.m_Offset;
            if (physics->MoveObject(
                    state.object, targetPosition, 0) != LT_OK ||
                g_client->GetObjectPos(
                    state.object, &state.position) != LT_OK) {
                ClearLegacyPropSimulation(state);
                continue;
            }

            LTVector velocity;
            if (physics->GetVelocity(
                    state.object, &velocity) != LT_OK) {
                ClearLegacyPropSimulation(state);
                continue;
            }
            CollisionInfo standing{};
            standing.m_hObject = nullptr;
            standing.m_hPoly = INVALID_HPOLY;
            const bool standingOnSurface =
                physics->GetStandingOn(
                    state.object, &standing) == LT_OK &&
                (standing.m_hObject != nullptr ||
                 standing.m_hPoly != INVALID_HPOLY);
            const bool downwardMoveBlocked =
                movement.m_Offset.y < -0.001F &&
                state.position.y - targetPosition.y > 0.01F;
            const bool onGround =
                standingOnSurface || downwardMoveBlocked;
            if (onGround && velocity.y < 0.0F) {
                velocity.y = 0.0F;
            }

            if (onGround) {
                const float rollingDamping =
                    std::pow(0.18F, frameSeconds);
                velocity.x *= rollingDamping;
                velocity.z *= rollingDamping;
                const LTVector rollingAngular =
                    LegacyPropRollingAngularVelocity(
                        state.object, velocity);
                state.angularVelocity =
                    state.angularVelocity * 0.35F +
                    rollingAngular * 0.65F;
                state.angularVelocity *= rollingDamping;
            } else {
                state.angularVelocity *=
                    std::pow(0.92F, frameSeconds);
            }
            physics->SetVelocity(state.object, velocity);

            const float angularSpeed =
                state.angularVelocity.Mag();
            if (std::isfinite(angularSpeed) &&
                angularSpeed > 0.001F) {
                const LTVector axis =
                    state.angularVelocity / angularSpeed;
                const LTRotation delta(
                    axis, angularSpeed * frameSeconds);
                state.rotation = delta * state.rotation;
                state.rotation.Normalize();
                g_client->SetObjectRotation(
                    state.object, state.rotation);
            }

            const float horizontalSpeedSquared =
                velocity.x * velocity.x +
                velocity.z * velocity.z;
            if (onGround && horizontalSpeedSquared < 4.0F &&
                std::fabs(velocity.y) < 1.0F &&
                angularSpeed < 0.35F) {
                ++state.restFrames;
            } else {
                state.restFrames = 0;
            }
            if (state.restFrames >= 8) {
                velocity.Init();
                physics->SetVelocity(state.object, velocity);
                state.angularVelocity.Init();
                state.resting = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Level streaming can invalidate several client object handles at
        // once. The world-change path clears them before the next frame.
    }
}

void ReleaseHandPhysicsGrab(
    std::uint32_t hand, bool applyThrow) noexcept {
    if (hand >= FEARVR_HAND_COUNT) {
        return;
    }
    HandPhysicsState& state = g_handPhysics[hand];
    if (!HandPhysicsGrabActive(hand)) {
        state.squeezeDown = false;
        return;
    }

    HPHYSICSRIGIDBODY body = state.heldBody;
    HOBJECT object = state.heldObject;
    const bool legacyObject = state.heldLegacyObject;
    LTVector releaseVelocity = state.velocity;
    const float speed = releaseVelocity.Mag();
    releaseVelocity *=
        ClampHandPhysicsSpeedScale(speed);
    LTVector releaseAngularVelocity = state.angularVelocity;
    const float angularSpeed = releaseAngularVelocity.Mag();
    if (!applyThrow) {
        releaseVelocity.Init();
        releaseAngularVelocity.Init();
    }
    // Auch ein nahezu bewegungslos losgelassenes Objekt muss aufgeweckt
    // werden. Sonst bleibt insbesondere ein legacy MOVEABLE bis zum naechsten
    // Spieler-/Weltkontakt scheinbar in der Luft stehen.
    LTVector wakeVelocity = releaseVelocity;
    if (wakeVelocity.MagSqr() <= 1.0F) {
        wakeVelocity = LTVector(0.0F, -1.0F, 0.0F);
    }
    state.heldBody = INVALID_PHYSICS_RIGID_BODY;
    state.heldObject = nullptr;
    state.heldLegacyObject = false;

    if (g_client != nullptr) {
        __try {
            if (legacyObject && object != nullptr) {
                ILTPhysics* const physics = g_client->Physics();
                if (physics != nullptr) {
                    physics->SetVelocity(object, wakeVelocity);
                    WakeLegacyPropSimulation(
                        object, releaseAngularVelocity,
                        GetTickCount64(), true);
                }
            } else {
                ILTPhysicsSim* const physics = g_client->PhysicsSim();
                if (physics == nullptr) {
                    __leave;
                }
                physics->SetRigidBodyVelocity(body, wakeVelocity);
                if (applyThrow &&
                    releaseAngularVelocity.MagSqr() > 0.01F) {
                    physics->SetRigidBodyAngularVelocity(
                        body, releaseAngularVelocity);
                }
                physics->ReleaseRigidBody(body);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    char message[160]{};
    std::snprintf(
        message, sizeof(message),
        "hand=%s object=%p type=%s speed=%.1f angular=%.1f",
        hand == FEARVR_HAND_LEFT ? "left" : "right", object,
        legacyObject ? "moveable" : "rigid",
        static_cast<double>(applyThrow ? speed : 0.0F),
        static_cast<double>(applyThrow ? angularSpeed : 0.0F));
    Report("INFO", "hand_physics_released", message);
    RequestHandPhysicsHaptic(hand, 0.08F, 18'000'000);
}

void ReleaseAllHandPhysicsGrabs(bool applyThrow) noexcept {
    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        ReleaseHandPhysicsGrab(hand, applyThrow);
        ReleaseHandPhysicsContactBody(hand);
        ResetHandPhysicsState(g_handPhysics[hand]);
        ResetHandPhysicsState(g_weaponPhysicsContact[hand]);
    }
    ForgetAllLegacyPropSimulations();
}

void ForgetHandPhysicsAfterWorldChange() noexcept {
    for (HandPhysicsState& state : g_handPhysics) {
        // The level owns these bodies and has already destroyed the world.
        // Never call ReleaseRigidBody with a handle from that old world.
        ResetHandPhysicsState(state);
    }
    for (HandPhysicsState& state : g_weaponPhysicsContact) {
        ResetHandPhysicsState(state);
    }
    ForgetAllLegacyPropSimulations();
}

void UpdateHandPhysicsVelocity(
    HandPhysicsState& state, const LTVector& position,
    const LTRotation& rotation,
    ULONGLONG now) noexcept {
    if (!state.poseValid || state.lastPoseTick == 0 ||
        now <= state.lastPoseTick ||
        now - state.lastPoseTick > 100) {
        state.velocity = LTVector(0.0F, 0.0F, 0.0F);
        state.angularVelocity = LTVector(0.0F, 0.0F, 0.0F);
    } else {
        const float seconds =
            static_cast<float>(now - state.lastPoseTick) * 0.001F;
        const LTVector measured =
            (position - state.lastPosition) / seconds;
        if (std::isfinite(measured.x) &&
            std::isfinite(measured.y) &&
            std::isfinite(measured.z)) {
            state.velocity =
                state.velocity * 0.35F + measured * 0.65F;
        } else {
            state.velocity = LTVector(0.0F, 0.0F, 0.0F);
        }

        LTRotation delta =
            rotation * state.lastRotation.Conjugate();
        delta.Normalize();
        float x = delta[LTRotation::QX];
        float y = delta[LTRotation::QY];
        float z = delta[LTRotation::QZ];
        float w = delta[LTRotation::QW];
        if (w < 0.0F) {
            x = -x;
            y = -y;
            z = -z;
            w = -w;
        }
        const float vectorMagnitude =
            std::sqrt(x * x + y * y + z * z);
        if (std::isfinite(vectorMagnitude) &&
            vectorMagnitude > 0.0001F &&
            std::isfinite(w)) {
            const float angle =
                2.0F * std::atan2(
                           vectorMagnitude,
                           std::clamp(w, 0.0F, 1.0F));
            LTVector measuredAngular(
                x / vectorMagnitude * angle / seconds,
                y / vectorMagnitude * angle / seconds,
                z / vectorMagnitude * angle / seconds);
            const float angularSpeed = measuredAngular.Mag();
            if (std::isfinite(angularSpeed)) {
                if (angularSpeed > 20.0F) {
                    measuredAngular *= 20.0F / angularSpeed;
                }
                state.angularVelocity =
                    state.angularVelocity * 0.35F +
                    measuredAngular * 0.65F;
            } else {
                state.angularVelocity.Init();
            }
        } else {
            state.angularVelocity *= 0.35F;
        }
    }
    state.lastPosition = position;
    state.lastRotation = rotation;
    state.lastPoseTick = now;
    state.poseValid = true;
}

bool HandPhysicsCanContact(std::uint32_t hand) noexcept {
    if (!g_handPhysicsEnabled || hand >= FEARVR_HAND_COUNT ||
        g_climbOnLadder) {
        return false;
    }
    const std::uint32_t handMask = hand == FEARVR_HAND_LEFT
        ? FEARVR_HAND_MASK_LEFT
        : FEARVR_HAND_MASK_RIGHT;
    if ((g_currentInput.activeHands & handMask) == 0) {
        return false;
    }
    return hand == FEARVR_HAND_LEFT
        ? g_weaponAim.leftGripValid
        : g_weaponAim.gripValid;
}

bool HandPhysicsCanGrab(
    std::uint32_t hand, const void* weapon) noexcept {
    if (!HandPhysicsCanContact(hand)) {
        return false;
    }
    if (hand == FEARVR_HAND_LEFT) {
        return !DualPistolsVrActive() &&
               !g_twoHandedGripActive;
    }
    return weapon == nullptr || RetailWeaponIsDisabled(weapon);
}

LTRigidTransform HandPhysicsTransform(
    std::uint32_t hand) noexcept {
    if (hand == FEARVR_HAND_LEFT) {
        // The physical palm must occupy the same side of the wrist as the
        // corrected visible hand; otherwise the player touches a prop while
        // the grab volume sits behind it.
        return LTRigidTransform(
            EffectiveLeftHandPosition(),
            EffectiveLeftHandRotation());
    }
    return g_weaponAim.gripTransform;
}

LTVector HandPhysicsPalmCenter(
    const LTRigidTransform& handTransform) noexcept {
    return handTransform.m_vPos +
           handTransform.m_rRot.Forward() *
               kHandPhysicsPalmForwardUnits;
}

void UpdateTrackedPhysicsContact(
    std::uint32_t hand, HandPhysicsState& state,
    const LTVector& contactCenter,
    const LTVector& contactVelocity,
    ULONGLONG now, float contactRadius) noexcept {
    const float handSpeed = contactVelocity.Mag();
    if (!std::isfinite(handSpeed) || handSpeed <= 4.0F) {
        return;
    }

    HandPhysicsQuery hit;
    if (!FindNearestHandPhysicsBody(
            hand, contactCenter,
            contactRadius,
            kHandPhysicsMaximumContactMassKg, hit)) {
        return;
    }

    __try {
        LTVector bodyVelocity(0.0F, 0.0F, 0.0F);
        LTRESULT velocityResult = LT_ERROR;
        if (hit.legacyObject) {
            ILTPhysics* const physics = g_client->Physics();
            if (physics != nullptr) {
                velocityResult =
                    physics->GetVelocity(hit.object, &bodyVelocity);
            }
        } else {
            velocityResult =
                hit.physics->GetRigidBodyVelocity(
                    hit.body, bodyVelocity);
        }
        const LTVector direction = contactVelocity / handSpeed;
        const float relativeSpeed =
            (contactVelocity - bodyVelocity).Dot(direction);
        const float impulseMagnitude =
            HandPhysicsPushImpulseMagnitude(
                hit.massKg, relativeSpeed);
        LTRESULT impulseResult = LT_ERROR;
        if (velocityResult == LT_OK && impulseMagnitude > 0.0F) {
            if (hit.legacyObject) {
                ILTPhysics* const physics = g_client->Physics();
                if (physics != nullptr) {
                    const LTVector velocityDelta =
                        direction *
                        (impulseMagnitude / hit.massKg);
                    const LTVector pushedVelocity =
                        bodyVelocity + velocityDelta;
                    impulseResult = physics->SetVelocity(
                        hit.object, pushedVelocity);
                    if (impulseResult == LT_OK) {
                        WakeLegacyPropSimulation(
                            hit.object,
                            LegacyPropRollingAngularVelocity(
                                hit.object, pushedVelocity),
                            now, false);
                    }
                }
            } else {
                impulseResult =
                    hit.physics->ApplyRigidBodyImpulseWorldSpace(
                        hit.body, hit.closestPoint,
                        direction * impulseMagnitude);
                if (impulseResult == LT_OK) {
                    // A kinematic tracked hand must transfer translation as
                    // well as off-centre torque. Some compact tools (notably
                    // the hammer prop) use a very large Havok mass: an impulse
                    // at the touched edge merely spins them around one axis.
                    // Preserve their current velocity and add the hand's
                    // normal component directly so the body actually moves.
                    const float transferredSpeed = std::clamp(
                        relativeSpeed * 0.65F, 0.0F, 350.0F);
                    const LTVector pushedVelocity =
                        bodyVelocity + direction * transferredSpeed;
                    const LTRESULT velocitySetResult =
                        hit.physics->SetRigidBodyVelocity(
                            hit.body, pushedVelocity);
                    if (velocitySetResult != LT_OK) {
                        impulseResult = velocitySetResult;
                    }
                }
            }
        }
        if (impulseResult == LT_OK) {
            if (state.lastContactHapticTick == 0 ||
                now - state.lastContactHapticTick >=
                    kHandPhysicsContactPulseGapMs) {
                state.lastContactHapticTick = now;
                RequestHandPhysicsHaptic(
                    hand, std::clamp(
                              impulseMagnitude / 900.0F,
                              0.04F, 0.16F),
                    14'000'000);
            }
            if (InterlockedCompareExchange(
                    &g_handPhysicsActiveLogged, 1, 0) == 0) {
                Report(
                    "INFO", "hand_physics_active",
                    "Tracked hands push both Havok rigid bodies and legacy "
                    "F.E.A.R. MOVEABLE props; squeeze near a prop grabs it "
                    "and release velocity is preserved for throwing.");
            }
        }
        if (!hit.legacyObject &&
            hit.body != INVALID_PHYSICS_RIGID_BODY) {
            hit.physics->ReleaseRigidBody(hit.body);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void UpdateOneHandPhysics(
    std::uint32_t hand, const void* weapon,
    ULONGLONG now) noexcept {
    HandPhysicsState& state = g_handPhysics[hand];
    const bool canContact = HandPhysicsCanContact(hand);
    if (!canContact) {
        if (HandPhysicsGrabActive(hand)) {
            ReleaseHandPhysicsGrab(hand, false);
        }
        ReleaseHandPhysicsContactBody(hand);
        state.poseValid = false;
        state.squeezeDown = false;
        return;
    }

    const LTRigidTransform handTransform =
        HandPhysicsTransform(hand);
    UpdateHandPhysicsVelocity(
        state, handTransform.m_vPos,
        handTransform.m_rRot, now);
    if (HandPhysicsGrabActive(hand)) {
        // A held body is keyframed separately. Keeping the physical palm
        // collider active would make both bodies fight at the grab point.
        ReleaseHandPhysicsContactBody(hand);
    } else {
        UpdateHandPhysicsContactBody(
            state, handTransform, now);
    }
    const bool canGrab = HandPhysicsCanGrab(hand, weapon);
    if (!canGrab) {
        if (HandPhysicsGrabActive(hand)) {
            ReleaseHandPhysicsGrab(hand, false);
            UpdateHandPhysicsContactBody(
                state, handTransform, now);
        }
        state.squeezeDown = false;
        UpdateTrackedPhysicsContact(
            hand, state, HandPhysicsPalmCenter(handTransform),
            state.velocity, now,
            kHandPhysicsContactRadiusUnits);
        return;
    }
    const bool squeezeDown = UpdateHandPhysicsSqueeze(
        g_currentInput.squeeze[hand], state.squeezeDown);
    const bool squeezePressed =
        squeezeDown && !state.squeezeDown;
    state.squeezeDown = squeezeDown;

    if (HandPhysicsGrabActive(hand)) {
        if (!squeezeDown) {
            ReleaseHandPhysicsGrab(hand, true);
            return;
        }
        __try {
            const LTRigidTransform desired =
                handTransform * state.bodyInHand;
            if (state.heldLegacyObject) {
                LTRigidTransform currentObject;
                ILTPhysics* const physics = g_client->Physics();
                if (state.heldObject == nullptr || physics == nullptr ||
                    g_client->GetObjectTransform(
                        state.heldObject, &currentObject) != LT_OK ||
                    (desired.m_vPos - currentObject.m_vPos).MagSqr() >
                        10000.0F ||
                    physics->MoveObject(
                        state.heldObject, desired.m_vPos,
                        MOVEOBJECT_TELEPORT) != LT_OK ||
                    g_client->SetObjectRotation(
                        state.heldObject, desired.m_rRot) != LT_OK) {
                    ReleaseHandPhysicsGrab(hand, false);
                }
            } else {
                ILTPhysicsSim* const physics = g_client->PhysicsSim();
                LTRigidTransform currentBody;
                const float frameSeconds =
                    std::clamp(g_client->GetFrameTime(), 0.005F, 0.05F);
                if (physics == nullptr ||
                    physics->GetRigidBodyTransform(
                        state.heldBody, currentBody) != LT_OK ||
                    (desired.m_vPos - currentBody.m_vPos).MagSqr() >
                        10000.0F ||
                    physics->KeyframeRigidBody(
                        state.heldBody, desired,
                        frameSeconds) != LT_OK) {
                    ReleaseHandPhysicsGrab(hand, false);
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // A streaming transition can invalidate the client physics world
            // between two WeaponManager updates. Forget the handle; the world
            // owns its lifetime in this exceptional path.
            state.heldBody = INVALID_PHYSICS_RIGID_BODY;
            state.heldObject = nullptr;
            state.heldLegacyObject = false;
        }
        return;
    }

    if (squeezePressed) {
        HandPhysicsQuery grab;
        const LTVector palmCenter =
            HandPhysicsPalmCenter(handTransform);
        if (FindNearestHandPhysicsBody(
                hand, palmCenter,
                kHandPhysicsGrabRadiusUnits,
                kHandPhysicsMaximumGrabMassKg, grab)) {
            LTRigidTransform bodyTransform;
            bool acquired = false;
            __try {
                acquired = grab.legacyObject
                    ? g_client->GetObjectTransform(
                          grab.object, &bodyTransform) == LT_OK
                    : grab.physics->GetRigidBodyTransform(
                          grab.body, bodyTransform) == LT_OK;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                acquired = false;
            }
            if (acquired) {
                // Preserve orientation, but snap the touched surface point
                // into the palm. Keeping the old object origin relative to
                // the hand preserved the entire search-radius gap and made a
                // prop orbit outside the fingers.
                const LTVector grabPointInBody =
                    bodyTransform.m_rRot.Conjugate().RotateVector(
                        grab.closestPoint -
                        bodyTransform.m_vPos);
                LTRigidTransform heldTransform = bodyTransform;
                heldTransform.m_vPos =
                    palmCenter -
                    heldTransform.m_rRot.RotateVector(
                        grabPointInBody);
                state.heldBody = grab.body;
                state.heldObject = grab.object;
                state.heldLegacyObject = grab.legacyObject;
                state.bodyInHand =
                    handTransform.GetInverse() * heldTransform;
                if (grab.legacyObject) {
                    ForgetLegacyPropSimulation(grab.object);
                }
                ReleaseHandPhysicsContactBody(hand);
                char message[160]{};
                std::snprintf(
                    message, sizeof(message),
                    "hand=%s object=%p type=%s mass_kg=%.2f "
                    "surface_snap_cm=%.1f",
                    hand == FEARVR_HAND_LEFT ? "left" : "right",
                    grab.object,
                    grab.legacyObject ? "moveable" : "rigid",
                    static_cast<double>(grab.massKg),
                    static_cast<double>(
                        std::sqrt(grab.distanceSquared)));
                Report("INFO", "hand_physics_grabbed", message);
                RequestHandPhysicsHaptic(
                    hand, 0.14F, 28'000'000);
                InterlockedExchange(&g_handPhysicsActiveLogged, 1);
                return;
            }
            if (!grab.legacyObject &&
                grab.body != INVALID_PHYSICS_RIGID_BODY) {
                __try {
                    grab.physics->ReleaseRigidBody(grab.body);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }
        } else if (now >= g_handPhysicsMissDiagnosticAt) {
            char message[160]{};
            std::snprintf(
                message, sizeof(message),
                "hand=%s pos=(%.1f,%.1f,%.1f) radius=%.1f",
                hand == FEARVR_HAND_LEFT ? "left" : "right",
                static_cast<double>(handTransform.m_vPos.x),
                static_cast<double>(handTransform.m_vPos.y),
                static_cast<double>(handTransform.m_vPos.z),
                static_cast<double>(kHandPhysicsGrabRadiusUnits));
            Report("INFO", "hand_physics_grip_miss", message);
            g_handPhysicsMissDiagnosticAt = now + 500;
        }
    }

    UpdateTrackedPhysicsContact(
        hand, state, HandPhysicsPalmCenter(handTransform),
        state.velocity, now,
        kHandPhysicsContactRadiusUnits);
}

void UpdateHandPhysics(
    const void* weapon, ULONGLONG now) noexcept {
    // Legacy props do not advance from SetVelocity alone. Restore their local
    // pose before hand queries, then run LithTech's own collision movement so
    // released and touched cans continue falling and rolling without waiting
    // for player locomotion to wake them.
    UpdateLegacyPropSimulations(now);

    UpdateOneHandPhysics(FEARVR_HAND_LEFT, weapon, now);
    UpdateOneHandPhysics(FEARVR_HAND_RIGHT, weapon, now);

    if (weapon != nullptr && !g_weaponDisabled &&
        g_weaponAim.valid) {
        const LTVector rightMuzzle =
            g_weaponAim.muzzleValid
                ? g_weaponAim.muzzleTransform.m_vPos
                : g_weaponAim.fireTransform.m_vPos +
                      g_weaponAim.fireTransform.m_rRot.Forward() *
                          24.0F;
        const LTVector rightContact =
            (g_weaponAim.fireTransform.m_vPos + rightMuzzle) *
            0.5F;
        const float rightContactRadius = std::clamp(
            (rightMuzzle -
             g_weaponAim.fireTransform.m_vPos).Mag() *
                    0.5F +
                4.0F,
            7.0F, 22.0F);
        HandPhysicsState& rightState =
            g_weaponPhysicsContact[FEARVR_HAND_RIGHT];
        UpdateHandPhysicsVelocity(
            rightState, rightContact,
            g_weaponAim.fireTransform.m_rRot, now);
        UpdateTrackedPhysicsContact(
            FEARVR_HAND_RIGHT, rightState, rightContact,
            rightState.velocity, now, rightContactRadius);
    } else {
        g_weaponPhysicsContact[FEARVR_HAND_RIGHT].poseValid =
            false;
    }

    if (weapon != nullptr && !g_weaponDisabled &&
        DualPistolsVrActive() && g_weaponAim.leftAimValid) {
        const LTVector leftMuzzle =
            g_weaponAim.leftMuzzleValid
                ? g_weaponAim.leftMuzzleTransform.m_vPos
                : g_weaponAim.leftAimTransform.m_vPos +
                      g_weaponAim.leftAimTransform.m_rRot.Forward() *
                          24.0F;
        const LTVector leftContact =
            (g_weaponAim.leftAimTransform.m_vPos + leftMuzzle) *
            0.5F;
        const float leftContactRadius = std::clamp(
            (leftMuzzle -
             g_weaponAim.leftAimTransform.m_vPos).Mag() *
                    0.5F +
                4.0F,
            7.0F, 22.0F);
        HandPhysicsState& leftState =
            g_weaponPhysicsContact[FEARVR_HAND_LEFT];
        UpdateHandPhysicsVelocity(
            leftState, leftContact,
            g_weaponAim.leftAimTransform.m_rRot, now);
        UpdateTrackedPhysicsContact(
            FEARVR_HAND_LEFT, leftState, leftContact,
            leftState.velocity, now, leftContactRadius);
    } else {
        g_weaponPhysicsContact[FEARVR_HAND_LEFT].poseValid =
            false;
    }
}
#endif

bool UpdateRetailMuzzlePosition(const void* weapon) noexcept {
    if (weapon != g_weaponAim.muzzleWeapon) {
        g_weaponAim.muzzleWeapon = weapon;
        g_weaponAim.muzzleValid = false;
        g_weaponAim.muzzleDirectionValid = false;
        g_weaponAim.muzzleLocalValid = false;
        g_weaponAim.muzzleDiagnosticLogged = false;
    }
    if (weapon == nullptr) {
        return false;
    }

    HOBJECT weaponModel = RetailRightWeaponModel(weapon);
    HMODELSOCKET muzzleSocket = INVALID_MODEL_SOCKET;
    __try {
        muzzleSocket = *reinterpret_cast<HMODELSOCKET const*>(
            static_cast<const unsigned char*>(weapon) +
            kRetailRightWeaponMuzzleSocketOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (g_client == nullptr || weaponModel == nullptr ||
        muzzleSocket == INVALID_MODEL_SOCKET) {
        return false;
    }

    LTTransform muzzleTransform;
    LTRigidTransform weaponTransform;
    __try {
        ILTModel* const model = g_client->GetModelLT();
        if (model == nullptr ||
            model->GetSocketTransform(
                weaponModel, muzzleSocket, muzzleTransform, true) != LT_OK) {
            return false;
        }
        if (g_client->GetObjectTransform(
                weaponModel, &weaponTransform) != LT_OK) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (!std::isfinite(muzzleTransform.m_vPos.x) ||
        !std::isfinite(muzzleTransform.m_vPos.y) ||
        !std::isfinite(muzzleTransform.m_vPos.z)) {
        return false;
    }

    // Reject unrelated/corrupt data. A first-person muzzle remains close to
    // its tracked controller.
    const LTVector referencePosition = g_weaponAim.gripValid
        ? g_weaponAim.gripTransform.m_vPos
        : g_weaponAim.fireTransform.m_vPos;
    const LTVector separation =
        muzzleTransform.m_vPos - referencePosition;
    if (muzzleTransform.m_vPos.MagSqr() < 0.0001F ||
        separation.MagSqr() > 40000.0F) {
        return false;
    }

    g_weaponAim.muzzleTransform.m_vPos = muzzleTransform.m_vPos;
    g_weaponAim.muzzleTransform.m_rRot = muzzleTransform.m_rRot;
    const LTRotation weaponRotationInverse =
        weaponTransform.m_rRot.Conjugate();
    g_weaponAim.muzzleForwardInWeapon =
        weaponRotationInverse.RotateVector(
            muzzleTransform.m_rRot.Forward());
    if (g_weaponAim.muzzleForwardInWeapon.MagSqr() > 0.0001F) {
        g_weaponAim.muzzleForwardInWeapon.Normalize();
        g_weaponAim.muzzleDirectionValid = true;
    }
    // Starrer Sockelversatz im Waffenraum. Er ist unabhängig davon, ob die
    // Engine gerade die animierte Retail-Pose oder unsere Controllerpose auf
    // dem Waffenobjekt stehen hat, weil beide Werte im selben Moment gelesen
    // werden.
    g_weaponAim.muzzleOffsetInWeapon =
        weaponRotationInverse.RotateVector(
            muzzleTransform.m_vPos - weaponTransform.m_vPos);
    g_weaponAim.muzzleRotationInWeapon =
        weaponRotationInverse * muzzleTransform.m_rRot;
    g_weaponAim.muzzleLocalValid = true;
    g_weaponAim.muzzleValid = true;
    if (!g_weaponAim.muzzleDiagnosticLogged) {
        LTVector aimForward =
            g_weaponAim.fireTransform.m_rRot.Forward();
        LTVector muzzleForward =
            muzzleTransform.m_rRot.Forward();
        aimForward.y = 0.0F;
        muzzleForward.y = 0.0F;
        if (aimForward.MagSqr() > 0.0001F &&
            muzzleForward.MagSqr() > 0.0001F) {
            aimForward.Normalize();
            muzzleForward.Normalize();
            const float dot = (std::max)(
                -1.0F, (std::min)(
                    1.0F, aimForward.Dot(muzzleForward)));
            const float crossY =
                aimForward.z * muzzleForward.x -
                aimForward.x * muzzleForward.z;
            const float yawDegrees =
                std::atan2(crossY, dot) * 57.29577951308232F;
            const LTVector muzzleOffset =
                muzzleTransform.m_vPos -
                g_weaponAim.gripTransform.m_vPos;
            char message[192]{};
            std::snprintf(
                message, sizeof(message),
                "Measured muzzle yaw from controller: %.2f deg; "
                "grip-to-muzzle offset: (%.2f, %.2f, %.2f).",
                yawDegrees, muzzleOffset.x, muzzleOffset.y,
                muzzleOffset.z);
            Report(
                "INFO", "weapon_pose_diagnostic", message);
            g_weaponAim.muzzleDiagnosticLogged = true;
        }
    }
    return true;
}

bool UpdateRetailLeftMuzzlePosition() noexcept {
    const HOBJECT weaponModel = g_weaponAim.leftWeaponModel;
    if (g_client == nullptr || weaponModel == nullptr) {
        return false;
    }

    HMODELSOCKET muzzleSocket = INVALID_MODEL_SOCKET;
    LTTransform muzzleTransform;
    LTRigidTransform weaponTransform;
    __try {
        ILTModel* const model = g_client->GetModelLT();
        // In the shipped Dual Pistols record, both RDPistol and LDPistol use
        // the explicitly named "Flash" muzzle socket.
        if (model == nullptr ||
            model->GetSocket(
                weaponModel, "Flash", muzzleSocket) != LT_OK ||
            muzzleSocket == INVALID_MODEL_SOCKET ||
            model->GetSocketTransform(
                weaponModel, muzzleSocket,
                muzzleTransform, true) != LT_OK ||
            g_client->GetObjectTransform(
                weaponModel, &weaponTransform) != LT_OK) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (!std::isfinite(muzzleTransform.m_vPos.x) ||
        !std::isfinite(muzzleTransform.m_vPos.y) ||
        !std::isfinite(muzzleTransform.m_vPos.z)) {
        return false;
    }

    const LTVector referencePosition = g_weaponAim.leftGripValid
        ? g_weaponAim.leftGripTransform.m_vPos
        : g_weaponAim.leftAimTransform.m_vPos;
    const LTVector separation =
        muzzleTransform.m_vPos - referencePosition;
    if (muzzleTransform.m_vPos.MagSqr() < 0.0001F ||
        separation.MagSqr() > 40000.0F) {
        return false;
    }

    g_weaponAim.leftMuzzleTransform.m_vPos = muzzleTransform.m_vPos;
    g_weaponAim.leftMuzzleTransform.m_rRot = muzzleTransform.m_rRot;
    const LTRotation weaponRotationInverse =
        weaponTransform.m_rRot.Conjugate();
    g_weaponAim.leftMuzzleOffsetInWeapon =
        weaponRotationInverse.RotateVector(
            muzzleTransform.m_vPos - weaponTransform.m_vPos);
    g_weaponAim.leftMuzzleRotationInWeapon =
        weaponRotationInverse * muzzleTransform.m_rRot;
    g_weaponAim.leftMuzzleLocalValid = true;
    g_weaponAim.leftMuzzleValid = true;
    return true;
}

HOBJECT CurrentRetailPlayerBody() noexcept {
    const HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return nullptr;
    }
    HOBJECT playerBody = nullptr;
    __try {
        playerBody = *reinterpret_cast<HOBJECT*>(
            reinterpret_cast<unsigned char*>(module) +
            kRetailPlayerBodyManagerRva +
            kRetailPlayerBodyObjectOffset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        playerBody = nullptr;
    }
    return playerBody;
}

void EnsureHandNodeControls(HOBJECT playerBody) noexcept;

bool RotationBetweenDirections(
    LTVector from, LTVector to, LTRotation& rotation) noexcept {
    if (from.MagSqr() < 0.0001F || to.MagSqr() < 0.0001F) {
        return false;
    }
    from.Normalize();
    to.Normalize();
    float dot = from.Dot(to);
    dot = (std::max)(-1.0F, (std::min)(1.0F, dot));
    rotation = LTRotation::GetIdentity();
    if (dot > 0.9999F) {
        return true;
    }

    LTVector axis(
        from.y * to.z - from.z * to.y,
        from.z * to.x - from.x * to.z,
        from.x * to.y - from.y * to.x);
    if (axis.MagSqr() < 0.0001F) {
        const LTVector reference =
            std::fabs(from.y) < 0.9F
                ? LTVector(0.0F, 1.0F, 0.0F)
                : LTVector(1.0F, 0.0F, 0.0F);
        axis.Init(
            from.y * reference.z - from.z * reference.y,
            from.z * reference.x - from.x * reference.z,
            from.x * reference.y - from.y * reference.x);
    }
    axis.Normalize();
    rotation.Init(axis, std::acos(dot));
    return true;
}

void SelectDualPistolFromTriggers(void* weapon) noexcept {
    if (!RetailDualPistolsActive(weapon)) {
        return;
    }
    constexpr float kTriggerThreshold = 0.55F;
    const bool leftDown =
        (g_currentInput.activeHands & FEARVR_HAND_MASK_LEFT) != 0 &&
        g_currentInput.trigger[FEARVR_HAND_LEFT] >= kTriggerThreshold;
    const bool rightDown =
        (g_currentInput.activeHands & FEARVR_HAND_MASK_RIGHT) != 0 &&
        g_currentInput.trigger[FEARVR_HAND_RIGHT] >= kTriggerThreshold;
    // Bei genau einem gedrueckten Trigger ist die Wahl eindeutig. Sind beide
    // gedrueckt, darf Retails Dual-Pistol-Animation weiterhin abwechseln.
    if (leftDown == rightDown) {
        return;
    }
    __try {
        *(static_cast<unsigned char*>(weapon) +
          kRetailWeaponFireLeftHandOffset) =
            leftDown ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool ApplyRetailLeftWeaponTransform() noexcept {
    if (g_client == nullptr ||
        g_weaponAim.leftWeaponModel == nullptr ||
        !g_weaponAim.leftAimValid ||
        !g_weaponAim.leftGripValid) {
        g_weaponAim.leftWeaponTransformValid = false;
        return false;
    }

    LTRotation weaponRotation = g_weaponAim.leftAimTransform.m_rRot;
    if (g_weaponAim.leftMuzzleLocalValid) {
        // worldMuzzle = worldWeapon * muzzleInWeapon. Also ist die eindeutige
        // Waffenrotation fuer eine gewuenschte OpenXR-Muendungsrotation:
        // worldWeapon = aim * inverse(muzzleInWeapon).
        //
        // Anders als eine reine Vorwaertsvektor-Korrektur behebt das auch den
        // Rollversatz des gespiegelten LDPistol-Modells.
        weaponRotation =
            g_weaponAim.leftAimTransform.m_rRot *
            g_weaponAim.leftMuzzleRotationInWeapon.Conjugate();
        weaponRotation.Normalize();
    }

    g_weaponAim.leftWeaponTransform =
        LTRigidTransform(
            g_weaponAim.leftGripTransform.m_vPos,
            weaponRotation);
    __try {
        if (g_client->SetObjectTransform(
                g_weaponAim.leftWeaponModel,
                g_weaponAim.leftWeaponTransform) != LT_OK) {
            g_weaponAim.leftWeaponTransformValid = false;
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_weaponAim.leftWeaponTransformValid = false;
        return false;
    }
    g_weaponAim.leftWeaponTransformValid = true;
    return true;
}

// Laenge der aktuellen Waffe: der gemessene Abstand zwischen Waffenursprung
// und Muendungssockel. Er ist der einzige Waffenwert, den wir ohne
// Retail-Waffendatenbank verlaesslich haben, und trennt Pistole von Gewehr
// genau so, wie es fuer das beidhaendige Zielen noetig ist.
float CurrentBarrelLengthMeters() noexcept {
    if (!g_weaponAim.muzzleLocalValid) {
        return 0.0F;
    }
    const float lengthUnits =
        std::sqrt(g_weaponAim.muzzleOffsetInWeapon.MagSqr());
    if (!std::isfinite(lengthUnits)) {
        return 0.0F;
    }
    return lengthUnits / kGameUnitsPerMeter;
}

// Richtung von der Waffenhand zur Stuetzhand, normalisiert.
bool CurrentSupportDirection(LTVector& direction) noexcept {
    if (!g_weaponAim.supportRightGripValid ||
        !g_weaponAim.leftGripValid) {
        return false;
    }
    // Both ends of the steering line must come from the same, unfiltered
    // controller sample space. Using the weighted right-hand position here
    // mixes a delayed weapon pose with the live support hand; the resulting
    // artificial diagonal moves the support hand away from the weapon grip.
    direction =
        g_weaponAim.leftGripTransform.m_vPos -
        g_weaponAim.supportRightGripTransform.m_vPos;
    const float minimumSeparation =
        kTwoHandMinSteerSeparationMeters * kGameUnitsPerMeter;
    if (!(direction.MagSqr() >
          minimumSeparation * minimumSeparation)) {
        return false;
    }
    direction.Normalize();
    return true;
}

// Haelt die Stuetzhand die Handlinie weiter als den zulaessigen Lenkwinkel
// seitlich, darf die Zweihandkorrektur nicht einfach aussetzen. Die
// Waffenpose wurde fuer dieses Bild bereits aus der rechten Hand aufgebaut;
// ein `return` wuerde sie deshalb schlagartig auf Einhand-Zielen und damit in
// die Bildmitte zuruecksetzen. Stattdessen wird der Winkel weich gedaempft.
bool ClampTwoHandedTargetDirection(
    const LTVector& baseDirection, LTVector& target) noexcept {
    const float dot = std::clamp(baseDirection.Dot(target), -1.0F, 1.0F);
    const float angle = std::acos(dot);
    const float limited = SoftLimitedSteerAngle(angle);
    if (!std::isfinite(limited) || limited >= angle - 0.0001F) {
        return true;
    }

    // Das Kreuzprodukt von Hand: `LTVector::Cross` dreht die Operanden-
    // reihenfolge gegenueber dem rechtshaendigen Kreuzprodukt um
    // (COORDINATE-SYSTEM.md §1). Mit `baseDirection.Cross(target)` zeigte die
    // Drehachse deshalb genau in die Gegenrichtung, und die begrenzte Waffe
    // kippte auf die falsche Seite der Zielachse — der Sprung, den der
    // Benutzer am 28.07.2026 als "ploetzlich nach links" gemeldet hat.
    // `RotationBetweenDirections` rechnet aus demselben Grund von Hand.
    LTVector axis(
        baseDirection.y * target.z - baseDirection.z * target.y,
        baseDirection.z * target.x - baseDirection.x * target.z,
        baseDirection.x * target.y - baseDirection.y * target.x);
    if (axis.MagSqr() < 0.0001F) {
        // Handlinie und Waffenachse sind (anti)parallel: jede senkrechte
        // Achse taugt, die Richtung ist hier ohnehin nicht bestimmt.
        const LTVector reference =
            std::fabs(baseDirection.y) < 0.9F
                ? LTVector(0.0F, 1.0F, 0.0F)
                : LTVector(1.0F, 0.0F, 0.0F);
        axis.Init(
            baseDirection.y * reference.z - baseDirection.z * reference.y,
            baseDirection.z * reference.x - baseDirection.x * reference.z,
            baseDirection.x * reference.y - baseDirection.y * reference.x);
    }
    if (axis.MagSqr() < 0.0001F) {
        return false;
    }
    axis.Normalize();

    LTRotation limit;
    limit.Init(axis, limited);
    target = limit.RotateVector(baseDirection);
    return target.MagSqr() > 0.0001F;
}

void ReleaseTwoHandedGrip(bool preserveGeometry = true) noexcept {
    g_twoHandedGripActive = false;
    if (!preserveGeometry) {
        g_twoHandedGrip = TwoHandedGripState{};
        return;
    }
    g_twoHandedGrip.placementValid = false;
    g_twoHandedGrip.animatedRightSocketValid = false;
    g_twoHandedGrip.animatedLeftSocketValid = false;
}

void UpdateTwoHandedGrip() noexcept {
    const void* const weapon = g_weaponAim.retailWeapon;
    if (g_twoHandedGrip.geometryWeapon != weapon) {
        ReleaseTwoHandedGrip(false);
        g_twoHandedGrip.geometryWeapon = weapon;
    }
    const bool usable =
        weapon != nullptr && g_twoHandedGripEnabled && g_weaponAim.valid &&
        g_weaponAim.gripValid && g_weaponAim.leftGripValid &&
        !DualPistolsVrActive();
    if (g_twoHandedGripActive) {
        if (!usable || ShouldReleaseTwoHandedGrip(g_currentInput)) {
            ReleaseTwoHandedGrip();
        }
        return;
    }
    if (!usable || !ShouldEngageTwoHandedGrip(g_currentInput)) {
        return;
    }

    g_twoHandedGripActive = true;
    if (g_twoHandedGrip.geometryValid &&
        g_twoHandedGrip.geometryWeapon == weapon) {
        g_twoHandedGrip.placementValid = true;
        g_twoHandedGrip.animatedRightSocketValid = false;
        g_twoHandedGrip.animatedLeftSocketValid = false;
    } else {
        // Die sichtbare Hand darf nicht an der zufaelligen Controllerstelle
        // einrasten. Die naechste unveraenderte Player-Body-Auswertung liefert
        // stattdessen Retails original animierten linken Griff relativ zum
        // rechten Waffenhand-Socket.
        g_twoHandedGrip.placementValid = false;
        g_twoHandedGrip.geometryValid = false;
        g_twoHandedGrip.animatedRightSocketValid = false;
        g_twoHandedGrip.animatedLeftSocketValid = false;
    }

    if (InterlockedCompareExchange(
            &g_twoHandedGripActiveLogged, 1, 0) == 0) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "The left hand is supporting the weapon as a rigid secondary "
            "pivot using Retail's weapon-specific animated grip. Both "
            "tracked hands drive the weapon until release.");
        Report("INFO", "two_handed_grip_active", message);
    }
}

// Rigid two-anchor solve. The dominant controller contributes the base
// orientation (including roll), the line between both tracked grips corrects
// its direction, and translation then places the stored support attachment
// exactly at the left controller. Moving the right hand around a stationary
// support hand therefore rotates around the support pivot instead of dragging
// the visible left hand around a right-hand origin.
void ApplyTwoHandedAimSupport() noexcept {
    if (!g_twoHandedGripActive || !g_twoHandedGrip.geometryValid) {
        return;
    }

    LTVector support;
    if (!CurrentSupportDirection(support)) {
        // A collapsed hand line has no stable rigid solution. Release the
        // secondary attachment rather than reverting to right-origin motion
        // and dragging the visible support hand.
        ReleaseTwoHandedGrip();
        return;
    }
    LTVector predictedSupport =
        g_weaponAim.fireTransform.m_rRot.RotateVector(
            g_twoHandedGrip.grabOffsetInWeapon);
    if (predictedSupport.MagSqr() < 0.0001F) {
        return;
    }
    predictedSupport.Normalize();
    // Zwei Haende an derselben Waffe koennen nicht beliebig zueinander
    // stehen. Laeuft die Stuetzhand zu weit weg, wird die Lenkung am Rand des
    // erlaubten Kegels begrenzt. Ein harter Abbruch hier wuerde auf die zuvor
    // berechnete Einhandpose zurueckfallen und die Waffe in die Bildmitte
    // springen lassen.
    if (!ClampTwoHandedTargetDirection(predictedSupport, support)) {
        return;
    }

    LTRotation correction;
    if (!RotationBetweenDirections(
            predictedSupport, support, correction)) {
        return;
    }
    g_weaponAim.fireTransform.m_rRot =
        correction * g_weaponAim.fireTransform.m_rRot;
    g_weaponAim.fireTransform.m_rRot.Normalize();

    const LTVector rotatedSupportOffset =
        g_weaponAim.fireTransform.m_rRot.RotateVector(
            g_twoHandedGrip.grabOffsetInWeapon);
    const TwoHandedPivotTranslation pivot = SolveSecondaryGripPivot(
        {g_weaponAim.gripTransform.m_vPos.x,
         g_weaponAim.gripTransform.m_vPos.y,
         g_weaponAim.gripTransform.m_vPos.z},
        {g_weaponAim.leftGripTransform.m_vPos.x,
         g_weaponAim.leftGripTransform.m_vPos.y,
         g_weaponAim.leftGripTransform.m_vPos.z},
        {rotatedSupportOffset.x, rotatedSupportOffset.y,
         rotatedSupportOffset.z});
    if (!pivot.valid) {
        return;
    }
    const LTVector pivotCorrection(
        pivot.correction.x, pivot.correction.y, pivot.correction.z);
    g_weaponAim.gripTransform.m_vPos = LTVector(
        pivot.primaryPosition.x,
        pivot.primaryPosition.y,
        pivot.primaryPosition.z);
    // Aim and grip poses originate on the same controller. Preserve their
    // measured offset when the rigid weapon origin moves to honor the pivot.
    g_weaponAim.fireTransform.m_vPos += pivotCorrection;
}

struct WeaponCollisionBoxGeometry {
    LTVector rearCenter;
    LTVector right;
    LTVector up;
    LTVector forward;
    float lengthUnits{0.0F};
    float halfWidthUnits{0.0F};
    float halfHeightUnits{0.0F};
};

bool ResolveWeaponCollisionBox(
    const void* weapon, WeaponCollisionBoxGeometry& box) noexcept {
    if (weapon == nullptr || weapon != g_weaponAim.muzzleWeapon ||
        !g_weaponAim.valid || !g_weaponAim.gripValid ||
        !g_weaponAim.muzzleLocalValid) {
        return false;
    }
    const WeaponCollisionProfile profile =
        SanitizeWeaponCollisionProfile(ActiveWeaponCollisionProfile());
    const float barrelLengthMeters = CurrentBarrelLengthMeters();
    if (!(barrelLengthMeters > 0.02F)) {
        return false;
    }

    g_weaponAim.fireTransform.m_rRot.GetVectors(
        box.right, box.up, box.forward);
    if (box.right.MagSqr() < 0.0001F ||
        box.up.MagSqr() < 0.0001F ||
        box.forward.MagSqr() < 0.0001F) {
        return false;
    }
    box.right.Normalize();
    box.up.Normalize();
    box.forward.Normalize();
    box.lengthUnits =
        barrelLengthMeters * profile.lengthScale * kGameUnitsPerMeter;
    box.halfWidthUnits =
        profile.widthMeters * 0.5F * kGameUnitsPerMeter;
    box.halfHeightUnits =
        profile.heightMeters * 0.5F * kGameUnitsPerMeter;
    box.rearCenter = g_weaponAim.gripTransform.m_vPos +
        box.forward *
            (profile.forwardOffsetMeters * kGameUnitsPerMeter);
    return std::isfinite(box.lengthUnits) && box.lengthUnits > 2.0F;
}

void BuildWeaponCollisionCorners(
    const WeaponCollisionBoxGeometry& box, float expansionUnits,
    LTVector (&corners)[8]) noexcept {
    expansionUnits = (std::max)(0.0F, expansionUnits);
    const LTVector rearCenter =
        box.rearCenter - box.forward * expansionUnits;
    const LTVector frontCenter = box.rearCenter + box.forward *
        (box.lengthUnits + expansionUnits);
    const LTVector right = box.right *
        (box.halfWidthUnits + expansionUnits);
    const LTVector up = box.up *
        (box.halfHeightUnits + expansionUnits);
    corners[0] = rearCenter - right - up;
    corners[1] = rearCenter + right - up;
    corners[2] = rearCenter + right + up;
    corners[3] = rearCenter - right + up;
    corners[4] = frontCenter - right - up;
    corners[5] = frontCenter + right - up;
    corners[6] = frontCenter + right + up;
    corners[7] = frontCenter - right + up;
}

bool TraceWeaponCollisionSegment(
    const LTVector& from, const LTVector& to,
    IntersectInfo& hit) noexcept {
    if ((to - from).MagSqr() < 0.0001F) {
        return false;
    }
    IntersectQuery query;
    query.m_From = from;
    query.m_To = to;
    // Deliberately do not set INTERSECT_OBJECTS. Retail creates many solid,
    // invisible helper objects and coarse AABB collision volumes around the
    // player and weapon. They produced permanent false contacts and unstable
    // normals. World polygons retain real level boundaries and carry HPOLY.
    query.m_Flags = IGNORE_NONSOLID | INTERSECT_HPOLY;
    return g_client->IntersectSegment(query, &hit) &&
           hit.m_hPoly != INVALID_HPOLY;
}

bool AccumulateWeaponCollisionContact(
    const WeaponCollisionBoxGeometry& box,
    const LTVector (&corners)[8], const IntersectInfo& hit,
    LTVector& correction) noexcept {
    LTVector normal = hit.m_Plane.m_Normal;
    if (!std::isfinite(normal.x) || !std::isfinite(normal.y) ||
        !std::isfinite(normal.z) || normal.MagSqr() < 0.0001F) {
        return false;
    }
    normal.Normalize();

    // Engine polygon normals are not guaranteed to face the player. The
    // tracking base remains on the traversable side even if the physical
    // controller has crossed a thin wall, so orient every contact normal
    // toward that stable safe point rather than toward the penetrating box.
    const LTVector safePoint = g_weaponAim.trackingBaseValid
        ? g_weaponAim.trackingBase.m_vPos
        : box.rearCenter - box.forward * box.lengthUnits;
    if ((safePoint - hit.m_Point).Dot(normal) < 0.0F) {
        normal *= -1.0F;
    }

    float closestDistance = std::numeric_limits<float>::infinity();
    for (const LTVector& corner : corners) {
        closestDistance = (std::min)(
            closestDistance, (corner - hit.m_Point).Dot(normal));
    }
    if (!std::isfinite(closestDistance)) {
        return false;
    }
    const float maximumCorrection = (std::max)({
        box.lengthUnits,
        box.halfWidthUnits * 2.0F,
        box.halfHeightUnits * 2.0F}) + kWeaponCollisionMarginUnits;
    const float required = WeaponCollisionPlaneCorrection(
        closestDistance, kWeaponCollisionMarginUnits,
        maximumCorrection);
    if (!(required > 0.0F)) {
        return false;
    }
    const float alreadyCorrected = correction.Dot(normal);
    if (required > alreadyCorrected) {
        correction += normal * (required - alreadyCorrected);
    }
    return true;
}

void ApplyWeaponWorldCollision(const void* weapon) noexcept {
    WeightedWeaponInputState& weighted = g_weightedWeaponInput;
    WeaponCollisionBoxGeometry box{};
    if (!g_weaponCollisionEnabled || g_client == nullptr ||
        !ResolveWeaponCollisionBox(weapon, box)) {
        ResetWeaponCollisionRuntime(weighted.collision);
        weighted.collisionObstructed = false;
        return;
    }

    LTVector corners[8]{};
    BuildWeaponCollisionCorners(box, 0.0F, corners);
    struct ProbeOffset {
        float right;
        float up;
    };
    constexpr ProbeOffset probes[] = {
        { 0.0F,  0.0F},
        {-1.0F, -1.0F}, { 1.0F, -1.0F},
        { 1.0F,  1.0F}, {-1.0F,  1.0F},
        { 0.0F, -1.0F}, { 1.0F,  0.0F},
        { 0.0F,  1.0F}, {-1.0F,  0.0F},
    };

    LTVector targetCorrection;
    targetCorrection.Init();
    IntersectInfo selectedHit;
    float selectedCorrectionMagnitudeSqr = 0.0F;
    bool contact = false;
    bool queryFailed = false;
    for (const ProbeOffset& probe : probes) {
        const LTVector origin = box.rearCenter +
            box.right * (probe.right * box.halfWidthUnits) +
            box.up * (probe.up * box.halfHeightUnits);
        IntersectInfo hit;
        __try {
            if (TraceWeaponCollisionSegment(
                    origin,
                    origin + box.forward *
                        (box.lengthUnits + kWeaponCollisionMarginUnits),
                    hit)) {
                LTVector candidate;
                candidate.Init();
                if (AccumulateWeaponCollisionContact(
                        box, corners, hit, candidate)) {
                    const float magnitudeSqr = candidate.MagSqr();
                    if (!contact ||
                        magnitudeSqr > selectedCorrectionMagnitudeSqr) {
                        contact = true;
                        targetCorrection = candidate;
                        selectedHit = hit;
                        selectedCorrectionMagnitudeSqr = magnitudeSqr;
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            queryFailed = true;
            break;
        }
    }
    WeaponCollisionRuntimeState& state = weighted.collision;
    if (queryFailed) {
        ResetWeaponCollisionRuntime(weighted.collision);
        weighted.collisionObstructed = false;
        return;
    }

    const std::uint64_t nowNs = MonotonicNanoseconds();
    float seconds = 0.0F;
    if (state.haveUpdate && nowNs > state.lastUpdateNs &&
        nowNs - state.lastUpdateNs <= kWeaponCollisionMaxGapNs) {
        seconds = static_cast<float>(nowNs - state.lastUpdateNs) * 1.0e-9F;
    }
    state.lastUpdateNs = nowNs;
    state.haveUpdate = true;
    if (contact) {
        if (seconds <= 0.0F) {
            state.correction = targetCorrection;
        } else {
            const float tighten = std::clamp(
                seconds / kWeaponCollisionTightenSeconds, 0.0F, 1.0F);
            state.correction +=
                (targetCorrection - state.correction) * tighten;
        }
        state.holdUntilNs = nowNs + kWeaponCollisionHoldNs;
    } else if (nowNs >= state.holdUntilNs) {
        if (seconds <= 0.0F ||
            seconds >= kWeaponCollisionReleaseSeconds) {
            state.correction.Init();
        } else {
            const float release = std::clamp(
                seconds / kWeaponCollisionReleaseSeconds, 0.0F, 1.0F);
            state.correction *= 1.0F - release;
        }
    }

    weighted.collisionObstructed = contact ||
        state.correction.MagSqr() > 0.0001F;

    const ULONGLONG diagnosticNow = GetTickCount64();
    if (contact && g_weaponWeightDiagnosticsEnabled &&
        (weighted.lastCollisionDiagnosticTick == 0 ||
         diagnosticNow - weighted.lastCollisionDiagnosticTick >= 1000)) {
        weighted.lastCollisionDiagnosticTick = diagnosticNow;
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "profile=%s world=%u poly=%u surface=0x%08X "
            "correction=(%.2f,%.2f,%.2f)",
            weighted.profileName,
            static_cast<unsigned>(selectedHit.m_hPoly.m_nWorldIndex),
            static_cast<unsigned>(selectedHit.m_hPoly.m_nPolyIndex),
            static_cast<unsigned>(selectedHit.m_SurfaceFlags),
            state.correction.x, state.correction.y,
            state.correction.z);
        Report("INFO", "weapon_collision_contact", message);
    }
    g_weaponAim.fireTransform.m_vPos += state.correction;
    g_weaponAim.gripTransform.m_vPos += state.correction;
    if (g_twoHandedGripActive) {
        g_weaponAim.leftAimTransform.m_vPos += state.correction;
        g_weaponAim.leftGripTransform.m_vPos += state.correction;
    }

    static volatile LONG activeLogged = 0;
    if (InterlockedCompareExchange(&activeLogged, 1, 0) == 0) {
        Report(
            "INFO", "weapon_world_collision_active",
            "The equipped weapon uses nine world-polygon-only probes and "
            "smoothly resolves the rendered hands and muzzle along one "
            "bounded surface normal.");
    }
}

void RenderWeaponCollisionBox(HLOCALOBJ camera) noexcept {
    const bool menuPreview =
        g_devMenu.open && g_devMenu.selectedTab == DevMenuTab::collision;
    if (g_client == nullptr || camera == nullptr || g_weaponDisabled ||
        (!g_weaponCollisionDebugEnabled && !menuPreview)) {
        return;
    }
    WeaponCollisionBoxGeometry box{};
    if (!ResolveWeaponCollisionBox(g_weightedWeaponInput.weapon, box)) {
        return;
    }

    const LTVector frontCenter =
        box.rearCenter + box.forward * box.lengthUnits;
    const LTVector right = box.right * box.halfWidthUnits;
    const LTVector up = box.up * box.halfHeightUnits;
    const LTVector corners[] = {
        box.rearCenter - right - up,
        box.rearCenter + right - up,
        box.rearCenter + right + up,
        box.rearCenter - right + up,
        frontCenter - right - up,
        frontCenter + right - up,
        frontCenter + right + up,
        frontCenter - right + up,
    };
    constexpr std::uint8_t edgeIndices[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    LT_LINEG edges[std::size(edgeIndices)]{};
    const bool obstructed = g_weightedWeaponInput.collisionObstructed;
    const std::uint8_t red = obstructed ? 255 : 40;
    const std::uint8_t green = obstructed ? 45 :
        (g_weaponCollisionEnabled ? 255 : 170);
    const std::uint8_t blue = obstructed ? 20 :
        (g_weaponCollisionEnabled ? 80 : 255);
    for (std::size_t index = 0; index < std::size(edges); ++index) {
        edges[index].verts[0].pos = corners[edgeIndices[index][0]];
        edges[index].verts[1].pos = corners[edgeIndices[index][1]];
        edges[index].verts[0].rgba.Init(red, green, blue, 245);
        edges[index].verts[1].rgba.Init(red, green, blue, 245);
    }

    ILTDrawPrim* const drawPrim = g_client->GetDrawPrim();
    if (drawPrim == nullptr) {
        return;
    }
    const HOBJECT oldCamera = drawPrim->GetCamera();
    const ELTDrawPrimTransformMode oldTransformMode =
        drawPrim->GetTransformMode();
    const ELTDrawPrimZMode oldZMode = drawPrim->GetZMode();
    const ELTDrawPrimRenderMode oldRenderMode =
        drawPrim->GetRenderMode();
    drawPrim->SetCamera(camera);
    drawPrim->SetTransformMode(eLTDrawPrimTransformMode_World);
    drawPrim->SetZMode(eLTDrawPrimZMode_NoWrite);
    drawPrim->SetRenderMode(eLTDrawPrimRenderMode_Modulate_Additive);
    drawPrim->DrawPrim(
        edges, static_cast<std::uint32_t>(std::size(edges)));
    drawPrim->SetRenderMode(oldRenderMode);
    drawPrim->SetZMode(oldZMode);
    drawPrim->SetTransformMode(oldTransformMode);
    drawPrim->SetCamera(oldCamera);
}

void __fastcall HookRetailStartMuzzleFlash(
    void* weaponModelData, void* ignoredEdx) {
    (void)ignoredEdx;
    void* flashOwner = weaponModelData;
    void* const weapon = g_retailWeaponUpdateInProgress;
    if (weapon != nullptr) {
        auto* const bytes = static_cast<unsigned char*>(weapon);
        void* const leftModelData =
            bytes + kRetailLeftWeaponModelDataOffset;
        if (weaponModelData == leftModelData &&
            RetailRightWeaponModel(weapon) != nullptr) {
            flashOwner =
                bytes + kRetailRightWeaponModelDataOffset;
            static volatile LONG redirectLogged = 0;
            if (InterlockedCompareExchange(
                    &redirectLogged, 1, 0) == 0) {
                Report(
                    "INFO", "dual_pistol_muzzle_flash_redirected",
                    "A left-hand Dual Pistols shot now starts its flash on "
                    "the carried right-hand pistol.");
            }
        }
    }
    g_retailStartMuzzleFlash(flashOwner);
}

int __fastcall HookRetailWeaponManagerUpdate(
    void* weaponManager, void* ignoredEdx,
    const LTRotation& baseRotation,
    const LTVector& basePosition) {
    (void)ignoredEdx;
    // Wachhund: Sollte der Renderhook seit dem letzten Update nicht gelaufen
    // sein — etwa weil eine Zwischensequenz den Renderpfad uebernommen hat —
    // waere die Kamera bis hierher entfuehrt geblieben. Spaetestens jetzt
    // zuruecksetzen, damit der Override nie laenger als einen Updatezyklus
    // offen steht.
    RestorePreRenderCameraOverride();

    const ULONGLONG now = GetTickCount64();
    const bool enteringPlayingState =
        g_lastWeaponManagerUpdateTick == 0 ||
        now - g_lastWeaponManagerUpdateTick > 1000;
    g_lastWeaponManagerUpdateTick = now;
    if (enteringPlayingState) {
        // Eine Pause im Weapon-Manager-Update bedeutet Ladebildschirm,
        // Levelwechsel oder eine Sequenz, die den Spielerpfad uebernommen hat.
        // Danach ist die Welt neu aufgebaut und jeder zwischengespeicherte
        // Objektzeiger zeigt auf ein zerstoertes Objekt. Bisher haben wir
        // trotzdem weiter darauf geschrieben.
        //
        // Bewusst nur vergessen, nicht aufraeumen: RemoveObject auf einem
        // toten Handle waere genau der Fehler, den wir vermeiden wollen. Die
        // Engine hat die Objekte beim Weltwechsel bereits selbst zerstoert.
        ForgetWorldObjectsAfterLevelChange();
    }
    LTRotation trackedBaseRotation = baseRotation;
    const LTVector trackedBasePosition =
        ResolveTrackedHandBasePosition(
            basePosition, baseRotation, trackedBaseRotation);
    if (!g_autoStereoActivationAttempted && !g_disableStereoRender &&
        g_isStereoAvailable != nullptr &&
        g_isStereoEnabled != nullptr &&
        g_setStereoEnabled != nullptr) {
        g_autoStereoActivationAttempted = true;
        __try {
            if (g_isStereoAvailable() && !g_isStereoEnabled()) {
                g_setStereoEnabled(TRUE);
                InterlockedIncrement(&g_trackingResetGeneration);
                Report(
                    "INFO", "stereo_auto_enabled_after_load",
                    "The first verified playing-frame weapon update "
                    "automatically enabled native stereo.");
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Report(
                "WARN", "stereo_auto_enable_failed",
                "Automatic stereo activation after loading failed; "
                "F8 remains available.");
        }
    }
    FearVrPose weightedAimPose{};
    FearVrPose weightedGripPose{};
    std::uint32_t weightedAimValidHands = 0;
    std::uint32_t weightedGripValidHands = 0;
    void* const weaponBeforeUpdate =
        CurrentRetailWeapon(weaponManager);
    PrepareWeightedWeaponPoses(
        weaponBeforeUpdate, weightedAimPose,
        weightedAimValidHands, weightedGripPose,
        weightedGripValidHands);
    g_weaponAim.valid = BuildStableTrackedHandTransform(
        trackedBaseRotation, trackedBasePosition,
        weightedAimValidHands,
        FEARVR_HAND_MASK_RIGHT,
        weightedAimPose,
        g_aimPoseCache[FEARVR_HAND_RIGHT],
        g_weaponAim.fireTransform);
    g_weaponAim.gripValid = BuildStableTrackedHandTransform(
        trackedBaseRotation, trackedBasePosition,
        weightedGripValidHands,
        FEARVR_HAND_MASK_RIGHT,
        weightedGripPose,
        g_gripPoseCache[FEARVR_HAND_RIGHT],
        g_weaponAim.gripTransform);
    g_weaponAim.supportRightGripValid = BuildStableTrackedHandTransform(
        trackedBaseRotation, trackedBasePosition,
        g_currentInput.gripPoseValidHands,
        FEARVR_HAND_MASK_RIGHT,
        g_currentInput.handGripPose[FEARVR_HAND_RIGHT],
        g_supportRightGripPoseCache,
        g_weaponAim.supportRightGripTransform);
    g_weaponAim.leftAimValid = BuildStableTrackedHandTransform(
        trackedBaseRotation, trackedBasePosition,
        g_currentInput.aimPoseValidHands,
        FEARVR_HAND_MASK_LEFT,
        g_currentInput.handAimPose[FEARVR_HAND_LEFT],
        g_aimPoseCache[FEARVR_HAND_LEFT],
        g_weaponAim.leftAimTransform);
    g_weaponAim.leftGripValid = BuildStableTrackedHandTransform(
        trackedBaseRotation, trackedBasePosition,
        g_currentInput.gripPoseValidHands,
        FEARVR_HAND_MASK_LEFT,
        g_currentInput.handGripPose[FEARVR_HAND_LEFT],
        g_gripPoseCache[FEARVR_HAND_LEFT],
        g_weaponAim.leftGripTransform);
    g_weaponAim.trackingBase =
        LTRigidTransform(
            trackedBasePosition, trackedBaseRotation);
    g_weaponAim.trackingBaseValid =
        g_weaponAim.valid || g_weaponAim.gripValid ||
        g_weaponAim.leftAimValid || g_weaponAim.leftGripValid;
    void* weapon = CurrentRetailWeapon(weaponManager);
    g_weaponAim.retailWeapon = weapon;
    UpdateRetailDualPistolState(weapon);
    SelectDualPistolFromTriggers(weapon);

    if (g_weaponAim.valid && g_weaponAim.muzzleDirectionValid) {
        const LTVector predictedMuzzleForward =
            g_weaponAim.fireTransform.m_rRot.RotateVector(
                g_weaponAim.muzzleForwardInWeapon);
        const LTVector controllerForward =
            g_weaponAim.fireTransform.m_rRot.Forward();
        LTRotation directionCorrection;
        if (RotationBetweenDirections(
                predictedMuzzleForward, controllerForward,
                directionCorrection)) {
            g_weaponAim.fireTransform.m_rRot =
                directionCorrection *
                g_weaponAim.fireTransform.m_rRot;
        }
    }
    if (DualPistolsVrActive() && !g_disableWeaponTransform) {
        // Der GetFireVectors-Hook kann innerhalb des folgenden Retail-Updates
        // laufen. Deshalb muss die aktuelle linke Controllerpose bereits vor
        // diesem Aufruf als Schusstransformation bereitstehen.
        ApplyRetailLeftWeaponTransform();
    }

    // Erst nach der Muendungskorrektur: Die Stuetzhand dreht die fertige
    // Feuerachse, nicht die Rohpose des Controllers.
    UpdateTwoHandedGrip();
    ApplyTwoHandedAimSupport();
    ApplyWeaponWorldCollision(weaponBeforeUpdate);

    // Install before Retail updates the weapon from the RightHand socket.
    // This path is verified to run every playing frame, unlike the optional
    // SetTrackedTarget call site which is dormant in the shipped game path.
    EnsureHandNodeControls(CurrentRetailPlayerBody());

    // Player-body mode normally keeps the separate player-view weapon hidden.
    // Initialize Retail's logical visibility for each new weapon, or restore
    // it when Retail actually hides the model. Do not call SetVisible on
    // ordinary frames: it always hides the looped muzzle FX and can restart a
    // short pistol flash between renders.
    if (weaponBeforeUpdate != nullptr &&
        !RetailWeaponIsDisabled(weaponBeforeUpdate) &&
        (weaponBeforeUpdate !=
             g_retailVisibilityInitializedWeapon ||
         !RetailRightWeaponModelIsVisible(
             weaponBeforeUpdate)) &&
        g_retailSetWeaponVisible != nullptr) {
        __try {
            g_retailSetWeaponVisible(
                weaponBeforeUpdate, true, true);
            g_retailVisibilityInitializedWeapon =
                weaponBeforeUpdate;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    // F.E.A.R. applies CAccuracyMgr perturbation after GetFireVectors.
    // Suppress it only during the actual weapon update while VR aiming is
    // active, then restore the game's state immediately. This keeps the
    // client vector and server fire message on the visible guide ray.
    float* currentPerturb = nullptr;
    float savedPerturb = 0.0F;
    if (g_weaponAim.valid &&
        g_retailAccuracyManager != nullptr) {
        __try {
            currentPerturb = static_cast<float*>(
                g_retailAccuracyManager());
            if (currentPerturb != nullptr) {
                savedPerturb = *currentPerturb;
                *currentPerturb = 0.0F;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            currentPerturb = nullptr;
        }
    }
    g_retailWeaponUpdateInProgress = weaponBeforeUpdate;
    const int state = g_retailWeaponManagerUpdate(
        weaponManager, baseRotation, basePosition);
    g_retailWeaponUpdateInProgress = nullptr;
    if (currentPerturb != nullptr) {
        __try {
            *currentPerturb = savedPerturb;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        if (InterlockedCompareExchange(
                &g_bulletGuideAlignmentActiveLogged, 1, 0) == 0) {
            Report(
                "INFO", "bullet_guide_alignment_active",
                "Retail bullet perturbation is zero only during VR fire; "
                "bullets and server messages follow the visible guide ray.");
        }
    }

    // Retail samples the animated RightHand socket during its update, before
    // the final node-control result is rendered. On stair-step animation that
    // sampled socket can be one frame old. The hand node is already solved to
    // this exact grip/aim transform, so apply the same transform to the weapon
    // after Retail updates it and keep both visually locked together.
    weapon = CurrentRetailWeapon(weaponManager);
    g_weaponAim.retailWeapon = weapon;
    UpdateRetailDualPistolState(weapon);
    if (weapon != nullptr && !g_disableWeaponTransform &&
        g_weaponAim.valid && g_weaponAim.gripValid &&
        g_retailSetWeaponTransform != nullptr) {
        const LTTransform synchronizedTransform(
            g_weaponAim.gripTransform.m_vPos,
            g_weaponAim.fireTransform.m_rRot, 1.0F);
        __try {
            g_retailSetWeaponTransform(
                weapon, synchronizedTransform);
            if (InterlockedCompareExchange(
                    &g_weaponSocketSyncActiveLogged, 1, 0) == 0) {
                Report(
                    "INFO", "weapon_socket_sync_active",
                    "The visible weapon and corrected RightHand socket use "
                    "the same post-update OpenXR transform.");
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    if (g_weaponAim.valid) {
        UpdateRetailMuzzlePosition(weapon);
    }
    if (weapon != nullptr && !g_disableWeaponTransform &&
        DualPistolsVrActive()) {
        // Retails SetWeaponTransform setzt beide Modelle auf die Waffenhand.
        // Das linke Modell bekommt danach seine eigene Support-Hand-Pose. Die
        // erste Anwendung liefert den Socket, die zweite nutzt dessen
        // gemessene lokale Laufachse bereits im selben Bild.
        if (ApplyRetailLeftWeaponTransform()) {
            UpdateRetailLeftMuzzlePosition();
            ApplyRetailLeftWeaponTransform();
        }
    }
    // Retails Deaktivierung gilt vor unserer Sichtbarkeit: Waehrend die Waffe
    // abgeschaltet ist (Leiter, Zwischensequenz, Geschuetz), gehoert auch der
    // Zielstrahl nicht ins Bild. `SetVisible` selbst kehrt in diesem Zustand
    // ohnehin folgenlos zurueck.
    g_weaponDisabled = RetailWeaponIsDisabled(weapon);
    UpdateFlashlightModel(weapon);
    UpdateVrMuzzleFlash();
    if (weapon != nullptr && !g_weaponDisabled) {
        ForceRetailRightWeaponModelVisible(weapon);
    }
    // Die linke Dual-Pistol nach allen Retail-Sichtbarkeitsänderungen erneut
    // an der Support-Hand fixieren.
    if (weapon != nullptr && !g_disableWeaponTransform &&
        DualPistolsVrActive()) {
        ApplyRetailLeftWeaponTransform();
    }
    if (weapon != nullptr && g_weaponAim.valid) {
        if (InterlockedCompareExchange(
                &g_weaponAimActiveLogged, 1, 0) == 0) {
            Report(
                "INFO", "weapon_aim_active",
                "The Retail AimAt tracker keeps hands and visible weapon "
            "together while OpenXR aim drives the fire vector.");
        }
    }

    // Die Retail-Taschenlampe wird bewusst nicht mehr mitgefuehrt.
    //
    // Frueher wurde sie per Kommandopuls dauerhaft eingeschaltet, und die
    // Kamera — ihr Folgeobjekt — bekam fuer die Dauer ihres Updates die
    // Handpose. Daneben existiert aber ohnehin unser eigener, schaltbarer
    // Spotprojektor. In der Standardposition an der linken Hand lagen beide
    // Lampen im Normalfall uebereinander und fielen deshalb nicht auf.
    //
    // Nach Zwischensequenzen ruht der Kameraeingriff jedoch, weil die Kamera
    // dann der Engine gehoert. Die Retail-Lampe leuchtete in diesem Moment
    // wieder vom Kopf aus, waehrend der VR-Scheinwerfer weiterlief: zwei
    // getrennte Kegel, deren Lichtfelder sich addierten, und nur einer davon
    // liess sich ausschalten.
    //
    // Der VR-Scheinwerfer allein deckt denselben Zweck ab. Damit entfaellt
    // zugleich ein weiterer Kameraeingriff in geskripteten Szenen — genau die
    // Sorte Eingriff, die hier bereits einmal zu Abstuerzen gefuehrt hat.
    return state;
}

bool IsStaticPlayerBodyTrackerContext(const void* context) noexcept {
    const HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr || context == nullptr) {
        return false;
    }
    const std::uintptr_t offset =
        reinterpret_cast<std::uintptr_t>(context) -
        reinterpret_cast<std::uintptr_t>(module);
    // The PlayerBody singleton, including its embedded client node tracker,
    // has static storage in the verified Retail .data section. Character
    // trackers are heap objects and must retain their original targets.
    return offset >= 0x002D0000U && offset < 0x002E9900U;
}

bool ApplyHandSocketPose(
    const NodeControlData& data,
    const HandNodeControlState& control,
    const LTRigidTransform& desiredSocketPose) noexcept {
    if (!control.installed || !control.socketFromNodeValid ||
        data.m_hModel != g_playerBodyObject ||
        data.m_hNode != control.node ||
        data.m_pModelTransform == nullptr ||
        data.m_pNodeTransform == nullptr) {
        return false;
    }

    // The socket offset contains the Retail model's hand-axis correction.
    // Solve the node transform from the desired socket transform so the
    // weapon socket itself, rather than merely the hand bone, matches OpenXR.
    const LTTransform desiredSocketWorld(desiredSocketPose);
    const LTTransform desiredNodeWorld =
        desiredSocketWorld * control.socketFromNode.GetInverse();
    const LTTransform desiredObject =
        data.m_pModelTransform->GetInverse() * desiredNodeWorld;
    data.m_pNodeTransform->m_vPos = desiredObject.m_vPos;
    data.m_pNodeTransform->m_rRot = desiredObject.m_rRot;
    return true;
}

bool CaptureOriginalTwoHandSocket(
    const NodeControlData& data,
    const HandNodeControlState& control,
    bool rightHand) noexcept {
    if (!g_twoHandedGripActive ||
        g_twoHandedGrip.placementValid ||
        !control.installed || !control.socketFromNodeValid ||
        data.m_hModel != g_playerBodyObject ||
        data.m_hNode != control.node ||
        data.m_pModelTransform == nullptr ||
        data.m_pNodeTransform == nullptr) {
        return false;
    }

    // Zu diesem Zeitpunkt enthalten Node und Socket noch Retails aktuelle
    // Waffenanimation. Die Arm-Controller lassen fuer diese eine Auswertung
    // absichtlich beide Ketten unangetastet.
    const LTTransform animatedNodeWorld =
        *data.m_pModelTransform * LTTransform(*data.m_pNodeTransform);
    const LTTransform animatedSocketWorld =
        animatedNodeWorld * control.socketFromNode;
    const LTRigidTransform socket(
        animatedSocketWorld.m_vPos, animatedSocketWorld.m_rRot);
    if (!std::isfinite(socket.m_vPos.x) ||
        !std::isfinite(socket.m_vPos.y) ||
        !std::isfinite(socket.m_vPos.z)) {
        return false;
    }

    if (rightHand) {
        g_twoHandedGrip.animatedRightSocket = socket;
        g_twoHandedGrip.animatedRightSocketValid = true;
    } else {
        g_twoHandedGrip.animatedLeftSocket = socket;
        g_twoHandedGrip.animatedLeftSocketValid = true;
    }
    if (!g_twoHandedGrip.animatedRightSocketValid ||
        !g_twoHandedGrip.animatedLeftSocketValid) {
        return false;
    }

    const LTRigidTransform& animatedRight =
        g_twoHandedGrip.animatedRightSocket;
    const LTRigidTransform& animatedLeft =
        g_twoHandedGrip.animatedLeftSocket;
    const LTRotation rightInverse =
        animatedRight.m_rRot.Conjugate();
    const LTVector gripOffset =
        rightInverse.RotateVector(
            animatedLeft.m_vPos - animatedRight.m_vPos);
    const float separation = gripOffset.Mag();
    if (!std::isfinite(separation) ||
        separation < 2.0F || separation > 120.0F) {
        g_twoHandedGrip.animatedRightSocketValid = false;
        g_twoHandedGrip.animatedLeftSocketValid = false;
        return false;
    }

    g_twoHandedGrip.grabOffsetInWeapon = gripOffset;
    g_twoHandedGrip.grabRotationInWeapon =
        rightInverse * animatedLeft.m_rRot;
    g_twoHandedGrip.grabRotationInWeapon.Normalize();
    const float minimumSeparation =
        kTwoHandMinSteerSeparationMeters * kGameUnitsPerMeter;
    g_twoHandedGrip.geometryValid =
        gripOffset.MagSqr() > minimumSeparation * minimumSeparation;
    if (!g_twoHandedGrip.geometryValid) {
        ReleaseTwoHandedGrip(false);
        return false;
    }
    g_twoHandedGrip.placementValid = true;
    g_twoHandedGrip.geometryWeapon = g_weaponAim.retailWeapon;

    char message[224]{};
    std::snprintf(
        message, sizeof(message),
        "Cached the first untouched Retail LeftHand socket at %.1f cm "
        "from the weapon hand: offset=(%.1f, %.1f, %.1f).",
        static_cast<double>(separation),
        static_cast<double>(gripOffset.x),
        static_cast<double>(gripOffset.y),
        static_cast<double>(gripOffset.z));
    Report("INFO", "two_handed_original_grip_captured", message);
    return true;
}

bool ApplyUpperArmTarget(
    const NodeControlData& data,
    HandNodeControlState& control,
    const LTVector& targetPosition) noexcept {
    control.desiredElbowValid = false;
    if (!control.upperArmInstalled ||
        !control.forearmOffsetFromUpperArmValid ||
        !control.socketOffsetFromForearmValid ||
        data.m_hModel != g_playerBodyObject ||
        data.m_hNode != control.upperArmNode ||
        data.m_pModelTransform == nullptr ||
        data.m_pNodeTransform == nullptr) {
        return false;
    }

    const LTTransform currentWorld =
        *data.m_pModelTransform *
        LTTransform(*data.m_pNodeTransform);
    const float upperLength =
        control.forearmOffsetFromUpperArm.Mag();
    const float lowerLength =
        control.socketOffsetFromForearm.Mag();
    LTVector currentUpperDirection =
        currentWorld.m_rRot.RotateVector(
            control.forearmOffsetFromUpperArm);
    const LTRotation& bodyRotation =
        data.m_pModelTransform->m_rRot;
    const LTVector bodyRight = bodyRotation.Right();
    const LTVector bodyUp =
        bodyRotation.RotateVector(LTVector(0.0F, 1.0F, 0.0F));
    const LTVector bodyForward = bodyRotation.Forward();
    // Elbows prefer an anatomical plane beside and slightly below/behind
    // the chest. Unlike the prior animated-arm projection this pole remains
    // stable while Retail changes locomotion or weapon animations.
    const LTVector poleDirection =
        bodyRight * (
            control.elbowSideSign * g_armIkTuning.elbowOutward) -
        bodyUp * g_armIkTuning.elbowDown -
        bodyForward * g_armIkTuning.elbowBack;
    const TwoBoneElbowSolution solution = SolveTwoBoneElbow(
        {currentWorld.m_vPos.x, currentWorld.m_vPos.y,
         currentWorld.m_vPos.z},
        {targetPosition.x, targetPosition.y, targetPosition.z},
        upperLength, lowerLength,
        {poleDirection.x, poleDirection.y, poleDirection.z},
        {control.bendDirectionWorld.x,
         control.bendDirectionWorld.y,
         control.bendDirectionWorld.z},
        control.bendDirectionValid &&
            g_armIkTuning.preserveElbowContinuity,
        {currentUpperDirection.x, currentUpperDirection.y,
         currentUpperDirection.z});
    if (!solution.valid) {
        return false;
    }
    const LTVector desiredElbow(
        solution.elbow.x, solution.elbow.y, solution.elbow.z);
    LTRotation upperCorrection;
    if (!RotationBetweenDirections(
            currentUpperDirection,
            desiredElbow - currentWorld.m_vPos,
            upperCorrection)) {
        return false;
    }

    const LTTransform desiredWorld(
        currentWorld.m_vPos,
        upperCorrection * currentWorld.m_rRot,
        1.0F);
    const LTTransform desiredObject =
        data.m_pModelTransform->GetInverse() * desiredWorld;
    data.m_pNodeTransform->m_rRot = desiredObject.m_rRot;
    control.desiredElbowWorld = desiredElbow;
    control.desiredElbowValid = true;
    control.bendDirectionWorld = LTVector(
        solution.bendDirection.x,
        solution.bendDirection.y,
        solution.bendDirection.z);
    control.bendDirectionValid = true;
    return true;
}

bool ApplyForearmTarget(
    const NodeControlData& data,
    const HandNodeControlState& control,
    const LTVector& targetPosition) noexcept {
    if (!control.forearmInstalled ||
        !control.socketOffsetFromForearmValid ||
        data.m_hModel != g_playerBodyObject ||
        data.m_hNode != control.forearmNode ||
        data.m_pModelTransform == nullptr ||
        data.m_pNodeTransform == nullptr) {
        return false;
    }

    const LTTransform currentWorld =
        *data.m_pModelTransform *
        LTTransform(*data.m_pNodeTransform);
    LTVector currentDirection =
        currentWorld.m_rRot.RotateVector(
            control.socketOffsetFromForearm);
    LTVector targetDirection =
        targetPosition - currentWorld.m_vPos;
    if (currentDirection.MagSqr() < 0.0001F ||
        targetDirection.MagSqr() < 0.0001F) {
        return false;
    }
    currentDirection.Normalize();
    targetDirection.Normalize();

    float dot = currentDirection.Dot(targetDirection);
    dot = (std::max)(-1.0F, (std::min)(1.0F, dot));
    LTRotation delta = LTRotation::GetIdentity();
    if (dot < 0.9999F) {
        LTVector axis(
            currentDirection.y * targetDirection.z -
                currentDirection.z * targetDirection.y,
            currentDirection.z * targetDirection.x -
                currentDirection.x * targetDirection.z,
            currentDirection.x * targetDirection.y -
                currentDirection.y * targetDirection.x);
        if (axis.MagSqr() < 0.0001F) {
            const LTVector reference =
                std::fabs(currentDirection.y) < 0.9F
                    ? LTVector(0.0F, 1.0F, 0.0F)
                    : LTVector(1.0F, 0.0F, 0.0F);
            axis.Init(
                currentDirection.y * reference.z -
                    currentDirection.z * reference.y,
                currentDirection.z * reference.x -
                    currentDirection.x * reference.z,
                currentDirection.x * reference.y -
                    currentDirection.y * reference.x);
        }
        axis.Normalize();
        delta.Init(axis, std::acos(dot));
    }

    const LTRotation desiredRotation =
        delta * currentWorld.m_rRot;
    // The upper-arm controller has already moved this node's origin to the
    // solved elbow. Preserve that joint and rotate only the lower arm.
    const LTTransform desiredWorld(
        currentWorld.m_vPos,
        desiredRotation,
        1.0F);
    const LTTransform desiredObject =
        data.m_pModelTransform->GetInverse() * desiredWorld;
    data.m_pNodeTransform->m_rRot = desiredObject.m_rRot;
    return true;
}

void RightUpperArmNodeControl(
    const NodeControlData& data, void* userData) {
    (void)userData;
    if (g_twoHandedGripActive &&
        !g_twoHandedGrip.placementValid) {
        return;
    }
    if (g_weaponAim.gripValid) {
        ApplyUpperArmTarget(
            data, g_rightHandControl,
            g_weaponAim.gripTransform.m_vPos);
    }
}

void LeftUpperArmNodeControl(
    const NodeControlData& data, void* userData) {
    (void)userData;
    if (g_twoHandedGripActive &&
        !g_twoHandedGrip.placementValid) {
        return;
    }
    if (g_weaponAim.leftGripValid) {
        ApplyUpperArmTarget(
            data, g_leftHandControl,
            EffectiveLeftHandPosition());
    }
}

void RightForearmNodeControl(
    const NodeControlData& data, void* userData) {
    (void)userData;
    if (g_twoHandedGripActive &&
        !g_twoHandedGrip.placementValid) {
        return;
    }
    if (!g_weaponAim.gripValid) {
        return;
    }
    if (ApplyForearmTarget(
            data, g_rightHandControl,
            g_weaponAim.gripTransform.m_vPos)) {
        if (InterlockedCompareExchange(
                &g_rightForearmTrackingActiveLogged, 1, 0) == 0) {
            Report(
                "INFO", "right_forearm_tracking_active",
                "Right_arml rotates from its animated elbow toward "
                "the right OpenXR grip.");
        }
    }
}

void LeftForearmNodeControl(
    const NodeControlData& data, void* userData) {
    (void)userData;
    if (g_twoHandedGripActive &&
        !g_twoHandedGrip.placementValid) {
        return;
    }
    if (!g_weaponAim.leftGripValid) {
        return;
    }
    if (ApplyForearmTarget(
            data, g_leftHandControl,
            EffectiveLeftHandPosition())) {
        if (InterlockedCompareExchange(
                &g_leftForearmTrackingActiveLogged, 1, 0) == 0) {
            Report(
                "INFO", "left_forearm_tracking_active",
                "Left_arml rotates from its animated elbow toward "
                "the left OpenXR grip.");
        }
    }
}

void RightHandNodeControl(
    const NodeControlData& data, void* userData) {
    (void)userData;
    if (!g_weaponAim.valid || !g_weaponAim.gripValid) {
        return;
    }
    CaptureOriginalTwoHandSocket(
        data, g_rightHandControl, true);
    const LTRigidTransform desiredSocket(
        g_weaponAim.gripTransform.m_vPos,
        g_weaponAim.fireTransform.m_rRot);
    if (ApplyHandSocketPose(
            data, g_rightHandControl, desiredSocket) &&
        InterlockedCompareExchange(
            &g_weaponHandTrackingActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "weapon_hand_tracking_active",
            "The Retail RightHand socket exactly follows OpenXR grip "
            "position and the same aim rotation as bullets.");
    }
}

void LeftHandNodeControl(
    const NodeControlData& data, void* userData) {
    (void)userData;
    if (!g_weaponAim.leftGripValid) {
        return;
    }
    CaptureOriginalTwoHandSocket(
        data, g_leftHandControl, false);
    const LTRigidTransform desiredSocket(
        EffectiveLeftHandPosition(), EffectiveLeftHandRotation());
    if (ApplyHandSocketPose(
            data, g_leftHandControl,
            desiredSocket) &&
        InterlockedCompareExchange(
            &g_leftHandTrackingActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "left_hand_tracking_active",
            "The Retail LeftHand socket follows the anatomical OpenXR grip "
            "pose; aim and pistol orientation remain independent.");
    }
}

void BodyPresentationNodeControl(
    const NodeControlData& data, void* userData) {
    (void)userData;
    if (!g_bodyPresentationOffsetActive ||
        !g_bodyPresentationNodeControl.installed ||
        data.m_hModel != g_playerBodyObject ||
        data.m_hNode != g_bodyPresentationNodeControl.node ||
        data.m_pModelTransform == nullptr ||
        data.m_pNodeTransform == nullptr) {
        return;
    }

    // Move only the rendered skeleton. Moving the HOBJECT while RenderCamera
    // is building visibility lists can invalidate the engine's spatial
    // bookkeeping and intermittently cull unrelated world models.
    const LTVector modelOffset =
        data.m_pModelTransform->m_rRot.Conjugate().RotateVector(
            g_bodyPresentationWorldOffset);
    data.m_pNodeTransform->m_vPos += modelOffset;
}

void RemoveHandNodeControls() noexcept {
    __try {
        ILTModel* const model =
            g_client != nullptr ? g_client->GetModelLT() : nullptr;
        if (model != nullptr && g_playerBodyObject != nullptr) {
            if (g_bodyPresentationNodeControl.installed) {
                model->RemoveNodeControlFn(
                    g_playerBodyObject,
                    g_bodyPresentationNodeControl.node,
                    &BodyPresentationNodeControl,
                    &g_playerBodyObject);
            }
            if (g_rightHandControl.upperArmInstalled) {
                model->RemoveNodeControlFn(
                    g_playerBodyObject,
                    g_rightHandControl.upperArmNode,
                    &RightUpperArmNodeControl,
                    &g_playerBodyObject);
            }
            if (g_rightHandControl.forearmInstalled) {
                model->RemoveNodeControlFn(
                    g_playerBodyObject,
                    g_rightHandControl.forearmNode,
                    &RightForearmNodeControl,
                    &g_playerBodyObject);
            }
            if (g_rightHandControl.installed) {
                model->RemoveNodeControlFn(
                    g_playerBodyObject, g_rightHandControl.node,
                    &RightHandNodeControl, &g_playerBodyObject);
            }
            if (g_leftHandControl.upperArmInstalled) {
                model->RemoveNodeControlFn(
                    g_playerBodyObject,
                    g_leftHandControl.upperArmNode,
                    &LeftUpperArmNodeControl,
                    &g_playerBodyObject);
            }
            if (g_leftHandControl.forearmInstalled) {
                model->RemoveNodeControlFn(
                    g_playerBodyObject,
                    g_leftHandControl.forearmNode,
                    &LeftForearmNodeControl,
                    &g_playerBodyObject);
            }
            if (g_leftHandControl.installed) {
                model->RemoveNodeControlFn(
                    g_playerBodyObject, g_leftHandControl.node,
                    &LeftHandNodeControl, &g_playerBodyObject);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_playerBodyObject = nullptr;
    g_rightHandControl = HandNodeControlState{};
    g_leftHandControl = HandNodeControlState{};
    g_bodyPresentationNodeControl =
        BodyPresentationNodeControlState{};
    InterlockedExchange(&g_armGeometryInspectedLogged, 0);
    InterlockedExchange(&g_armGeometryEmptyAttempts, 0);
    InterlockedExchange(&g_armGeometryNeverAvailableLogged, 0);
    InterlockedExchange(&g_bodyMaterialOverrideLogged, 0);
}

bool InstallHandNodeControl(
    ILTModel* model, HOBJECT playerBody,
    const char* nodeName, const char* upperArmNodeName,
    const char* forearmNodeName,
    const char* socketName, NodeControlFn handCallback,
    NodeControlFn upperArmCallback, NodeControlFn forearmCallback,
    float elbowSideSign,
    HandNodeControlState& control,
    const char* installedEvent) noexcept {
    HMODELNODE node = INVALID_MODEL_NODE;
    HMODELNODE upperArmNode = INVALID_MODEL_NODE;
    HMODELNODE forearmNode = INVALID_MODEL_NODE;
    HMODELSOCKET socket = INVALID_MODEL_SOCKET;
    LTTransform nodeWorld;
    LTTransform upperArmWorld;
    LTTransform forearmWorld;
    LTTransform socketWorld;
    if (model->GetNode(playerBody, nodeName, node) != LT_OK ||
        node == INVALID_MODEL_NODE ||
        model->GetNode(
            playerBody, upperArmNodeName, upperArmNode) != LT_OK ||
        upperArmNode == INVALID_MODEL_NODE ||
        model->GetNode(
            playerBody, forearmNodeName, forearmNode) != LT_OK ||
        forearmNode == INVALID_MODEL_NODE ||
        model->GetSocket(playerBody, socketName, socket) != LT_OK ||
        socket == INVALID_MODEL_SOCKET ||
        model->GetNodeTransform(
            playerBody, node, nodeWorld, true) != LT_OK ||
        model->GetNodeTransform(
            playerBody, upperArmNode, upperArmWorld, true) != LT_OK ||
        model->GetNodeTransform(
            playerBody, forearmNode, forearmWorld, true) != LT_OK ||
        model->GetSocketTransform(
            playerBody, socket, socketWorld, true) != LT_OK) {
        return false;
    }

    control.node = node;
    control.elbowSideSign = elbowSideSign;
    control.socketFromNode =
        nodeWorld.GetInverse() * socketWorld;
    control.socketFromNodeValid = true;
    control.upperArmNode = upperArmNode;
    control.forearmOffsetFromUpperArm =
        (upperArmWorld.GetInverse() * forearmWorld).m_vPos;
    control.forearmOffsetFromUpperArmValid =
        control.forearmOffsetFromUpperArm.MagSqr() > 0.0001F;
    control.forearmNode = forearmNode;
    control.socketOffsetFromForearm =
        (forearmWorld.GetInverse() * socketWorld).m_vPos;
    control.socketOffsetFromForearmValid =
        control.socketOffsetFromForearm.MagSqr() > 0.0001F;
    if (!control.forearmOffsetFromUpperArmValid ||
        !control.socketOffsetFromForearmValid ||
        model->AddNodeControlFn(
            playerBody, upperArmNode, upperArmCallback,
            &g_playerBodyObject) != LT_OK) {
        control = HandNodeControlState{};
        return false;
    }
    control.upperArmInstalled = true;
    if (model->AddNodeControlFn(
            playerBody, forearmNode, forearmCallback,
            &g_playerBodyObject) != LT_OK) {
        model->RemoveNodeControlFn(
            playerBody, upperArmNode, upperArmCallback,
            &g_playerBodyObject);
        control = HandNodeControlState{};
        return false;
    }
    control.forearmInstalled = true;
    if (model->AddNodeControlFn(
            playerBody, node, handCallback,
            &g_playerBodyObject) != LT_OK) {
        model->RemoveNodeControlFn(
            playerBody, forearmNode, forearmCallback,
            &g_playerBodyObject);
        model->RemoveNodeControlFn(
            playerBody, upperArmNode, upperArmCallback,
            &g_playerBodyObject);
        control = HandNodeControlState{};
        return false;
    }
    control.installed = true;
    char message[192]{};
    std::snprintf(
        message, sizeof(message),
        "Retail upper arm '%s', forearm '%s', hand '%s' and socket "
        "'%s' installed with measured two-bone lengths.",
        upperArmNodeName, forearmNodeName, nodeName, socketName);
    Report("INFO", installedEvent, message);
    return true;
}

// Appends to a fixed buffer and keeps `used` pointing at the terminator.
void AppendToList(
    char* list, std::size_t capacity, std::size_t& used,
    const char* text) noexcept {
    if (used + 1 >= capacity) {
        return;
    }
    const int written = std::snprintf(
        list + used, capacity - used, "%s%s",
        used == 0 ? "" : ", ", text);
    if (written > 0) {
        used += (std::min)(
            static_cast<std::size_t>(written),
            capacity - used - 1);
    }
}

// Lowercases into a caller-owned buffer so name matching is case safe.
void CopyLowerCase(
    char* destination, std::size_t capacity,
    const char* source) noexcept {
    std::size_t character = 0;
    for (; character + 1 < capacity && source[character] != '\0';
         ++character) {
        const char value = source[character];
        destination[character] =
            value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a')
                : value;
    }
    destination[character] = '\0';
}

// One-shot dump of everything the Retail player-body model exposes. The
// arm geometry can only be hidden reliably once the real piece, material
// and node names are known, so log them instead of guessing further.
void LogRetailPlayerBodyGeometry(
    ILTModel* model, HOBJECT playerBody,
    std::uint32_t pieceCount) noexcept {
    char modelFile[192]{};
    if (model->GetModelFilename(
            playerBody, modelFile,
            static_cast<std::uint32_t>(sizeof(modelFile))) != LT_OK ||
        modelFile[0] == '\0') {
        std::snprintf(modelFile, sizeof(modelFile), "<unavailable>");
    }
    std::uint32_t nodeCount = 0;
    if (model->GetNumNodes(playerBody, nodeCount) != LT_OK) {
        nodeCount = 0;
    }
    char summary[320]{};
    std::snprintf(
        summary, sizeof(summary),
        "Retail player body model '%s' reports %u pieces and %u nodes.",
        modelFile, pieceCount, nodeCount);
    Report("INFO", "vr_player_body_geometry", summary);

    char materialList[512]{};
    std::size_t materialUsed = 0;
    for (std::uint32_t index = 0; index < 16U; ++index) {
        char materialFile[128]{};
        if (model->GetMaterialFilename(
                playerBody, index, materialFile,
                static_cast<std::uint32_t>(sizeof(materialFile))) !=
                LT_OK ||
            materialFile[0] == '\0') {
            break;
        }
        char entry[160]{};
        std::snprintf(
            entry, sizeof(entry), "%u=%s", index, materialFile);
        AppendToList(
            materialList, sizeof(materialList), materialUsed, entry);
    }
    char materialMessage[640]{};
    std::snprintf(
        materialMessage, sizeof(materialMessage),
        "Retail player-body materials: %s",
        materialList[0] != '\0' ? materialList : "<none>");
    Report("INFO", "vr_player_body_materials", materialMessage);

    // Node names arrive in pre-order, so log them in chunks rather than
    // truncating a single oversized line.
    HMODELNODE node = INVALID_MODEL_NODE;
    char nodeList[400]{};
    std::size_t nodeUsed = 0;
    std::uint32_t logged = 0;
    for (HMODELNODE next = INVALID_MODEL_NODE;
         logged < 160U &&
         model->GetNextNode(playerBody, node, next) == LT_OK &&
         next != INVALID_MODEL_NODE;
         node = next) {
        char nodeName[64]{};
        if (model->GetNodeName(
                playerBody, next, nodeName,
                static_cast<std::uint32_t>(sizeof(nodeName))) != LT_OK) {
            std::snprintf(nodeName, sizeof(nodeName), "<unnamed>");
        }
        ++logged;
        if (nodeUsed + std::strlen(nodeName) + 2 >= sizeof(nodeList)) {
            Report("INFO", "vr_player_body_nodes", nodeList);
            nodeList[0] = '\0';
            nodeUsed = 0;
        }
        AppendToList(nodeList, sizeof(nodeList), nodeUsed, nodeName);
    }
    if (nodeList[0] != '\0') {
        Report("INFO", "vr_player_body_nodes", nodeList);
    }
}

// Retail player.Model00p stores no piece names, so visibility is driven by
// piece index. Re-applied every weapon update because a model reload resets
// the hide flags.
void ApplyPlayerBodyPieceMask(
    ILTModel* model, HOBJECT playerBody,
    std::uint32_t pieceCount) noexcept {
    if (g_disableBodyPieceHiding) {
        return;
    }
    // SetPieceHideStatus only covers the first 32 pieces of a model.
    const std::uint32_t supportedCount = (std::min)(pieceCount, 32U);
    for (std::uint32_t index = 0; index < supportedCount; ++index) {
        HMODELPIECE piece = INVALID_MODEL_PIECE;
        if (model->GetPiece(playerBody, index, piece) != LT_OK ||
            piece == INVALID_MODEL_PIECE) {
            continue;
        }
        const bool hidden =
            (g_hiddenBodyPieceMask & (1U << index)) != 0;
        model->SetPieceHideStatus(playerBody, piece, hidden);
    }
}

// Mask that leaves only piece `index` of a four-piece model visible.
std::uint32_t IsolatePieceMask(std::uint32_t index) noexcept {
    return kPlayerBodyPieceMaskAll & ~(1U << index);
}

// F11 isolates one player-body piece at a time. Showing a single piece names
// it far better than hiding one does, because Retail stores no piece names.
// Step 0 is the persisted default, so the probe is only ever needed once.
void PollBodyPieceProbeKey() noexcept {
    const bool keyDown = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    if (!keyDown || g_bodyPieceProbeKeyWasDown) {
        g_bodyPieceProbeKeyWasDown = keyDown;
        return;
    }
    g_bodyPieceProbeKeyWasDown = true;
    g_bodyPieceProbeStep = (g_bodyPieceProbeStep + 1) % 6U;
    char message[224]{};
    if (g_bodyPieceProbeStep == 0) {
        g_hiddenBodyPieceMask = 0;
        std::snprintf(
            message, sizeof(message),
            "F11 body piece probe: all player-body pieces visible.");
    } else if (g_bodyPieceProbeStep == 5U) {
        g_hiddenBodyPieceMask = kPlayerBodyPieceMaskAll;
        std::snprintf(
            message, sizeof(message),
            "F11 body piece probe: every player-body piece hidden "
            "(mask 0x%X).",
            g_hiddenBodyPieceMask);
    } else {
        const std::uint32_t visible = g_bodyPieceProbeStep - 1U;
        g_hiddenBodyPieceMask = IsolatePieceMask(visible);
        std::snprintf(
            message, sizeof(message),
            "F11 body piece probe: only player-body piece #%u is visible "
            "(mask 0x%X). This is a diagnostic isolation step; return to "
            "step 0 for the VR body material.",
            visible, g_hiddenBodyPieceMask);
    }
    WriteVrSetting(
        L"HiddenBodyPieces",
        static_cast<int>(g_hiddenBodyPieceMask));
    Report("INFO", "vr_body_piece_probe", message);
}

void ConfigureRetailArmPieceVisibility(
    ILTModel* model, HOBJECT playerBody) noexcept {
    if (model == nullptr || playerBody == nullptr) {
        return;
    }
    PollBodyPieceProbeKey();
    const char* const bodyMaterial = g_showPlayerArms
        ? "chars\\materials\\player_new.Mat00"
        : "fearvr\\player_body.Mat00";
    const LTRESULT bodyMaterialResult = model->SetMaterialFilename(
        playerBody, 0, bodyMaterial);
    const char* const hiddenHeadMaterial =
        "fearvr\\player_hidden.Mat00";
    const LTRESULT headMaterialResult = model->SetMaterialFilename(
        playerBody, 1, hiddenHeadMaterial);
    if (InterlockedCompareExchange(
            &g_bodyMaterialOverrideLogged, 1, 0) == 0) {
        char materialMessage[256]{};
        std::snprintf(
            materialMessage, sizeof(materialMessage),
            "Player body material slot 0 was set to %s "
            "(result %ld; arms %s); slot 1 uses %s "
            "(result %ld; head hidden).",
            bodyMaterial, static_cast<long>(bodyMaterialResult),
            g_showPlayerArms ? "visible" : "hidden",
            hiddenHeadMaterial,
            static_cast<long>(headMaterialResult));
        const bool materialsApplied =
            bodyMaterialResult == LT_OK &&
            headMaterialResult == LT_OK;
        Report(
            materialsApplied ? "INFO" : "WARN",
            materialsApplied
                ? "vr_body_material_override"
                : "vr_body_material_override_failed",
            materialMessage);
    }
    if (InterlockedCompareExchange(
            &g_armGeometryInspectedLogged, 0, 0) != 0) {
        std::uint32_t currentCount = 0;
        if (model->GetNumPieces(playerBody, currentCount) == LT_OK) {
            ApplyPlayerBodyPieceMask(model, playerBody, currentCount);
        }
        return;
    }

    std::uint32_t pieceCount = 0;
    if (model->GetNumPieces(playerBody, pieceCount) != LT_OK ||
        pieceCount == 0) {
        // The player-body object can exist one or more updates before its
        // render model has finished loading. Do not cache that transient
        // empty result; retry on the next weapon update. Report once if the
        // model never starts reporting pieces so the log stays conclusive.
        if (InterlockedIncrement(&g_armGeometryEmptyAttempts) >= 600 &&
            InterlockedCompareExchange(
                &g_armGeometryNeverAvailableLogged, 1, 0) == 0) {
            LogRetailPlayerBodyGeometry(model, playerBody, 0);
            Report(
                "WARN", "vr_arm_pieces_never_available",
                "The Retail player body kept reporting zero pieces across "
                "600 weapon updates, so piece hiding cannot be used.");
        }
        return;
    }

    LogRetailPlayerBodyGeometry(model, playerBody, pieceCount);

    bool namedArmPiece = false;
    char pieceList[640]{};
    std::size_t used = 0;
    // SetPieceHideStatus only covers the first 32 pieces of a model.
    const std::uint32_t supportedCount =
        (std::min)(pieceCount, 32U);
    for (std::uint32_t index = 0;
         index < supportedCount; ++index) {
        HMODELPIECE piece = INVALID_MODEL_PIECE;
        char name[64]{};
        const LTRESULT pieceResult =
            model->GetPiece(playerBody, index, piece);
        if (pieceResult != LT_OK || piece == INVALID_MODEL_PIECE) {
            char entry[64]{};
            std::snprintf(
                entry, sizeof(entry), "#%u=<GetPiece %ld>",
                index, static_cast<long>(pieceResult));
            AppendToList(pieceList, sizeof(pieceList), used, entry);
            continue;
        }
        const LTRESULT nameResult = model->GetPieceName(
            playerBody, piece, name,
            static_cast<std::uint32_t>(sizeof(name)));
        if (nameResult != LT_OK) {
            char entry[64]{};
            std::snprintf(
                entry, sizeof(entry), "#%u=<GetPieceName %ld>",
                index, static_cast<long>(nameResult));
            AppendToList(pieceList, sizeof(pieceList), used, entry);
            continue;
        }

        char lowerName[64]{};
        CopyLowerCase(lowerName, sizeof(lowerName), name);
        const bool isHand =
            std::strstr(lowerName, "hand") != nullptr ||
            std::strstr(lowerName, "glove") != nullptr ||
            std::strstr(lowerName, "finger") != nullptr;
        const bool isArm =
            !isHand &&
            (std::strstr(lowerName, "arm") != nullptr ||
             std::strstr(lowerName, "sleeve") != nullptr ||
             std::strstr(lowerName, "elbow") != nullptr ||
             std::strstr(lowerName, "shoulder") != nullptr);
        char entry[96]{};
        if (isArm) {
            namedArmPiece = true;
            std::snprintf(
                entry, sizeof(entry), "#%u=%s(arm)", index, name);
        } else {
            std::snprintf(
                entry, sizeof(entry), "#%u=%s", index, name);
        }
        AppendToList(pieceList, sizeof(pieceList), used, entry);
    }
    ApplyPlayerBodyPieceMask(model, playerBody, pieceCount);
    InterlockedExchange(&g_armGeometryInspectedLogged, 1);
    char message[768]{};
    std::snprintf(
        message, sizeof(message),
        "%s Diagnostic hidden-piece mask 0x%X. "
        "Arm visibility is controlled by the Show arms material switch. "
        "Retail player-body pieces: %s",
        namedArmPiece
            ? "Named arm pieces were found."
            : "No separately named arm pieces were available; Retail "
              "Body_Group combines arms, torso and legs.",
        g_hiddenBodyPieceMask,
        pieceList[0] != '\0' ? pieceList : "<none>");
    Report(
        "INFO",
        namedArmPiece
            ? "vr_arm_pieces_available"
            : "vr_arm_material_required",
        message);
}

void EnsureHandNodeControls(HOBJECT playerBody) noexcept {
    if (g_disableHandNodes) {
        return;
    }
    const int gameState = ReadRetailGameState();
    if (gameState >= 0 &&
        gameState != kRetailGameStatePlaying) {
        // Nach dem Invalidieren in GS_LOADINGLEVEL darf ein spaeter
        // Node-Tracker-Aufruf die alten Bindungen nicht schon auf einem
        // Lade-/Postload-Modell neu installieren. Erst GS_PLAYING garantiert,
        // dass PlayerBody und sein D3DX-Skelett wieder dauerhaft gehoeren.
        return;
    }
    if (playerBody == nullptr || g_client == nullptr) {
        return;
    }
    if (g_playerBodyObject != nullptr &&
        playerBody != g_playerBodyObject) {
        RemoveHandNodeControls();
    }
    __try {
        ILTModel* const model = g_client->GetModelLT();
        if (model == nullptr) {
            return;
        }
        g_playerBodyObject = playerBody;
        ConfigureRetailArmPieceVisibility(model, playerBody);
        if (!g_bodyPresentationNodeControl.installed) {
            HMODELNODE presentationNode = INVALID_MODEL_NODE;
            if ((model->GetNode(
                     playerBody, "null2",
                     presentationNode) == LT_OK ||
                 model->GetNode(
                     playerBody, "translation",
                     presentationNode) == LT_OK) &&
                presentationNode != INVALID_MODEL_NODE &&
                model->AddNodeControlFn(
                    playerBody, presentationNode,
                    &BodyPresentationNodeControl,
                    &g_playerBodyObject) == LT_OK) {
                g_bodyPresentationNodeControl.node =
                    presentationNode;
                g_bodyPresentationNodeControl.installed = true;
                Report(
                    "INFO", "body_presentation_node_installed",
                    "Room-scale body correction uses a skeleton node and "
                    "does not move the world HOBJECT during rendering.");
            }
        }
        if (g_rightHandControl.installed &&
            g_rightHandControl.upperArmInstalled &&
            g_rightHandControl.forearmInstalled &&
            g_leftHandControl.installed &&
            g_leftHandControl.upperArmInstalled &&
            g_leftHandControl.forearmInstalled) {
            return;
        }
        bool success = true;
        if (!g_rightHandControl.installed) {
            success = InstallHandNodeControl(
                model, playerBody, "Right_hand", "Right_armu",
                "Right_arml", "RightHand", &RightHandNodeControl,
                &RightUpperArmNodeControl, &RightForearmNodeControl,
                1.0F,
                g_rightHandControl,
                "weapon_hand_tracking_installed") && success;
        }
        if (!g_leftHandControl.installed) {
            success = InstallHandNodeControl(
                model, playerBody, "Left_hand", "Left_armu",
                "Left_arml", "LeftHand", &LeftHandNodeControl,
                &LeftUpperArmNodeControl, &LeftForearmNodeControl,
                -1.0F,
                g_leftHandControl,
                "left_hand_tracking_installed") && success;
        }
        if (!success &&
            InterlockedCompareExchange(
                &g_weaponHandTrackingFailureLogged, 1, 0) == 0) {
            Report(
                "WARN", "hand_tracking_unavailable",
                "At least one Retail hand node/socket pair could not "
                "install its local OpenXR control callback.");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        RemoveHandNodeControls();
    }
}

void __fastcall HookRetailSetTrackedTarget(
    void* context, void* ignoredEdx, int group,
    const LTVector& originalTarget) {
    (void)ignoredEdx;
    HOBJECT trackerPlayerBody = nullptr;
    if (IsStaticPlayerBodyTrackerContext(context)) {
        __try {
            // CNodeTrackerContext's LTObjRef stores its HOBJECT at +0x10.
            trackerPlayerBody = *reinterpret_cast<HOBJECT*>(
                static_cast<unsigned char*>(context) + 0x10);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            trackerPlayerBody = nullptr;
        }
    }
    EnsureHandNodeControls(trackerPlayerBody);
    if (g_aimAtPassthrough || group != kRetailTrackerGroupAimAt ||
        !g_weaponAim.valid ||
        !IsStaticPlayerBodyTrackerContext(context)) {
        g_retailSetTrackedTarget(context, group, originalTarget);
        return;
    }
    LTVector right;
    LTVector up;
    LTVector forward;
    g_weaponAim.fireTransform.m_rRot.GetVectors(right, up, forward);
    const LTVector controllerTarget =
        g_weaponAim.fireTransform.m_vPos + forward * 10000.0F;
    g_retailSetTrackedTarget(context, group, controllerTarget);
    if (InterlockedCompareExchange(
            &g_weaponBodyAimActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "weapon_body_aim_active",
            "The Retail player-body AimAt target follows the right "
            "OpenXR controller while the weapon stays on its hand socket.");
    }
}

// Ein Ruckeln je Schuss, auch im Dauerfeuer. Retail holt die Fire-Vectors in
// seinen beiden Feuerpfaden genau einmal pro Schuss, nicht pro Bild — das ist
// deshalb die verlaessliche Schussquelle. Sie ist der Triggerflanke auch
// inhaltlich voraus: Bei leerem Magazin faellt die Vibration korrekt aus.
void RequestFireHaptic(bool leftWeaponFired) noexcept {
    // Beide Feuerpfade koennen denselben Schuss abfragen. Ein Mindestabstand
    // deutlich unterhalb der schnellsten Kadenz fasst das zu einem Impuls
    // zusammen, ohne echte Schuesse zu verschlucken.
    constexpr ULONGLONG kMinimumPulseGapMs = 30;
    if (g_submitHapticRequest == nullptr ||
        !g_controllerHapticsEnabled) {
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (g_lastFireHapticTick != 0 &&
        now - g_lastFireHapticTick < kMinimumPulseGapMs) {
        return;
    }
    g_lastFireHapticTick = now;

    FearVrHapticRequest request{};
    request.requestId = ++g_hapticRequestId;
    request.durationNs = 35'000'000;
    request.amplitude = 0.25F;
    request.frequency = 0.0F;
    // Der Haptikauftrag adressiert wieder einen physischen Controller und
    // muss deshalb zurueckgespiegelt werden.
    if (leftWeaponFired) {
        request.handMask = g_leftHandedBindings
            ? FEARVR_HAND_MASK_RIGHT
            : FEARVR_HAND_MASK_LEFT;
    } else {
        request.handMask = g_leftHandedBindings
            ? FEARVR_HAND_MASK_LEFT
            : FEARVR_HAND_MASK_RIGHT;
    }
    request.flags = FEARVR_HF_VALID;
    if (g_submitHapticRequest(&request) &&
        InterlockedCompareExchange(
            &g_fireHapticActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "controller_fire_haptic",
            "Every shot requests a haptic pulse, including sustained "
            "automatic fire.");
    }
}

// Queue exactly one spring impulse for each successful Retail shot. Like the
// haptic path, this is driven by GetFireVectors rather than trigger state, so
// dry fire and blocked fire modes cannot move the weapon. The pose is rebuilt
// before Retail fires in the current update, therefore the impulse is consumed
// on the following update and never changes the bullet that caused it.
void RequestWeaponRecoil() noexcept {
    if (!g_weaponRecoilEnabled) {
        return;
    }
    constexpr ULONGLONG kMinimumImpulseGapMs = 30;
    const ULONGLONG now = GetTickCount64();
    if (g_lastWeaponRecoilTick != 0 &&
        now - g_lastWeaponRecoilTick < kMinimumImpulseGapMs) {
        return;
    }
    g_lastWeaponRecoilTick = now;
    InterlockedIncrement(&g_pendingWeaponRecoilShots);
    if (InterlockedCompareExchange(
            &g_weaponRecoilActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "weapon_recoil_active",
            "Successful shots add configurable kick and muzzle-rise "
            "impulses to the tracked weapon pose.");
    }
}

bool __fastcall HookRetailGetFireVectors(
    const void* weapon, void* ignoredEdx,
    LTVector& right, LTVector& up,
    LTVector& forward, LTVector& firePosition) {
    (void)ignoredEdx;
    SelectDualPistolFromTriggers(
        const_cast<void*>(weapon));
    const bool leftWeaponFired =
        RetailWeaponFiresLeft(weapon) &&
        g_weaponAim.leftWeaponTransformValid;
    const bool result = g_retailGetFireVectors(
        weapon, right, up, forward, firePosition);
    if (result) {
        RequestFireHaptic(leftWeaponFired);
        TriggerVrMuzzleFlash(leftWeaponFired);
        RequestWeaponRecoil();
    }
    if (!result || !g_weaponAim.valid) {
        return result;
    }

    // Projektil und Zielstrahl teilen sich denselben Ursprung, siehe
    // ResolveMuzzleWorldTransform.
    LTRigidTransform muzzle;
    if (leftWeaponFired) {
        ResolveLeftMuzzleWorldTransform(muzzle);
    } else {
        UpdateRetailMuzzlePosition(weapon);
        ResolveMuzzleWorldTransform(muzzle);
    }
    muzzle.m_rRot.GetVectors(right, up, forward);
    firePosition = muzzle.m_vPos;
    return true;
}

void RemoveVrCameraCollisionHook() noexcept {
    if (g_retailCalcNoClipTarget != nullptr) {
        MH_DisableHook(g_retailCalcNoClipTarget);
        MH_RemoveHook(g_retailCalcNoClipTarget);
    }
    g_retailCalcNoClipTarget = nullptr;
    g_retailCalcNoClip = nullptr;
}

bool ResolveRetailCalcNoClipTarget(
    void*& calcNoClip) noexcept {
    calcNoClip = nullptr;
    HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return false;
    }
    auto* const base = reinterpret_cast<unsigned char*>(module);
    __try {
        const auto* const dos =
            reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
            dos->e_lfanew <= 0) {
            return false;
        }
        const auto* const nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(
                base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->FileHeader.TimeDateStamp !=
                kRetailGameClientTimeDateStamp ||
            nt->OptionalHeader.SizeOfImage !=
                kRetailGameClientSizeOfImage) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    auto* const target = base + kRetailCalcNoClipRva;
    constexpr unsigned char kCalcNoClipPrefix[] = {
        0x81, 0xEC, 0x9C, 0x00, 0x00, 0x00,
        0x8B, 0x84, 0x24, 0xA4, 0x00, 0x00, 0x00,
        0xD9, 0x00, 0x56, 0xD8, 0x48, 0x04, 0x57};
    if (!MatchesCode(
            target, kCalcNoClipPrefix,
            sizeof(kCalcNoClipPrefix))) {
        return false;
    }
    calcNoClip = target;
    return true;
}

bool InstallVrCameraCollisionHook() noexcept {
    void* target = nullptr;
    if (!ResolveRetailCalcNoClipTarget(target)) {
        Report(
            "ERROR", "vr_camera_collision_layout_mismatch",
            "Retail 1.08 camera raycast signature did not match; "
            "character filtering remains disabled.");
        return false;
    }

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK &&
        initialize != MH_ERROR_ALREADY_INITIALIZED) {
        Report(
            "ERROR", "vr_camera_collision_hook_initialize_failed",
            MH_StatusToString(initialize));
        return false;
    }

    g_retailCalcNoClipTarget = target;
    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookRetailCalcNoClip),
        reinterpret_cast<void**>(&g_retailCalcNoClip));
    if (status == MH_OK) {
        status = MH_EnableHook(target);
    }
    if (status != MH_OK) {
        Report(
            "ERROR", "vr_camera_collision_hook_install_failed",
            MH_StatusToString(status));
        RemoveVrCameraCollisionHook();
        return false;
    }
    Report(
        "INFO", "vr_camera_collision_filter_installed",
        "Verified Retail camera anti-clipping rays ignore character and "
        "corpse hitboxes in native VR.");
    return true;
}

void RemoveWeaponAimHooks() noexcept {
    RestorePreRenderCameraOverride();
    ResetTwoHandedJumpCamera(g_twoHandedJumpCamera);
    g_twoHandedCameraPosition = TwoHandedCameraPositionState{};
    RemoveVrMuzzleFlashObjects();
    RemoveFlashlightModel();
    RemoveHandNodeControls();
    g_devMenu.open = false;
    g_devMenu.anchorValid = false;
    g_devMenu.pointerValid = false;
    g_devMenu.suppressUntilRelease = false;
    if (g_retailWeaponManagerUpdateTarget != nullptr) {
        MH_DisableHook(g_retailWeaponManagerUpdateTarget);
        MH_RemoveHook(g_retailWeaponManagerUpdateTarget);
    }
    if (g_retailStartMuzzleFlashTarget != nullptr) {
        MH_DisableHook(g_retailStartMuzzleFlashTarget);
        MH_RemoveHook(g_retailStartMuzzleFlashTarget);
    }
    if (g_retailGetFireVectorsTarget != nullptr) {
        MH_DisableHook(g_retailGetFireVectorsTarget);
        MH_RemoveHook(g_retailGetFireVectorsTarget);
    }
    if (g_retailSetTrackedTargetTarget != nullptr) {
        MH_DisableHook(g_retailSetTrackedTargetTarget);
        MH_RemoveHook(g_retailSetTrackedTargetTarget);
    }
    g_retailWeaponManagerUpdateTarget = nullptr;
    g_retailStartMuzzleFlashTarget = nullptr;
    g_retailGetFireVectorsTarget = nullptr;
    g_retailSetTrackedTargetTarget = nullptr;
    g_retailWeaponManagerUpdate = nullptr;
    g_retailSetWeaponTransform = nullptr;
    g_retailSetWeaponVisible = nullptr;
    g_retailStartMuzzleFlash = nullptr;
    g_retailGetFireVectors = nullptr;
    g_retailSetTrackedTarget = nullptr;
    g_retailVectorObjectFilter = nullptr;
    g_retailAccuracyManager = nullptr;
    g_weaponAim.valid = false;
    g_weaponAim.gripValid = false;
    g_weaponAim.supportRightGripValid = false;
    g_weaponAim.leftAimValid = false;
    g_weaponAim.leftGripValid = false;
    g_weaponAim.muzzleValid = false;
    g_weaponAim.muzzleDirectionValid = false;
    g_weaponAim.muzzleLocalValid = false;
    g_weaponAim.muzzleDiagnosticLogged = false;
    g_weaponAim.muzzleWeapon = nullptr;
    g_weaponAim.leftWeaponModel = nullptr;
    g_weaponAim.leftWeaponTransformValid = false;
    g_weaponAim.leftMuzzleValid = false;
    g_weaponAim.leftMuzzleLocalValid = false;
    g_weaponAim.retailWeapon = nullptr;
    g_retailWeaponUpdateInProgress = nullptr;
    g_retailVisibilityInitializedWeapon = nullptr;
    g_weaponAim.trackingBaseValid = false;
    g_supportRightGripPoseCache = TrackedPoseCache{};
    g_weightedWeaponInput = {};
    ResetWeaponWeightPair(
        g_weightedWeaponInput.filters,
        WeaponWeightResetReason::sceneLoaded);
    InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
    g_lastWeaponRecoilTick = 0;
    // Ohne Aim-Hooks laeuft kein Weapon-Manager-Update mehr, das den Griff
    // wieder loesen koennte. Sprinten und Lehnen duerfen nicht haengen.
    g_twoHandedGripActive = false;
    g_twoHandedGrip = TwoHandedGripState{};
    g_lastWeaponManagerUpdateTick = 0;
    g_flashlightEnabled = true;
    g_flashlightButtonWasDown = false;
    g_dualPistolSlowMoActive = false;
    g_weaponSwitchPulseUntil = 0;
    g_weaponSwitchTriggered = false;
    g_secondaryHoldStartTick = 0;
    g_reloadPulseUntil = 0;
    g_grenadePulseUntil = 0;
    g_secondaryWasDown = false;
    g_grenadeConsumed = false;
    g_meleePulseUntil = 0;
    g_slideDuckPulseUntil = 0;
    g_slideForwardPulseUntil = 0;
    ResetMeleeActions(g_meleeActions);
    g_retailMovement = {};
    g_retailMovementHadSnapshot = false;
    g_retailDuckWasDown = false;
    g_retailPostureDownUntil = 0;
    g_retailUnsupportedSince = 0;
    g_retailSupportedSince = 0;
    g_retailPersistentUnsupported = false;
    g_cutsceneCameraStateKnown = false;
    g_cutsceneCameraState = false;
    g_cutsceneCameraActivationTick = 0;
    g_cutsceneBodyPresentation = {};
    g_retailPostureDownVariable = nullptr;
    g_slideKickView = {};
    ResetClimbGrip(g_climbGrip);
    g_climbWasGripping = false;
    g_climbActive = false;
    g_climbOnLadder = false;
    g_climbAxis = 0.0F;
}

bool InstallWeaponAimHooks() noexcept {
    if (g_disableAimHooks) {
        Report(
            "WARN", "weapon_aim_hooks_skipped",
            "Diagnostic switch: weapon manager, AimAt tracker and fire-vector "
            "hooks were not installed.");
        return true;
    }
    void* update = nullptr;
    void* setTransform = nullptr;
    void* setVisible = nullptr;
    void* startMuzzleFlash = nullptr;
    void* fireVectors = nullptr;
    void* setTrackedTarget = nullptr;
    void* vectorObjectFilter = nullptr;
    if (!ResolveRetailWeaponTargets(
            update, setTransform, setVisible, startMuzzleFlash,
            fireVectors, setTrackedTarget, vectorObjectFilter)) {
        Report(
            "ERROR", "weapon_aim_layout_mismatch",
            "Retail 1.08 weapon transform/fire-vector signatures "
            "did not match; 6DoF weapon aiming remains disabled.");
        return false;
    }

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK &&
        initialize != MH_ERROR_ALREADY_INITIALIZED) {
        Report(
            "ERROR", "weapon_aim_hook_initialize_failed",
            MH_StatusToString(initialize));
        return false;
    }
    g_retailWeaponManagerUpdateTarget = update;
    g_retailStartMuzzleFlashTarget = startMuzzleFlash;
    g_retailGetFireVectorsTarget = fireVectors;
    g_retailSetTrackedTargetTarget = setTrackedTarget;
    g_retailSetWeaponTransform =
        reinterpret_cast<RetailSetWeaponTransformFunction>(
            setTransform);
    g_retailSetWeaponVisible =
        reinterpret_cast<RetailSetWeaponVisibleFunction>(
            setVisible);
    g_retailVectorObjectFilter =
        reinterpret_cast<ObjectFilterFn>(vectorObjectFilter);
    g_retailStartMuzzleFlash =
        reinterpret_cast<RetailStartMuzzleFlashFunction>(
            startMuzzleFlash);
    const HMODULE retailModule =
        GetModuleHandleW(L"GameOrig.dll");
    g_retailAccuracyManager =
        retailModule != nullptr
            ? reinterpret_cast<RetailAccuracyManagerFunction>(
                  reinterpret_cast<unsigned char*>(retailModule) +
                  kRetailAccuracyManagerRva)
            : nullptr;

    MH_STATUS status = MH_CreateHook(
        update,
        reinterpret_cast<void*>(&HookRetailWeaponManagerUpdate),
        reinterpret_cast<void**>(&g_retailWeaponManagerUpdate));
    if (status == MH_OK) {
        status = MH_CreateHook(
            startMuzzleFlash,
            reinterpret_cast<void*>(&HookRetailStartMuzzleFlash),
            reinterpret_cast<void**>(&g_retailStartMuzzleFlash));
    }
    if (status == MH_OK) {
        status = MH_CreateHook(
            fireVectors,
            reinterpret_cast<void*>(&HookRetailGetFireVectors),
            reinterpret_cast<void**>(&g_retailGetFireVectors));
    }
    // Der AimAt-Tracker laeuft fuer jeden Charakter-Node-Tracker, also auch
    // fuer neu gespawnte NPCs. Er ist deshalb einzeln abschaltbar.
    if (status == MH_OK && !g_disableAimAtHook) {
        status = MH_CreateHook(
            setTrackedTarget,
            reinterpret_cast<void*>(&HookRetailSetTrackedTarget),
            reinterpret_cast<void**>(&g_retailSetTrackedTarget));
    }
    if (status == MH_OK) {
        status = MH_EnableHook(update);
    }
    if (status == MH_OK) {
        status = MH_EnableHook(startMuzzleFlash);
    }
    if (status == MH_OK) {
        status = MH_EnableHook(fireVectors);
    }
    if (status == MH_OK && !g_disableAimAtHook) {
        status = MH_EnableHook(setTrackedTarget);
    }
    if (g_disableAimAtHook) {
        g_retailSetTrackedTargetTarget = nullptr;
        Report(
            "WARN", "aimat_hook_skipped",
            "Diagnostic switch: the Retail AimAt node-tracker hook was not "
            "installed.");
    }
    if (status != MH_OK) {
        Report(
            "ERROR", "weapon_aim_hook_install_failed",
            MH_StatusToString(status));
        RemoveWeaponAimHooks();
        return false;
    }
    Report(
        "INFO", "weapon_aim_hooks_installed",
        "Verified Retail 1.08 weapon visibility, body AimAt and "
        "fire-vector hooks use the right OpenXR aim pose.");
    return true;
}

void RemoveInteractionHooks() noexcept {
    if (g_retailCheckForIntersectTarget != nullptr) {
        MH_DisableHook(g_retailCheckForIntersectTarget);
        MH_RemoveHook(g_retailCheckForIntersectTarget);
        g_retailCheckForIntersectTarget = nullptr;
        g_retailCheckForIntersect = nullptr;
    }
    if (g_retailObjectDetectorUpdateTarget != nullptr) {
        MH_DisableHook(g_retailObjectDetectorUpdateTarget);
        MH_RemoveHook(g_retailObjectDetectorUpdateTarget);
        g_retailObjectDetectorUpdateTarget = nullptr;
        g_retailObjectDetectorUpdate = nullptr;
    }
    g_retailPlayerMgrPointer = nullptr;
}

bool ResolveRetailInteractionTargets(
    void*& checkForIntersect, void*& detectorUpdate,
    void**& playerMgrPointer) noexcept {
    checkForIntersect = nullptr;
    detectorUpdate = nullptr;
    playerMgrPointer = nullptr;

    HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return false;
    }
    auto* const base = reinterpret_cast<unsigned char*>(module);
    __try {
        const auto* const dos =
            reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
            dos->e_lfanew <= 0) {
            return false;
        }
        const auto* const nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(
                base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->FileHeader.TimeDateStamp !=
                kRetailGameClientTimeDateStamp ||
            nt->OptionalHeader.SizeOfImage !=
                kRetailGameClientSizeOfImage) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    auto* const intersect = base + kRetailCheckForIntersectRva;
    auto* const detector = base + kRetailObjectDetectorUpdateRva;
    // CTargetMgr::CheckForIntersect: legt den 0x88-Byte-Rahmen fuer
    // IntersectQuery an und laedt m_hTarget aus this+0x10.
    constexpr unsigned char kCheckForIntersectPrefix[] = {
        0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x53, 0x55,
        0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x10, 0x57, 0x8D,
        0x7E, 0x04};
    // ObjectDetector::Update: prueft zuerst die gesetzte Transformquelle
    // in this+0xC0.
    constexpr unsigned char kDetectorUpdatePrefix[] = {
        0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x8B, 0x86,
        0xC0, 0x00, 0x00, 0x00, 0x57, 0x33, 0xFF, 0x3B,
        0xC7};
    if (!MatchesCode(
            intersect, kCheckForIntersectPrefix,
            sizeof(kCheckForIntersectPrefix)) ||
        !MatchesCode(
            detector, kDetectorUpdatePrefix,
            sizeof(kDetectorUpdatePrefix))) {
        return false;
    }

    checkForIntersect = intersect;
    detectorUpdate = detector;
    playerMgrPointer = reinterpret_cast<void**>(
        base + kRetailPlayerMgrPointerRva);
    return true;
}

bool InstallInteractionHooks() noexcept {
    if (g_disableInteractionHooks) {
        Report(
            "WARN", "interaction_hooks_skipped",
            "Diagnostic switch: activation and pickup keep following the "
            "head-mounted view direction.");
        return true;
    }

    void* checkForIntersect = nullptr;
    void* detectorUpdate = nullptr;
    void** playerMgrPointer = nullptr;
    if (!ResolveRetailInteractionTargets(
            checkForIntersect, detectorUpdate, playerMgrPointer)) {
        Report(
            "ERROR", "interaction_layout_mismatch",
            "Retail 1.08 activation and object-detector signatures did not "
            "match; ray interaction remains disabled.");
        return false;
    }

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK &&
        initialize != MH_ERROR_ALREADY_INITIALIZED) {
        Report(
            "ERROR", "interaction_hook_initialize_failed",
            MH_StatusToString(initialize));
        return false;
    }

    g_retailPlayerMgrPointer = playerMgrPointer;
    MH_STATUS status = MH_CreateHook(
        checkForIntersect,
        reinterpret_cast<void*>(&HookRetailCheckForIntersect),
        reinterpret_cast<void**>(&g_retailCheckForIntersect));
    if (status == MH_OK) {
        g_retailCheckForIntersectTarget = checkForIntersect;
        status = MH_CreateHook(
            detectorUpdate,
            reinterpret_cast<void*>(&HookRetailObjectDetectorUpdate),
            reinterpret_cast<void**>(&g_retailObjectDetectorUpdate));
    }
    if (status == MH_OK) {
        g_retailObjectDetectorUpdateTarget = detectorUpdate;
        status = MH_EnableHook(checkForIntersect);
    }
    if (status == MH_OK) {
        status = MH_EnableHook(detectorUpdate);
    }
    if (status != MH_OK) {
        Report(
            "ERROR", "interaction_hook_install_failed",
            MH_StatusToString(status));
        RemoveInteractionHooks();
        return false;
    }

    Report(
        "INFO", "interaction_hooks_installed",
        "Activation ray and pickup cone follow the VR weapon pose.");
    return true;
}

void RemoveSemanticInputHook() noexcept {
    if (g_retailGetBindingValueTarget == nullptr) {
        return;
    }
    MH_DisableHook(g_retailGetBindingValueTarget);
    MH_RemoveHook(g_retailGetBindingValueTarget);
    g_retailGetBindingValueTarget = nullptr;
    g_retailGetBindingValue = nullptr;
}

bool InstallSemanticInputHook(
    const void* clientUpdateTarget) noexcept {
    if (g_disableBindingHook) {
        Report(
            "WARN", "controller_binding_hook_skipped",
            "Diagnostic switch: the Retail binding-value hook was not "
            "installed.");
        return true;
    }
    const unsigned char* const getBindingValue =
        FindRetailGetBindingValue(clientUpdateTarget);
    if (!IsExecutableAddress(getBindingValue)) {
        Report(
            "ERROR", "controller_binding_layout_mismatch",
            "Retail CBindMgr layout did not match the verified 1.08 "
            "code path; controller commands remain disabled.");
        return false;
    }

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK &&
        initialize != MH_ERROR_ALREADY_INITIALIZED) {
        Report(
            "ERROR", "controller_binding_hook_initialize_failed",
            MH_StatusToString(initialize));
        return false;
    }

    g_retailGetBindingValueTarget =
        const_cast<unsigned char*>(getBindingValue);
    MH_STATUS status = MH_CreateHook(
        g_retailGetBindingValueTarget,
        reinterpret_cast<void*>(&HookRetailGetBindingValue),
        reinterpret_cast<void**>(&g_retailGetBindingValue));
    if (status != MH_OK) {
        Report(
            "ERROR", "controller_binding_hook_create_failed",
            MH_StatusToString(status));
        g_retailGetBindingValueTarget = nullptr;
        return false;
    }
    status = MH_EnableHook(g_retailGetBindingValueTarget);
    if (status != MH_OK) {
        Report(
            "ERROR", "controller_binding_hook_enable_failed",
            MH_StatusToString(status));
        RemoveSemanticInputHook();
        return false;
    }
    Report(
        "INFO", "controller_binding_hook_installed",
        "Verified Retail 1.08 CBindMgr command values now merge "
        "OpenXR controller input with existing bindings.");
    return true;
}

void PollControllerInput() noexcept {
    if (g_getInputState == nullptr) {
        return;
    }

    FearVrInputState input{};
    const bool received = g_getInputState(&input) != FALSE;
    const ULONGLONG now = GetTickCount64();
    if (received && input.sampleId != 0 &&
        input.sampleId != g_lastInputSampleId) {
        g_lastInputSampleId = input.sampleId;
        g_lastInputSampleTick = now;
    }
    const bool fresh =
        received && g_lastInputSampleTick != 0 &&
        now - g_lastInputSampleTick <= 250;
    if (!IsInputStateUsable(input, fresh)) {
        NeutralizeInputState(input);
    }
    // Vor der Drehgeschwindigkeit: `turnX` traegt nach dem Spiegeln den
    // physisch anderen Stick, skaliert wird aber der Drehstick.
    if (g_leftHandedBindings) {
        MirrorInputHandedness(input);
    }
    const InputVector2 assistedMovement = ApplyForwardAxisAssist(
        input.moveX, input.moveY);
    input.moveX = assistedMovement.x;
    input.moveY = assistedMovement.y;
    // Retail interprets its axes in player-body space, while VR users can
    // freely turn their head without rotating that body. Convert only the
    // locomotion stick into the current horizontal HMD frame. If tracking is
    // unavailable, fail softly to body-relative input rather than dropping a
    // held movement command.
    const bool headPoseFresh =
        g_headTracking.centered && !g_headTracking.trackingLost &&
        g_headTracking.lastFreshFrameTick != 0 &&
        now - g_headTracking.lastFreshFrameTick <= 250;
    if (g_headRelativeMovement && headPoseFresh) {
        const HeadRelativeMovement movement = RotateMovementByHeadYaw(
            g_headTracking.recenter, g_headTracking.currentCenter,
            input.moveX, input.moveY);
        if (movement.valid) {
            input.moveX = movement.x;
            input.moveY = movement.y;
        }
    }
    constexpr float kTurnSpeedScale[] = {0.75F, 1.0F, 1.25F};
    const int turnPreset = std::clamp(g_turnSpeedPreset, 0, 2);
    input.turnX = std::clamp(
        input.turnX * kTurnSpeedScale[turnPreset], -1.0F, 1.0F);
    g_currentInput = input;
    const bool trackingFresh =
        g_headTracking.centered && !g_headTracking.trackingLost &&
        g_headTracking.lastFreshFrameTick != 0 &&
        now - g_headTracking.lastFreshFrameTick <= 250;
    const bool wasPhysicallyDucking = g_physicalDuckActive;
    g_physicalDuckActive = UpdatePhysicalDuck(
        g_physicalDuckEnabled, trackingFresh,
        g_headTracking.currentCenter.py,
        g_headTracking.recenter.py,
        g_physicalDuckActive);
    if (g_physicalDuckActive != wasPhysicallyDucking) {
        Report(
            "INFO",
            g_physicalDuckActive
                ? "physical_duck_engaged"
                : "physical_duck_released",
            g_physicalDuckActive
                ? "Headset lowered by 26cm; Retail crouch is active."
                : "Headset returned above the 18cm release height.");
    }
    // Der Griff wird im Weapon-Manager-Update geloest. Steht das still —
    // Menue, Zwischensequenz, Ladebildschirm — bleibt der letzte Zustand
    // sonst stehen und haelt Sprinten und Lehnen dauerhaft zurueck.
    if (g_twoHandedGripActive &&
        input.squeeze[FEARVR_HAND_LEFT] <
            kTwoHandReleaseSqueeze) {
        ReleaseTwoHandedGrip();
    }
    if (input.activeHands != g_lastActiveHands) {
        char message[96]{};
        std::snprintf(
            message, sizeof(message),
            "active_hands=0x%X", input.activeHands);
        Report("INFO", "client_input_devices_changed", message);
        g_lastActiveHands = input.activeHands;
    }
    if (input.buttons != g_lastInputButtons) {
        char message[96]{};
        std::snprintf(
            message, sizeof(message),
            "buttons=0x%X", input.buttons);
        Report("INFO", "client_input_buttons_changed", message);
        g_lastInputButtons = input.buttons;
    }

}

void TapMenuKey(int key) noexcept {
    if (g_clientShell == nullptr) {
        return;
    }
    __try {
        g_clientShell->OnKeyDown(key, 1);
        g_clientShell->OnKeyUp(key);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Report(
            "WARN", "controller_menu_key_failed",
            "Retail IClientShell rejected a synthesized menu key.");
    }
}

void UpdateMenuAxis(
    std::size_t index, bool pressed, int key,
    ULONGLONG now) noexcept {
    constexpr ULONGLONG kInitialRepeatDelayMs = 350;
    constexpr ULONGLONG kRepeatDelayMs = 120;
    if (!pressed) {
        g_menuAxisDown[index] = false;
        g_menuAxisRepeatTick[index] = 0;
        return;
    }
    if (!g_menuAxisDown[index]) {
        g_menuAxisDown[index] = true;
        g_menuAxisRepeatTick[index] =
            now + kInitialRepeatDelayMs;
        TapMenuKey(key);
        return;
    }
    if (now >= g_menuAxisRepeatTick[index]) {
        g_menuAxisRepeatTick[index] = now + kRepeatDelayMs;
        TapMenuKey(key);
    }
}

void PollFlashlightToggle() noexcept {
    const bool flashlightButtonDown =
        (g_currentInput.activeHands & FEARVR_HAND_MASK_LEFT) != 0 &&
        (g_currentInput.buttons & FEARVR_IB_LEFT_PRIMARY) != 0;
    if (DualPistolsVrActive()) {
        // Mit zwei Pistolen gehoeren beide Trigger den Waffen. X uebernimmt
        // dann direkt Slow-Mo; der aktuelle Lichtzustand bleibt unangetastet.
        g_dualPistolSlowMoActive = flashlightButtonDown;
        g_flashlightButtonWasDown = flashlightButtonDown;
        return;
    }

    g_dualPistolSlowMoActive = false;
    if (flashlightButtonDown && !g_flashlightButtonWasDown) {
        g_flashlightEnabled = !g_flashlightEnabled;
        Report(
            "INFO", "flashlight_toggled",
            g_flashlightEnabled
                ? "X enabled the flashlight."
                : "X disabled the flashlight.");
    }
    g_flashlightButtonWasDown = flashlightButtonDown;
}

// Sucht den Zeiger auf `CInterfaceMgr` und belegt dabei, dass der
// Spielzustand in dieser Binary wirklich bei +0x08 liegt.
//
// Zwei unabhaengige Proben:
//   1. Die Ladestelle in CInterfaceResMgr::DrawScreen (`mov ecx,[imm32]`)
//      liefert die relozierte Adresse des Globals. Passt sie nicht zur
//      erwarteten RVA, ist das Layout ein anderes.
//   2. Zwei kleine Zugriffsfunktionen lesen den Zustand bei +0x08 — einmal
//      als Menuetest, einmal als Sprungtabelle ueber genau zehn Werte.
void ResolveRetailGameStatePointer() noexcept {
    if (g_retailGameStateResolveAttempted) {
        return;
    }
    g_retailGameStateResolveAttempted = true;
    if (g_disableRetailGameState) {
        Report(
            "WARN", "retail_game_state_skipped",
            "Diagnostic switch: the flat-panel decision falls back to the "
            "weapon-manager freshness heuristic.");
        return;
    }

    HMODULE module = GetModuleHandleW(L"GameOrig.dll");
    if (module == nullptr) {
        return;
    }
    auto* const base = reinterpret_cast<unsigned char*>(module);
    __try {
        const auto* const dos =
            reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
            dos->e_lfanew <= 0) {
            return;
        }
        const auto* const nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(
                base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->FileHeader.TimeDateStamp !=
                kRetailGameClientTimeDateStamp ||
            nt->OptionalHeader.SizeOfImage !=
                kRetailGameClientSizeOfImage) {
            return;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    // cmp dword ptr [ecx+8], 5 (GS_MENU); setne al; ret
    constexpr unsigned char kMenuTest[] = {
        0x83, 0x79, 0x08, 0x05, 0x0F, 0x95, 0xC0, 0xC3};
    // mov al,[ecx+0x370C]; test al,al; jne …; mov eax,[ecx+8];
    // cmp eax, 9 (GS_MOVIE); ja …
    constexpr unsigned char kStateSwitch[] = {
        0x8A, 0x81, 0x0C, 0x37, 0x00, 0x00, 0x84, 0xC0,
        0x75, 0x41, 0x8B, 0x41, 0x08, 0x83, 0xF8,
        static_cast<unsigned char>(kRetailGameStateCount - 1)};
    if (!MatchesCode(
            base + kRetailGameStateMenuTestRva, kMenuTest,
            sizeof(kMenuTest)) ||
        !MatchesCode(
            base + kRetailGameStateSwitchRva, kStateSwitch,
            sizeof(kStateSwitch))) {
        Report(
            "ERROR", "retail_game_state_layout_mismatch",
            "The Retail game-state accessors did not match; the flat-panel "
            "decision falls back to the weapon-manager heuristic.");
        return;
    }

    const void* const* candidate = nullptr;
    __try {
        auto* const loadSite = base + kRetailInterfaceMgrLoadSiteRva;
        // mov ecx, dword ptr [imm32]
        if (loadSite[0] != 0x8B || loadSite[1] != 0x0D) {
            Report(
                "ERROR", "retail_game_state_layout_mismatch",
                "The interface-manager load site is not the expected "
                "absolute move.");
            return;
        }
        std::uintptr_t address = 0;
        std::memcpy(&address, loadSite + 2, sizeof(address));
        if (address !=
            reinterpret_cast<std::uintptr_t>(
                base + kRetailInterfaceMgrPointerRva)) {
            Report(
                "ERROR", "retail_game_state_layout_mismatch",
                "The interface-manager global does not sit at the expected "
                "relative address.");
            return;
        }
        candidate = reinterpret_cast<const void* const*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    g_retailInterfaceMgrPointer = candidate;
    Report(
        "INFO", "retail_game_state_available",
        "The Retail interface manager reports the game state; fullscreen "
        "screens now select the flat panel directly.");
}

// Der aktuelle Retail-Spielzustand, oder -1 solange er nicht lesbar ist.
int ReadRetailGameState() noexcept {
    ResolveRetailGameStatePointer();
    if (g_retailInterfaceMgrPointer == nullptr) {
        return -1;
    }
    int state = -1;
    __try {
        const auto* const manager =
            reinterpret_cast<const unsigned char*>(
                *g_retailInterfaceMgrPointer);
        if (manager == nullptr) {
            return -1;
        }
        std::int32_t value = 0;
        std::memcpy(
            &value, manager + kRetailInterfaceMgrGameStateOffset,
            sizeof(value));
        if (value < 0 || value >= kRetailGameStateCount) {
            return -1;
        }
        state = static_cast<int>(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
    if (state != g_lastReportedRetailGameState) {
        // `GS_EXITINGLEVEL` und `GS_LOADINGLEVEL` zerstoeren die aktuelle
        // Client-Welt. Die bislang alleinige Erkennung ueber eine Pause des
        // Weapon-Manager-Hooks kam zu spaet: Beim ersten Rendern des neuen
        // Levels konnte D3DX noch einen Node-Controller des alten PlayerBody
        // auswerten. Das endete reproduzierbar in D3DX9_27+0xFCCDF.
        //
        // Nur diese beiden Weltwechsel-Zustaende invalidieren die Handles.
        // GS_MENU/GS_PAUSED/GS_SCREEN duerfen die Hand-Bindungen behalten,
        // damit ESC- und VR-Menue nicht unnoetig in die Welt eingreifen.
        if (state == kRetailGameStateExitingLevel ||
            state == kRetailGameStateLoadingLevel) {
            ForgetWorldObjectsAfterLevelChange();
            // Der naechste echte Waffenframe muss alle Weltbindungen neu
            // aufbauen, auch wenn der Ladebildschirm kuerzer als die bisherige
            // Ein-Sekunden-Heuristik war.
            g_lastWeaponManagerUpdateTick = 0;
        }
        g_lastReportedRetailGameState = state;
        char message[128];
        _snprintf_s(
            message, sizeof(message), _TRUNCATE,
            "state=%d playing=%d", state,
            state == kRetailGameStatePlaying ? 1 : 0);
        Report("INFO", "retail_game_state", message);
    }
    return state;
}

void UpdateRetailHudOverride() noexcept {
    if (g_client == nullptr) {
        return;
    }

    bool stereoEnabled = false;
    __try {
        stereoEnabled =
            g_isStereoEnabled != nullptr && g_isStereoEnabled();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        stereoEnabled = false;
    }
    const bool hideRetailStatus =
        stereoEnabled &&
        ReadRetailGameState() == kRetailGameStatePlaying;
    if (hideRetailStatus == g_retailHudOverrideApplied) {
        return;
    }

    bool configured = true;
    __try {
        for (RetailHudVariableOverride& variable :
             g_retailHudVariables) {
            if (!variable.originalKnown) {
                const HCONSOLEVAR handle =
                    g_client->GetConsoleVariable(variable.name);
                if (handle == nullptr) {
                    configured = false;
                    continue;
                }
                variable.originalValue =
                    g_client->GetConsoleVariableFloat(handle);
                variable.originalKnown = true;
            }
            if (g_client->SetConsoleVariableFloat(
                    variable.name,
                    hideRetailStatus
                        ? 0.0F
                        : variable.originalValue) != LT_OK) {
                configured = false;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        configured = false;
    }
    if (!configured) {
        return;
    }

    g_retailHudOverrideApplied = hideRetailStatus;
    Report(
        "INFO",
        hideRetailStatus
            ? "retail_status_hud_hidden"
            : "retail_status_hud_restored",
        hideRetailStatus
            ? "Retail health, armor, ammo, grenade and gear panels are "
              "hidden during VR gameplay in favor of the wrist display."
            : "Retail status HUD settings were restored for menu and "
              "flat-panel rendering.");
}

DevMenuVector3 ToDevMenuVector(const LTVector& value) noexcept {
    return {value.x, value.y, value.z};
}

void ResetDevMenuGameplayInput() noexcept {
    g_weaponSwitchPulseUntil = 0;
    g_weaponSwitchTriggered = false;
    g_secondaryHoldStartTick = 0;
    g_reloadPulseUntil = 0;
    g_grenadePulseUntil = 0;
    g_secondaryWasDown = false;
    g_grenadeConsumed = false;
    g_meleePulseUntil = 0;
    g_slideDuckPulseUntil = 0;
    g_slideForwardPulseUntil = 0;
    ResetMeleeActions(g_meleeActions);
    g_climbAxis = 0.0F;
    g_climbActive = false;
    ReleaseTwoHandedGrip();
    for (bool& active : g_injectedCommandActive) {
        active = false;
    }
    for (bool& active : g_controllerCommandActive) {
        active = false;
    }
}

void SelectFloatingDevMenuTab(std::size_t tabIndex) noexcept;

void CloseFloatingDevMenu() noexcept {
    if (!g_devMenu.open) {
        return;
    }
    g_devMenu.open = false;
    g_devMenu.anchorValid = false;
    g_devMenu.pointerValid = false;
    g_devMenu.suppressUntilRelease = true;
    SaveVrSettings();
    Report(
        "INFO", "floating_dev_menu_closed",
        "The live-tuning panel closed and controller gameplay input will "
        "resume after its buttons are released.");
}

void OpenFloatingDevMenu() noexcept {
    g_devMenu.open = true;
    g_devMenu.anchorValid = false;
    g_devMenu.pointerValid = false;
    g_devMenu.suppressUntilRelease = true;
    SelectFloatingDevMenuTab(
        static_cast<std::size_t>(g_devMenu.selectedTab));
    ResetDevMenuGameplayInput();
    Report(
        "INFO", "floating_dev_menu_opened",
        "Both grips plus B opened the world-space live-tuning panel; "
        "point with the right controller and press trigger or A.");
}

void SelectFloatingDevMenuTab(std::size_t tabIndex) noexcept {
    if (tabIndex >= kDevMenuTabCount) {
        return;
    }
    g_devMenu.selectedTab = static_cast<DevMenuTab>(tabIndex);
    const std::size_t rowCount = DevMenuRowCount(g_devMenu.selectedTab);
    g_devMenu.selectedRow = rowCount == 0
        ? 0U : (std::min)(g_devMenu.selectedRow, rowCount - 1U);
}

void ActivateFloatingDevMenuRow(
    DevMenuTab tab, std::size_t row) noexcept {
    if (row >= DevMenuRowCount(tab)) {
        return;
    }
    switch (tab) {
    case DevMenuTab::recoil:
        switch (row) {
        case 0:
            g_weaponRecoilEnabled = !g_weaponRecoilEnabled;
            ResetWeaponRecoil(g_weightedWeaponInput.recoil);
            InterlockedExchange(&g_pendingWeaponRecoilShots, 0);
            g_lastWeaponRecoilTick = 0;
            break;
        case 1:
            g_vrRecoilProfileEditsCurrent =
                HasCurrentWeaponWeightProfile()
                    ? !g_vrRecoilProfileEditsCurrent : false;
            if (g_vrRecoilProfileEditsCurrent) {
                (void)EditableWeaponRecoilProfile();
            }
            break;
        case 2: {
            WeaponRecoilProfile& profile = EditableWeaponRecoilProfile();
            profile.strength =
                kWeaponRecoilStrengthPresets[NextVrPresetIndex(
                    profile.strength,
                    kWeaponRecoilStrengthPresets)];
            break;
        }
        case 3: {
            WeaponRecoilProfile& profile = EditableWeaponRecoilProfile();
            profile.muzzleRise =
                kWeaponRecoilRisePresets[NextVrPresetIndex(
                    profile.muzzleRise,
                    kWeaponRecoilRisePresets)];
            break;
        }
        case 4: {
            WeaponRecoilProfile& profile = EditableWeaponRecoilProfile();
            profile.recovery =
                kWeaponRecoilRecoveryPresets[NextVrPresetIndex(
                    profile.recovery,
                    kWeaponRecoilRecoveryPresets)];
            break;
        }
        }
        break;
    case DevMenuTab::weight: {
        WeaponWeightProfile& profile = EditableWeaponWeightProfile();
        switch (row) {
        case 0: {
            const int next =
                (static_cast<int>(g_weaponWeightPreset) + 1) % 4;
            g_weaponWeightPreset = SanitizeWeaponWeightPreset(next);
            g_weaponWeightEnabled =
                g_weaponWeightPreset != WeaponWeightPreset::none;
            ResetWeaponWeightPair(
                g_weightedWeaponInput.filters,
                WeaponWeightResetReason::enabledChanged);
            break;
        }
        case 1:
            g_vrWeaponProfileEditsCurrent =
                HasCurrentWeaponWeightProfile()
                    ? !g_vrWeaponProfileEditsCurrent : false;
            break;
        case 2:
            profile.weight = kWeaponWeightPresets[NextVrPresetIndex(
                profile.weight, kWeaponWeightPresets)];
            break;
        case 3:
            profile.positionalFollow =
                kWeaponPositionFollowPresets[NextVrPresetIndex(
                    profile.positionalFollow,
                    kWeaponPositionFollowPresets)];
            break;
        case 4:
            profile.rotationalFollow =
                kWeaponRotationFollowPresets[NextVrPresetIndex(
                    profile.rotationalFollow,
                    kWeaponRotationFollowPresets)];
            break;
        case 5:
            profile.catchUpStrength =
                kWeaponCatchUpPresets[NextVrPresetIndex(
                    profile.catchUpStrength, kWeaponCatchUpPresets)];
            break;
        }
        break;
    }
    case DevMenuTab::collision: {
        switch (row) {
        case 0:
            g_weaponCollisionEnabled = !g_weaponCollisionEnabled;
            ResetWeaponCollisionRuntime(g_weightedWeaponInput.collision);
            g_weightedWeaponInput.collisionObstructed = false;
            break;
        case 1:
            g_weaponCollisionDebugEnabled =
                !g_weaponCollisionDebugEnabled;
            break;
        case 2:
            g_vrCollisionProfileEditsCurrent =
                HasCurrentWeaponWeightProfile()
                    ? !g_vrCollisionProfileEditsCurrent : false;
            if (g_vrCollisionProfileEditsCurrent) {
                (void)EditableWeaponCollisionProfile();
            }
            break;
        case 3: {
            WeaponCollisionProfile& profile =
                EditableWeaponCollisionProfile();
            profile.lengthScale =
                kWeaponCollisionLengthPresets[NextVrPresetIndex(
                    profile.lengthScale,
                    kWeaponCollisionLengthPresets)];
            break;
        }
        case 4: {
            WeaponCollisionProfile& profile =
                EditableWeaponCollisionProfile();
            profile.widthMeters =
                kWeaponCollisionWidthPresets[NextVrPresetIndex(
                    profile.widthMeters,
                    kWeaponCollisionWidthPresets)];
            break;
        }
        case 5: {
            WeaponCollisionProfile& profile =
                EditableWeaponCollisionProfile();
            profile.heightMeters =
                kWeaponCollisionHeightPresets[NextVrPresetIndex(
                    profile.heightMeters,
                    kWeaponCollisionHeightPresets)];
            break;
        }
        case 6: {
            WeaponCollisionProfile& profile =
                EditableWeaponCollisionProfile();
            profile.forwardOffsetMeters =
                kWeaponCollisionOffsetPresets[NextVrPresetIndex(
                    profile.forwardOffsetMeters,
                    kWeaponCollisionOffsetPresets)];
            break;
        }
        }
        if (row >= 2) {
            ResetWeaponCollisionRuntime(g_weightedWeaponInput.collision);
            g_weightedWeaponInput.collisionObstructed = false;
        }
        break;
    }
    case DevMenuTab::weapon:
        switch (row) {
        case 0:
            g_weaponAimGuideEnabled = !g_weaponAimGuideEnabled;
            break;
        case 1:
            g_showPlayerArms = !g_showPlayerArms;
            break;
        case 2:
            g_twoHandedGripEnabled = !g_twoHandedGripEnabled;
            if (!g_twoHandedGripEnabled) {
                g_twoHandedGrip = TwoHandedGripState{};
            }
            break;
        }
        break;
    case DevMenuTab::ik:
        if (row == 0) {
            g_devMenuIkLeftHandPage = !g_devMenuIkLeftHandPage;
            g_devMenu.selectedRow = 0;
            break;
        }
        if (!g_devMenuIkLeftHandPage) {
            switch (row) {
            case 1:
                g_showPlayerArms = !g_showPlayerArms;
                break;
            case 2:
                g_armIkTuning.elbowOutward =
                    NextVrSteppedValue(
                        g_armIkTuning.elbowOutward,
                        0.20F, 2.00F, kIkElbowStep);
                ResetArmIkBendMemory();
                break;
            case 3:
                g_armIkTuning.elbowDown =
                    NextVrSteppedValue(
                        g_armIkTuning.elbowDown,
                        0.00F, 1.50F, kIkElbowStep);
                ResetArmIkBendMemory();
                break;
            case 4:
                g_armIkTuning.elbowBack =
                    NextVrSteppedValue(
                        g_armIkTuning.elbowBack,
                        -1.00F, 1.00F, kIkElbowStep);
                ResetArmIkBendMemory();
                break;
            case 5:
                g_armIkTuning.preserveElbowContinuity =
                    !g_armIkTuning.preserveElbowContinuity;
                ResetArmIkBendMemory();
                break;
            case 6:
                ResetArmIkTuning();
                break;
            }
        } else {
            switch (row) {
            case 1:
                g_armIkTuning.leftHandRightMeters =
                    NextVrSteppedValue(
                        g_armIkTuning.leftHandRightMeters,
                        -0.20F, 0.20F,
                        kIkHandPositionStepMeters);
                break;
            case 2:
                g_armIkTuning.leftHandUpMeters =
                    NextVrSteppedValue(
                        g_armIkTuning.leftHandUpMeters,
                        -0.20F, 0.20F,
                        kIkHandPositionStepMeters);
                break;
            case 3:
                g_armIkTuning.leftHandForwardMeters =
                    NextVrSteppedValue(
                        g_armIkTuning.leftHandForwardMeters,
                        -0.20F, 0.20F,
                        kIkHandPositionStepMeters);
                break;
            case 4:
                g_armIkTuning.leftHandPitchDegrees =
                    NextVrSteppedValue(
                        g_armIkTuning.leftHandPitchDegrees,
                        -180.0F, 180.0F,
                        kIkHandRotationStepDegrees);
                break;
            case 5:
                g_armIkTuning.leftHandYawDegrees =
                    NextVrSteppedValue(
                        g_armIkTuning.leftHandYawDegrees,
                        -180.0F, 180.0F,
                        kIkHandRotationStepDegrees);
                break;
            case 6:
                g_armIkTuning.leftHandRollDegrees =
                    NextVrSteppedValue(
                        g_armIkTuning.leftHandRollDegrees,
                        -180.0F, 180.0F,
                        kIkHandRotationStepDegrees);
                break;
            }
        }
        g_armIkTuning = SanitizeArmIkTuning(g_armIkTuning);
        break;
    case DevMenuTab::movement:
        switch (row) {
        case 0:
            SetBooleanOption(
                g_setTranslationEnabled,
                !QueryBooleanOption(g_isTranslationEnabled, false));
            break;
        case 1:
            g_headRelativeMovement = !g_headRelativeMovement;
            Report(
                "INFO", "vr_movement_direction_changed",
                g_headRelativeMovement
                    ? "Forward movement follows horizontal HMD direction."
                    : "Forward movement follows the Retail player body.");
            break;
        case 2:
            if (g_headBobEnabled || !g_forceHeadBobDisabled) {
                ApplyHeadBobEnabled(!g_headBobEnabled);
            }
            break;
        case 3:
            g_physicalLeanEnabled = !g_physicalLeanEnabled;
            ResetLeanCollision(g_leanCollision);
            g_leanTranslationScale = 1.0F;
            break;
        case 4:
            g_leanScalePercent = kLeanScalePresets[NextVrPresetIndex(
                g_leanScalePercent, kLeanScalePresets)];
            break;
        case 5:
            g_turnSpeedPreset = (g_turnSpeedPreset + 1) % 3;
            break;
        case 6:
            g_climbingEnabled = !g_climbingEnabled;
            ResetClimbGrip(g_climbGrip);
            g_climbActive = false;
            g_climbOnLadder = false;
            g_climbAxis = 0.0F;
            g_climbWasGripping = false;
            break;
        case 7:
            g_configuredEyeHeightMeters = 0.0F;
            if (g_headTracking.floorSpace &&
                IsPlausibleEyeHeightMeters(
                    g_headTracking.currentCenter.py)) {
                g_headTracking.sessionEyeHeightMeters =
                    g_headTracking.currentCenter.py;
            }
            Report(
                "INFO", "vr_height_auto",
                "Eye height returned to automatic session calibration.");
            break;
        case 8:
            if (g_headTracking.floorSpace &&
                IsPlausibleEyeHeightMeters(
                    g_headTracking.currentCenter.py)) {
                g_configuredEyeHeightMeters =
                    SanitizeEyeHeightMeters(
                        g_headTracking.currentCenter.py);
                g_headTracking.sessionEyeHeightMeters =
                    g_configuredEyeHeightMeters;
                char message[128]{};
                std::snprintf(
                    message, sizeof(message),
                    "Standing eye height set from HMD: %.0f cm.",
                    static_cast<double>(
                        g_configuredEyeHeightMeters * 100.0F));
                Report("INFO", "vr_height_set", message);
            } else {
                ResetVrTrackingBasis();
                Report(
                    "WARN", "vr_height_floor_unavailable",
                    "The runtime has no floor-relative reference space; "
                    "the current local origin was recentered instead.");
            }
            break;
        }
        break;
    case DevMenuTab::melee:
        switch (row) {
        case 0:
            g_meleeThrustEnabled = !g_meleeThrustEnabled;
            g_meleePulseUntil = 0;
            g_slideDuckPulseUntil = 0;
            g_slideForwardPulseUntil = 0;
            break;
        case 1:
            g_meleeWeaponStrikeEnabled = !g_meleeWeaponStrikeEnabled;
            break;
        case 2:
            g_meleeOffHandStrikeEnabled = !g_meleeOffHandStrikeEnabled;
            break;
        case 3:
            g_meleeJumpKickEnabled = !g_meleeJumpKickEnabled;
            break;
        case 4:
            g_meleeSlideKickEnabled = !g_meleeSlideKickEnabled;
            break;
        }
        ResetMeleeActions(g_meleeActions);
        break;
    case DevMenuTab::vr:
        switch (row) {
        case 0:
            SetBooleanOption(
                g_setStereoHudEnabled,
                !QueryBooleanOption(g_isStereoHudEnabled, true));
            break;
        case 1:
            g_fovScalePreset = (g_fovScalePreset + 1) % 4;
            ApplyFovScalePreset();
            break;
        case 2:
            g_leftHandedBindings = !g_leftHandedBindings;
            ResetVrTrackingBasis();
            break;
        case 3:
            g_controllerHapticsEnabled = !g_controllerHapticsEnabled;
            break;
        case 4:
            g_weaponWeightDiagnosticsEnabled =
                !g_weaponWeightDiagnosticsEnabled;
            break;
        case 5:
            g_renderScalePercent =
                kRenderScalePercents[NextVrPresetIndex(
                    g_renderScalePercent, kRenderScalePercents)];
            ApplyRenderScalePercent();
            break;
        }
        break;
    }
    SaveVrSettings();
}

bool UpdateDevMenuSelectionAxis(
    bool down, bool& wasDown, ULONGLONG& repeatTick,
    ULONGLONG now) noexcept {
    constexpr ULONGLONG kInitialRepeatDelayMs = 350;
    constexpr ULONGLONG kRepeatDelayMs = 120;
    if (!down) {
        wasDown = false;
        repeatTick = 0;
        return false;
    }
    if (!wasDown) {
        wasDown = true;
        repeatTick = now + kInitialRepeatDelayMs;
        return true;
    }
    if (now >= repeatTick) {
        repeatTick = now + kRepeatDelayMs;
        return true;
    }
    return false;
}

void UpdateFloatingDevMenuRaySelection() noexcept {
    g_devMenu.pointerValid = false;
    g_devMenu.pointerRegion = DevMenuHitRegion::none;
    if (!g_devMenu.open || !g_devMenu.anchorValid ||
        !g_weaponAim.valid) {
        return;
    }
    LTVector direction = g_weaponAim.fireTransform.m_rRot.Forward();
    if (direction.MagSqr() < 1.0e-6F) {
        return;
    }
    direction.Normalize();
    DevMenuPanelGeometry panel{};
    panel.center = ToDevMenuVector(g_devMenu.center);
    panel.right = ToDevMenuVector(g_devMenu.right);
    panel.up = ToDevMenuVector(g_devMenu.up);
    panel.normal = ToDevMenuVector(g_devMenu.normal);
    panel.width = kDevMenuWidthMeters * kGameUnitsPerMeter;
    panel.height = kDevMenuHeightMeters * kGameUnitsPerMeter;
    panel.headerHeight = kDevMenuHeaderMeters * kGameUnitsPerMeter;
    panel.titleHeight = kDevMenuTitleMeters * kGameUnitsPerMeter;
    panel.tabHeight = kDevMenuTabMeters * kGameUnitsPerMeter;
    panel.rowHeight = kDevMenuRowMeters * kGameUnitsPerMeter;
    panel.tabCount = kDevMenuTabCount;
    panel.rowCount = DevMenuRowCount(g_devMenu.selectedTab);
    DevMenuRayHit hit;
    if (!HitTestDevMenuPanel(
            panel,
            ToDevMenuVector(g_weaponAim.fireTransform.m_vPos),
            ToDevMenuVector(direction), hit)) {
        return;
    }
    g_devMenu.pointerRegion = hit.region;
    if (hit.region == DevMenuHitRegion::tab) {
        g_devMenu.pointerIndex = hit.tab;
    } else if (hit.region == DevMenuHitRegion::row) {
        g_devMenu.pointerIndex = hit.row;
        g_devMenu.selectedRow = hit.row;
    }
    g_devMenu.pointerWorld =
        g_weaponAim.fireTransform.m_vPos + direction * hit.distance;
    g_devMenu.pointerValid = true;
}

void PollFloatingDevMenuInput() noexcept {
    const std::uint32_t buttons = g_currentInput.buttons;
    const std::uint32_t pressed =
        buttons & ~g_devMenu.lastButtons;
    const bool triggerDown =
        (g_currentInput.activeHands & FEARVR_HAND_MASK_RIGHT) != 0 &&
        g_currentInput.trigger[FEARVR_HAND_RIGHT] >= 0.55F;
    const bool triggerPressed =
        triggerDown && !g_devMenu.lastTriggerDown;
    const bool bothGrips =
        g_currentInput.squeeze[FEARVR_HAND_LEFT] >= 0.75F &&
        g_currentInput.squeeze[FEARVR_HAND_RIGHT] >= 0.75F;
    const bool toggleRequested =
        bothGrips &&
        (pressed & FEARVR_IB_RIGHT_SECONDARY) != 0;
    const ULONGLONG now = GetTickCount64();

    const int gameState = ReadRetailGameState();
    const bool playing =
        gameState >= 0
            ? gameState == kRetailGameStatePlaying
            : (g_lastWeaponManagerUpdateTick != 0 &&
               now - g_lastWeaponManagerUpdateTick <=
                   kPlayingFrameFreshMilliseconds);

    if (g_devMenu.open && !playing) {
        CloseFloatingDevMenu();
    } else if (!g_devMenu.open && toggleRequested && playing) {
        OpenFloatingDevMenu();
        g_devMenu.lastButtons = buttons;
        g_devMenu.lastTriggerDown = triggerDown;
        return;
    }

    if (g_devMenu.open) {
        ResetDevMenuGameplayInput();
        UpdateFloatingDevMenuRaySelection();
        if ((pressed & FEARVR_IB_RIGHT_SECONDARY) != 0) {
            CloseFloatingDevMenu();
        } else {
            const std::size_t rowCount =
                DevMenuRowCount(g_devMenu.selectedTab);
            if (UpdateDevMenuSelectionAxis(
                    g_currentInput.turnY >= 0.55F,
                    g_devMenu.axisUpDown,
                g_devMenu.axisUpRepeatTick, now)) {
                g_devMenu.selectedRow =
                    (g_devMenu.selectedRow + rowCount - 1U) % rowCount;
            }
            if (UpdateDevMenuSelectionAxis(
                    g_currentInput.turnY <= -0.55F,
                    g_devMenu.axisDownDown,
                g_devMenu.axisDownRepeatTick, now)) {
                g_devMenu.selectedRow =
                    (g_devMenu.selectedRow + 1U) % rowCount;
            }
            if (UpdateDevMenuSelectionAxis(
                    g_currentInput.turnX <= -0.55F,
                    g_devMenu.axisLeftDown,
                    g_devMenu.axisLeftRepeatTick, now)) {
                SelectFloatingDevMenuTab(
                    (static_cast<std::size_t>(g_devMenu.selectedTab) +
                     kDevMenuTabCount - 1U) % kDevMenuTabCount);
            }
            if (UpdateDevMenuSelectionAxis(
                    g_currentInput.turnX >= 0.55F,
                    g_devMenu.axisRightDown,
                    g_devMenu.axisRightRepeatTick, now)) {
                SelectFloatingDevMenuTab(
                    (static_cast<std::size_t>(g_devMenu.selectedTab) + 1U) %
                    kDevMenuTabCount);
            }
            if (triggerPressed ||
                (pressed & FEARVR_IB_RIGHT_PRIMARY) != 0) {
                if (g_devMenu.pointerValid &&
                    g_devMenu.pointerRegion == DevMenuHitRegion::tab) {
                    SelectFloatingDevMenuTab(g_devMenu.pointerIndex);
                } else {
                    ActivateFloatingDevMenuRow(
                        g_devMenu.selectedTab, g_devMenu.selectedRow);
                }
            }
        }
    }

    if (g_devMenu.suppressUntilRelease && !g_devMenu.open &&
        g_currentInput.squeeze[FEARVR_HAND_LEFT] < 0.25F &&
        g_currentInput.squeeze[FEARVR_HAND_RIGHT] < 0.25F &&
        (buttons & (FEARVR_IB_RIGHT_PRIMARY |
                    FEARVR_IB_RIGHT_SECONDARY)) == 0 &&
        !triggerDown) {
        g_devMenu.suppressUntilRelease = false;
    }
    g_devMenu.lastButtons = buttons;
    g_devMenu.lastTriggerDown = triggerDown;
}

void PollControllerMenuInput() noexcept {
    PollFlashlightToggle();
    const std::uint32_t buttons = g_currentInput.buttons;
    const std::uint32_t pressed =
        buttons & ~g_lastMenuButtons;
    const bool triggerDown =
        (g_currentInput.activeHands & FEARVR_HAND_MASK_RIGHT) != 0 &&
        g_currentInput.trigger[FEARVR_HAND_RIGHT] >= 0.55F;
    const bool triggerPressed =
        triggerDown && !g_lastMenuTriggerDown;
    const bool menuRequested =
        (pressed & kLeftPauseButtonMask) != 0;
    const bool escapeDown =
        (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    const bool escapePressed = escapeDown && !g_escapeWasDown;
    g_escapeWasDown = escapeDown;

    const ULONGLONG now = GetTickCount64();
    if (menuRequested || escapePressed) {
        // Escape kommt synchron an, das Retail-Menü öffnet erst im nächsten
        // Update. Ein blindes Umschalten wäre falsch: Escape öffnet das
        // Pausenmenü nur aus dem Spiel heraus, innerhalb der Menüs geht es
        // eine Ebene zurück und schließt sie erst auf der obersten.
        if (!g_menuFocusKnown || !g_menuFocusActive) {
            g_menuActivationHoldUntil = now + 1000;
            g_menuFocusKnown = true;
            g_menuFocusActive = true;
        } else {
            // Ob das Menü damit geschlossen wurde oder nur eine Ebene
            // zurückging, ist hier nicht erkennbar. Die Heuristik entscheidet.
            g_menuFocusKnown = false;
        }
        if (menuRequested) {
            TapMenuKey(VK_ESCAPE);
            Report(
                "INFO", "controller_pause_requested",
                "Left secondary/menu button sent one Escape key edge.");
        }
    }
    // Main screens are intentionally mono until F8, so render coverage alone
    // cannot identify them. The verified weapon-manager hook is called every
    // playing frame and stops for screens, pause menus and message boxes.
    // A single delayed weapon-manager callback must not switch the headset to
    // the mono/video path for one frame. Retail pause/menu focus is handled
    // immediately above; this is only the fallback for screens and message
    // boxes without that focus edge.
    constexpr ULONGLONG kPlayingFrameFreshMs = 500;
    const bool playingFrameFresh =
        g_lastWeaponManagerUpdateTick != 0 &&
        now - g_lastWeaponManagerUpdateTick <= kPlayingFrameFreshMs;
    const bool menuActivationHeld =
        now < g_menuActivationHoldUntil;
    // Der Retail-Spielzustand entscheidet, sobald er lesbar ist.
    //
    // Die frueheren Ersatzsignale reichten fuer Vollbildschirme wie das
    // Missionsbriefing nicht: Der Menuefokus kennt nur Pausenmenues, und der
    // Weapon-Manager laeuft waehrend eines Briefings weiter. Das Briefing galt
    // damit als Spielframe, fiel im Compositor zwischen HUD-Overlay und
    // Flachbild und wurde verworfen — im Headset blitzte es nur waehrend der
    // Umschaltframes auf und verschwand wieder.
    //
    // `CInterfaceMgr::m_eGameState` ist dagegen exakt: Nur GS_PLAYING rendert
    // die Welt, jeder andere Zustand ist ein Vollbild-UI.
    const int retailGameState = ReadRetailGameState();
    const bool menuActive =
        retailGameState >= 0
            ? retailGameState != kRetailGameStatePlaying
            : (menuActivationHeld || !playingFrameFresh ||
               (g_menuFocusKnown && g_menuFocusActive));
    if (g_setMenuActive != nullptr) {
        __try {
            g_setMenuActive(menuActive ? TRUE : FALSE);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Report(
                "WARN", "menu_render_mode_failed",
                "The bridge rejected the pause/menu render-mode update.");
        }
    }
    if (!menuActive) {
        if ((pressed & FEARVR_IB_RIGHT_STICK) != 0) {
            constexpr ULONGLONG kManualMeleePulseMs = 200;
            // Retail exposes the slide-kick posture only after this input
            // frame. Capture the camera before every manual melee pulse so a
            // same-frame slide can never evade stabilization.
            BeginSlideKickViewStabilization();
            g_meleePulseUntil = std::max(
                g_meleePulseUntil, now + kManualMeleePulseMs);
            Report(
                "INFO", "controller_melee_requested",
                "Right stick click captured the camera basis and pulsed "
                "Retail melee; Retail selects the strike or kick variant.");
        }
        for (std::size_t index = 0; index < 4; ++index) {
            g_menuAxisDown[index] = false;
            g_menuAxisRepeatTick[index] = 0;
        }
        g_menuControllerActive = false;
        g_lastMenuButtons = buttons;
        g_lastMenuTriggerDown = triggerDown;
        return;
    }
    if (!g_menuControllerActive) {
        Report(
            "INFO", "controller_menu_active",
            "Flat VR menu accepts sticks, A/trigger and B/menu.");
        g_menuControllerActive = true;
    }

    if ((pressed & FEARVR_IB_RIGHT_PRIMARY) != 0 ||
        triggerPressed) {
        TapMenuKey(VK_RETURN);
    }
    if ((pressed & kRightSecondaryButtonMask) != 0) {
        if (g_vrSettingsPageActive) {
            NavigateBackRetailVrSettingsPage();
        } else {
            // B geht in Untermenüs eine Ebene zurück und schließt das Menü
            // nur auf der obersten Ebene. Deshalb hier kein "Menü zu"
            // behaupten, sondern der Heuristik überlassen.
            g_menuFocusKnown = false;
            g_menuActivationHoldUntil = 0;
            TapMenuKey(VK_ESCAPE);
        }
    }

    const float horizontal =
        std::fabs(g_currentInput.moveX) >=
                std::fabs(g_currentInput.turnX)
            ? g_currentInput.moveX
            : g_currentInput.turnX;
    const float vertical =
        std::fabs(g_currentInput.moveY) >=
                std::fabs(g_currentInput.turnY)
            ? g_currentInput.moveY
            : g_currentInput.turnY;
    UpdateMenuAxis(0, horizontal <= -0.55F, VK_LEFT, now);
    UpdateMenuAxis(1, horizontal >= 0.55F, VK_RIGHT, now);
    UpdateMenuAxis(2, vertical >= 0.55F, VK_UP, now);
    UpdateMenuAxis(3, vertical <= -0.55F, VK_DOWN, now);

    g_lastMenuButtons = buttons;
    g_lastMenuTriggerDown = triggerDown;
}

void __fastcall HookClientShellUpdate(
    IClientShell* clientShell, void* ignoredEdx) {
    (void)ignoredEdx;
    // Safety net for a frame that staged the center-eye camera for planar
    // render targets but did not reach RenderCamera during a state change.
    RestorePreRenderCameraOverride();
    if (InterlockedCompareExchange(
            &g_clientInputHookCallLogged, 1, 0) == 0) {
        Report(
            "INFO", "client_input_hook_called",
            "IClientShell version-5 Update slot 20 is active.");
    }
    // Some Retail console variables are registered only after the initial
    // interface hookup. Retry on the first client update instead of silently
    // leaving database-driven Walk/Run bob active for the whole session.
    if (!g_stableWeaponMotionConfigured) {
        ApplyHeadBobEnabled(false);
    }
    PollControllerInput();
    PollFloatingDevMenuInput();
    if (!g_disableClientUpdateWork) {
        if (!DevMenuCapturesControllerInput()) {
            PrepareWeaponSwitchPulse();
            PrepareGrenadeAndReloadPulse();
            UpdateMeleeActions();
            UpdateClimbMotion();
            PollControllerMenuInput();
        } else {
            ResetDevMenuGameplayInput();
        }
    }
    if (!g_disableClientUpdateWork) {
        UpdateCrosshairOverride();
        UpdateVrCameraCollisionPath();
    }
    g_semanticBitsInjected = false;
    g_clientShellUpdate(clientShell);
    if (g_vrSettingsPageActive) {
        // Tastatur, Maus und Controller können die Auswahl vor oder in
        // Retails Update ändern. Ein einziger Abgleich danach verhindert,
        // dass zwei Korrekturen vor CalculatePositions den Listenanfang mit
        // veralteten m_nLastShown-Daten hin und her setzen.
        MaintainRetailVrMenuScroll();
    }
    if (!g_disableClientUpdateWork) {
        // Erst nach Retails Update: Eine gerade geoeffnete ESC-Seite hat dann
        // bereits GS_MENU/GS_PAUSED gesetzt und bekommt im selben Renderframe
        // ihre unveraenderten HUD-/UI-Variablen zurueck.
        UpdateRetailHudOverride();
        MaintainSlideKickViewBase();
        UpdateRetailMeleeDiagnostics();
        StagePreRenderCameraOverride();
    }
}

bool InstallClientInputHook(void* masterDatabase) noexcept {
    if (!CommandLineContains(L"-fearvr-input")) {
        return true;
    }
    if (g_getInputState == nullptr ||
        g_submitHapticRequest == nullptr ||
        g_isFlatPanelActive == nullptr) {
        Report(
            "ERROR", "input_bridge_exports_missing",
            "The staged bridge lacks the M5 input exports.");
        return false;
    }
    g_clientShell = static_cast<IClientShell*>(
        FindCurrentInterface(
            masterDatabase, "IClientShell.Default", 5));
    if (g_clientShell == nullptr) {
        Report(
            "ERROR", "client_shell_interface_missing",
            "IClientShell.Default version 5 was not found.");
        return false;
    }
    void** const vtable =
        *reinterpret_cast<void***>(g_clientShell);
    void** const slot =
        vtable == nullptr
            ? nullptr
            : &vtable[kClientShellUpdateSlot];
    void* const target = slot == nullptr ? nullptr : *slot;
    if (!IsExecutableAddress(target)) {
        Report(
            "ERROR", "client_input_hook_layout_mismatch",
            "IClientShell Update slot is not executable.");
        return false;
    }
    g_clientShellUpdate =
        reinterpret_cast<ClientShellUpdateFunction>(target);
    if (!InstallSemanticInputHook(target)) {
        g_clientShellUpdate = nullptr;
        return false;
    }
    if (!ExchangeVtableSlot(
            slot, target,
            reinterpret_cast<void*>(&HookClientShellUpdate))) {
        RemoveSemanticInputHook();
        g_clientShellUpdate = nullptr;
        return false;
    }
    if (!InstallVrCameraCollisionHook()) {
        Report(
            "WARN", "vr_camera_collision_filter_unavailable",
            "Native VR keeps Retail's unfiltered raycast fallback; "
            "other controls and rendering remain active.");
    }
    if (!InstallWeaponAimHooks()) {
        Report(
            "WARN", "weapon_aim_unavailable",
            "Controller gameplay input remains active, but the "
            "verified Retail weapon hooks could not be installed.");
    }
    if (!InstallInteractionHooks()) {
        Report(
            "WARN", "interaction_unavailable",
            "Activation and pickup fall back to the Retail view direction; "
            "everything else stays active.");
    }
    Report(
        "INFO", "client_input_hook_installed",
        "M5 polls OpenXR input from IClientShell::Update and merges "
        "semantic controller commands with existing game input.");
    return true;
}

bool TryInstallRendererHook() noexcept {
    AcquireSRWLockExclusive(&g_hookLock);
    if (g_hookInstalled) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return true;
    }
    if (g_client == nullptr || g_renderer == nullptr) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }

    void** const vtable = *reinterpret_cast<void***>(g_renderer);
    void** const playerSlot =
        vtable == nullptr
            ? nullptr
            : &vtable[kRenderPlayerCameraSlot];
    void* const playerTarget =
        playerSlot == nullptr ? nullptr : *playerSlot;
    void* const overrideTarget =
        vtable == nullptr ? nullptr
                          : vtable[kRenderCameraWithOverrideSlot];
    const bool playerMatches =
        MatchesRetailPlayerCameraForwarder(playerTarget);
    const bool overrideExecutable =
        IsCallableMainImageAddress(overrideTarget);
    if (!playerMatches || !overrideExecutable) {
        ReportRendererHookLayout(
            playerTarget, overrideTarget,
            playerMatches, overrideExecutable);
        Report(
            "ERROR", "stereo_hook_layout_mismatch",
            "The retail RenderCamera 17-to-19 forwarding layout "
            "does not match; the VTable remains untouched.");
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }

    g_renderPlayerCamera =
        reinterpret_cast<RenderPlayerCameraFunction>(playerTarget);
    g_renderCameraWithOverride =
        reinterpret_cast<RenderCameraWithOverrideFunction>(
            overrideTarget);
    if (!ExchangeVtableSlot(
            playerSlot, playerTarget,
            reinterpret_cast<void*>(
                &HookRenderPlayerCamera))) {
        g_renderPlayerCamera = nullptr;
        g_renderCameraWithOverride = nullptr;
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }
    g_hookInstalled = true;
    g_renderCameraSlot = playerSlot;
    InterlockedIncrement(&g_trackingResetGeneration);
    Report(
        "INFO", "stereo_hook_installed",
        "ILTClient 105 and ILTRenderer 0 connected; "
        "retail player-camera slot 17 is hooked and renders through "
        "the exact slot 19 overload once per eye.");
    ReleaseSRWLockExclusive(&g_hookLock);
    return true;
}

bool TryRemoveRendererHook() noexcept {
    RestorePreRenderCameraOverride();
    RestoreRetailCameraCollisionPath();
    AcquireSRWLockExclusive(&g_hookLock);
    if (!g_hookInstalled ||
        g_renderCameraSlot == nullptr ||
        g_renderPlayerCamera == nullptr ||
        g_renderCameraWithOverride == nullptr) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return true;
    }

    const bool slotRestored = ExchangeVtableSlot(
        g_renderCameraSlot,
        reinterpret_cast<void*>(
            &HookRenderPlayerCamera),
        reinterpret_cast<void*>(
            g_renderPlayerCamera));
    g_hookInstalled = !slotRestored;
    if (g_hookInstalled) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }
    Report(
        "INFO", "stereo_hook_removed",
        "Original player-camera VTable slot 17 restored.");
    ReleaseSRWLockExclusive(&g_hookLock);
    return true;
}

void __cdecl SetStereoHookEnabled(BOOL enabled) {
    if (enabled) {
        TryInstallRendererHook();
    } else {
        TryRemoveRendererHook();
    }
    UpdateCrosshairOverride();
}

template <typename Function>
Function Resolve(HMODULE module, const char* name) noexcept {
    return module == nullptr
        ? nullptr
        : reinterpret_cast<Function>(GetProcAddress(module, name));
}

} // namespace

// --- Absturzmelder -----------------------------------------------------------
// Ohne Debugger und ohne aufbewahrte WER-Dumps ist ein Absturz sonst nicht
// zuzuordnen. Der Filter laeuft nur bei einer unbehandelten Ausnahme, schreibt
// Code und Adresse, ordnet die Adresse unseren Modulen zu und sucht im Stack
// nach Ruecksprungadressen in unserem Code. Damit ist beantwortbar, ob der
// Absturz aus dem VR-Loader kommt oder aus Retail.
LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter = nullptr;
HMODULE g_loaderModule = nullptr;
HMODULE g_bridgeModule = nullptr;

// Ein HMODULE ist die Basisadresse des Abbilds. Die Groesse steht im
// PE-Header, deshalb kommt das ohne psapi aus.
bool ModuleRange(HMODULE module, std::uintptr_t& base,
                 std::uintptr_t& end) noexcept {
    if (module == nullptr) {
        return false;
    }
    base = reinterpret_cast<std::uintptr_t>(module);
    __try {
        const auto* const dos =
            reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }
        const auto* const nt =
            reinterpret_cast<const IMAGE_NT_HEADERS*>(
                reinterpret_cast<const unsigned char*>(module) +
                dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }
        end = base + nt->OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

const char* ModuleNameForAddress(std::uintptr_t address) noexcept {
    std::uintptr_t base = 0;
    std::uintptr_t end = 0;
    if (ModuleRange(g_loaderModule, base, end) &&
        address >= base && address < end) {
        return "GameClient.dll(VR-Loader)";
    }
    if (ModuleRange(g_bridgeModule, base, end) &&
        address >= base && address < end) {
        return "fearvr-d3d9.dll";
    }
    return nullptr;
}

LONG WINAPI FearVrCrashFilter(EXCEPTION_POINTERS* info) noexcept {
    if (info != nullptr && info->ExceptionRecord != nullptr) {
        const auto address = reinterpret_cast<std::uintptr_t>(
            info->ExceptionRecord->ExceptionAddress);
        const char* owner = ModuleNameForAddress(address);
        char message[512];
        _snprintf_s(
            message, sizeof(message), _TRUNCATE,
            "code=0x%08lX address=0x%08zX module=%s step=%s "
            "menu_known=%d menu_active=%d vr_page=%d",
            static_cast<unsigned long>(
                info->ExceptionRecord->ExceptionCode),
            static_cast<std::size_t>(address),
            owner != nullptr ? owner : "retail_or_system",
            g_stereoStep != nullptr ? g_stereoStep : "-",
            g_menuFocusKnown ? 1 : 0, g_menuFocusActive ? 1 : 0,
            g_vrSettingsPageActive ? 1 : 0);
        Report("ERROR", "crash_caught", message);

        // Ruecksprungadressen im Stack, die in unseren Modulen liegen.
        // Beantwortet, ob unser Code im Aufrufpfad war.
        if (info->ContextRecord != nullptr) {
            const auto stackPointer =
                static_cast<std::uintptr_t>(info->ContextRecord->Esp);
            int found = 0;
            for (std::size_t offset = 0;
                 offset < 512 && found < 6; ++offset) {
                const auto slot =
                    reinterpret_cast<std::uintptr_t*>(
                        stackPointer + offset * sizeof(std::uintptr_t));
                std::uintptr_t value = 0;
                __try {
                    value = *slot;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    break;
                }
                const char* frameOwner = ModuleNameForAddress(value);
                if (frameOwner == nullptr) {
                    continue;
                }
                std::uintptr_t base = 0;
                std::uintptr_t end = 0;
                ModuleRange(
                    frameOwner[0] == 'G' ? g_loaderModule : g_bridgeModule,
                    base, end);
                char frame[160];
                _snprintf_s(
                    frame, sizeof(frame), _TRUNCATE,
                    "frame=%d module=%s rva=0x%08zX", found, frameOwner,
                    static_cast<std::size_t>(value - base));
                Report("ERROR", "crash_frame", frame);
                ++found;
            }
            if (found == 0) {
                Report(
                    "ERROR", "crash_frame",
                    "Keine Ruecksprungadresse aus dem VR-Loader im Stack.");
            }
        }
    }
    return g_previousExceptionFilter != nullptr
               ? g_previousExceptionFilter(info)
               : EXCEPTION_CONTINUE_SEARCH;
}

bool InstallStereoHook(void* masterDatabase, HMODULE bridge) noexcept {
    if (masterDatabase == nullptr || bridge == nullptr) {
        return false;
    }

    g_bridgeModule = bridge;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&FearVrCrashFilter), &g_loaderModule);
    g_previousExceptionFilter =
        SetUnhandledExceptionFilter(&FearVrCrashFilter);

    g_isHostConnected =
        Resolve<IsHostConnectedFunction>(
            bridge, "FearVr_IsHostConnected");
    g_isStereoAvailable =
        Resolve<IsStereoAvailableFunction>(
            bridge, "FearVr_IsStereoAvailable");
    g_isStereoEnabled =
        Resolve<IsStereoEnabledFunction>(
            bridge, "FearVr_IsStereoEnabled");
    g_setStereoEnabled =
        Resolve<SetStereoEnabledFunction>(
            bridge, "FearVr_SetStereoEnabled");
    g_setFovScalePercent =
        Resolve<SetFovScalePercentFunction>(
            bridge, "FearVr_SetFovScalePercent");
    g_getRenderScalePercent =
        Resolve<GetRenderScalePercentFunction>(
            bridge, "FearVr_GetRenderScalePercent");
    g_setRenderScalePercent =
        Resolve<SetRenderScalePercentFunction>(
            bridge, "FearVr_SetRenderScalePercent");
    g_isTranslationEnabled =
        Resolve<GetBooleanOptionFunction>(
            bridge, "FearVr_IsTranslationEnabled");
    g_setTranslationEnabled =
        Resolve<SetBooleanOptionFunction>(
            bridge, "FearVr_SetTranslationEnabled");
    g_isStereoHudEnabled =
        Resolve<GetBooleanOptionFunction>(
            bridge, "FearVr_IsStereoHudEnabled");
    g_setStereoHudEnabled =
        Resolve<SetBooleanOptionFunction>(
            bridge, "FearVr_SetStereoHudEnabled");
    g_isComfortModeEnabled =
        Resolve<GetBooleanOptionFunction>(
            bridge, "FearVr_IsComfortModeEnabled");
    g_setComfortModeEnabled =
        Resolve<SetBooleanOptionFunction>(
            bridge, "FearVr_SetComfortModeEnabled");
    g_setMenuActive =
        Resolve<SetMenuActiveFunction>(bridge, "FearVr_SetMenuActive");
    g_requestRecenter =
        Resolve<RequestRecenterFunction>(
            bridge, "FearVr_RequestRecenter");
    g_isFlatPanelActive =
        Resolve<IsFlatPanelActiveFunction>(
            bridge, "FearVr_IsFlatPanelActive");
    g_registerStereoToggle =
        Resolve<RegisterStereoToggleFunction>(
            bridge, "FearVr_RegisterStereoToggleCallback");
    g_getRenderRequest =
        Resolve<GetRenderRequestFunction>(
            bridge, "FearVr_GetRenderRequest");
    g_waitForNewRenderRequest =
        Resolve<WaitForNewRenderRequestFunction>(
            bridge, "FearVr_WaitForNewRenderRequest");
    g_getInputState =
        Resolve<GetInputStateFunction>(
            bridge, "FearVr_GetInputState");
    g_submitHapticRequest =
        Resolve<SubmitHapticRequestFunction>(
            bridge, "FearVr_SubmitHapticRequest");
    g_beginEye =
        Resolve<BeginEyeFunction>(bridge, "FearVr_BeginEye");
    g_captureEye =
        Resolve<CaptureEyeFunction>(bridge, "FearVr_CaptureEye");
    g_endStereoFrame =
        Resolve<EndStereoFrameFunction>(
            bridge, "FearVr_EndStereoFrame");
    g_reportHookStatus =
        Resolve<ReportHookStatusFunction>(
            bridge, "FearVr_ReportHookStatus");
    if (g_isHostConnected == nullptr ||
        g_isStereoAvailable == nullptr ||
        g_isStereoEnabled == nullptr ||
        g_setStereoEnabled == nullptr ||
        g_setFovScalePercent == nullptr ||
        g_getRenderScalePercent == nullptr ||
        g_setRenderScalePercent == nullptr ||
        g_isTranslationEnabled == nullptr ||
        g_setTranslationEnabled == nullptr ||
        g_isStereoHudEnabled == nullptr ||
        g_setStereoHudEnabled == nullptr ||
        g_isComfortModeEnabled == nullptr ||
        g_setComfortModeEnabled == nullptr ||
        g_requestRecenter == nullptr ||
        g_registerStereoToggle == nullptr ||
        g_getRenderRequest == nullptr ||
        g_waitForNewRenderRequest == nullptr ||
        g_beginEye == nullptr ||
        g_captureEye == nullptr || g_endStereoFrame == nullptr) {
        Report(
            "ERROR", "stereo_bridge_exports_missing",
            "The staged bridge lacks one or more M3 exports.");
        return false;
    }
    if (!g_isStereoAvailable()) {
        Report(
            "INFO", "stereo_hook_not_requested",
            "M3 was not requested; the renderer VTable remains untouched.");
        return true;
    }

    g_client = static_cast<ILTClient*>(FindCurrentInterface(
        masterDatabase, "ILTClient.Default", 105));
    ILTRenderer* const heldRenderer =
        static_cast<ILTRenderer*>(FindCurrentInterface(
            masterDatabase, "ILTRenderer.Default", 0));
    if (g_client == nullptr || heldRenderer == nullptr) {
        Report(
            "ERROR", "engine_interfaces_missing",
            "Versioned ILTClient/ILTRenderer defaults were not found.");
        return false;
    }

    __try {
        g_renderer = g_client->GetRenderer();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_renderer = nullptr;
    }
    if (g_renderer == nullptr) {
        Report(
            "ERROR", "active_renderer_missing",
            "ILTClient::GetRenderer did not return the active renderer.");
        return false;
    }
    Report(
        "INFO",
        g_renderer == heldRenderer
            ? "active_renderer_matches_holder"
            : "active_renderer_differs_from_holder",
        g_renderer == heldRenderer
            ? "ILTClient::GetRenderer matches ILTRenderer.Default."
            : "ILTClient::GetRenderer differs from ILTRenderer.Default; "
              "the active PlayerCamera renderer will be hooked.");
    Report(
        "INFO", "engine_interfaces_found",
        "Read-only lookup found ILTClient 105 and ILTRenderer 0; "
              "the active renderer was resolved through ILTClient.");
    ConfigureComfortOptions();
    InitializeVrSettings();
    if (CommandLineContains(L"-fearvr-input") &&
        !InstallRetailVrMenuHooks()) {
        Report(
            "WARN", "vr_settings_menu_unavailable",
            "The game remains playable, but the in-game VR settings "
            "entry could not be installed.");
    }
    if (!InstallClientInputHook(masterDatabase)) {
        Report(
            "WARN", "client_input_hook_unavailable",
            "M5 input remains disabled; original game input is untouched.");
    }
    g_registerStereoToggle(&SetStereoHookEnabled);
    Report(
        "INFO", "stereo_hook_armed",
        "Renderer VTable remains original in menus; the first verified "
        "playing frame enables native stereo automatically.");
    return true;
}

} // namespace fearvr
