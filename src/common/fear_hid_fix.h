#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fearvr {

// F.E.A.R. 1.08 retail executable. EchoPatch documents these two regions as
// the redundant legacy input/HID initialization responsible for the
// well-known frame-rate degradation after several minutes. The bytes below
// were independently verified against our unmodified local 1.08 executable;
// they are checked in full before any process memory is changed.
inline constexpr std::uint32_t kFear108TimeDateStamp = 0x44EF6AE6U;
inline constexpr std::size_t kLegacyInputHookRva = 0x84057U;
inline constexpr std::array<std::uint8_t, 29> kLegacyInputHookBytes{
    0x6A, 0x00, 0x6A, 0x00, 0xFF, 0x15, 0x1C, 0x82, 0x70, 0x00,
    0x50, 0x68, 0xC0, 0x0C, 0x48, 0x00, 0x6A, 0x0D, 0xFF, 0x15,
    0xA4, 0x88, 0x70, 0x00, 0xA3, 0xBC, 0x5B, 0x57, 0x00};

inline constexpr std::size_t kRedundantHidDeviceRva = 0x840DDU;
inline constexpr std::array<std::uint8_t, 22> kRedundantHidDeviceBytes{
    0x8B, 0x3F, 0x8B, 0x0F, 0x6A, 0x01, 0x8D, 0x54, 0x24, 0x0C,
    0x52, 0x68, 0xD0, 0x2E, 0x48, 0x00, 0x6A, 0x01, 0x57, 0xFF,
    0x51, 0x10};

enum class FearHidFixResult {
    Applied,
    AlreadyApplied,
    NotFear108,
    ImageTooSmall,
    SignatureMismatch,
    ProtectionFailed,
};

bool MatchesFear108HidSignatures(const std::uint8_t* image,
                                 std::size_t imageSize) noexcept;

// Accepts an untouched image as well as a safely, partially applied patch.
// Some supported FEAR.exe wrappers patch only one of the two regions before
// our dinput8 proxy is loaded. The remaining region can still be verified and
// completed without accepting unknown code.
bool HasCompatibleFear108HidPatchState(const std::uint8_t* image,
                                       std::size_t imageSize) noexcept;

FearHidFixResult ApplyFear108HidFix() noexcept;

const char* FearHidFixResultName(FearHidFixResult result) noexcept;

} // namespace fearvr
