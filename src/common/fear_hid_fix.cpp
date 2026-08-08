#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "fear_hid_fix.h"

#include <algorithm>
#include <cstring>

namespace fearvr {
namespace {

constexpr std::size_t kPatchEnd =
    kRedundantHidDeviceRva + kRedundantHidDeviceBytes.size();

bool IsAllNops(const std::uint8_t* bytes, std::size_t count) noexcept {
    return std::all_of(
        bytes, bytes + count,
        [](const std::uint8_t byte) { return byte == 0x90U; });
}

const std::array<std::uint8_t, 29>& ExpectedLegacyInputHookBytes(
    const std::size_t imageSize) noexcept {
    if (imageSize == kGogFear108ImageSize ||
        imageSize == kSteamFear108ImageSize) {
        return kUnpackedLegacyInputHookBytes;
    }
    return kLegacyInputHookBytes;
}

} // namespace

bool MatchesFear108HidSignatures(const std::uint8_t* image,
                                 const std::size_t imageSize) noexcept {
    if (image == nullptr || imageSize < kPatchEnd) {
        return false;
    }
    const auto& legacyBytes = ExpectedLegacyInputHookBytes(imageSize);
    return std::memcmp(image + kLegacyInputHookRva,
                       legacyBytes.data(),
                       legacyBytes.size()) == 0 &&
           std::memcmp(image + kRedundantHidDeviceRva,
                       kRedundantHidDeviceBytes.data(),
                       kRedundantHidDeviceBytes.size()) == 0;
}

bool HasCompatibleFear108HidPatchState(
    const std::uint8_t* image,
    const std::size_t imageSize) noexcept {
    if (image == nullptr || imageSize < kPatchEnd) {
        return false;
    }

    const auto& legacyBytes = ExpectedLegacyInputHookBytes(imageSize);
    const bool legacyOriginal =
        std::memcmp(image + kLegacyInputHookRva,
                    legacyBytes.data(),
                    legacyBytes.size()) == 0;
    const bool legacyPatched =
        IsAllNops(image + kLegacyInputHookRva,
                  kLegacyInputHookBytes.size());
    const bool hidOriginal =
        std::memcmp(image + kRedundantHidDeviceRva,
                    kRedundantHidDeviceBytes.data(),
                    kRedundantHidDeviceBytes.size()) == 0;
    const bool hidPatched =
        IsAllNops(image + kRedundantHidDeviceRva,
                  kRedundantHidDeviceBytes.size());
    return (legacyOriginal || legacyPatched) &&
           (hidOriginal || hidPatched);
}

FearHidFixResult ApplyFear108HidFix() noexcept {
    auto* const image = reinterpret_cast<std::uint8_t*>(
        GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return FearHidFixResult::NotFear108;
    }

    const auto* const dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return FearHidFixResult::NotFear108;
    }
    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.TimeDateStamp != kFear108TimeDateStamp ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        return FearHidFixResult::NotFear108;
    }

    const std::size_t imageSize = nt->OptionalHeader.SizeOfImage;
    if (imageSize < kPatchEnd) {
        return FearHidFixResult::ImageTooSmall;
    }

    const bool legacyAlreadyPatched =
        IsAllNops(image + kLegacyInputHookRva,
                  kLegacyInputHookBytes.size());
    const bool hidAlreadyPatched =
        IsAllNops(image + kRedundantHidDeviceRva,
                  kRedundantHidDeviceBytes.size());
    if (legacyAlreadyPatched && hidAlreadyPatched) {
        return FearHidFixResult::AlreadyApplied;
    }
    if (!HasCompatibleFear108HidPatchState(image, imageSize)) {
        return FearHidFixResult::SignatureMismatch;
    }

    auto* const patchStart = image + kLegacyInputHookRva;
    constexpr std::size_t patchSpan =
        kPatchEnd - kLegacyInputHookRva;
    DWORD oldProtection = 0;
    if (!VirtualProtect(
            patchStart, patchSpan, PAGE_EXECUTE_READWRITE,
            &oldProtection)) {
        return FearHidFixResult::ProtectionFailed;
    }

    if (!legacyAlreadyPatched) {
        std::memset(image + kLegacyInputHookRva, 0x90,
                    kLegacyInputHookBytes.size());
    }
    if (!hidAlreadyPatched) {
        std::memset(image + kRedundantHidDeviceRva, 0x90,
                    kRedundantHidDeviceBytes.size());
    }
    FlushInstructionCache(GetCurrentProcess(), patchStart, patchSpan);

    DWORD ignored = 0;
    VirtualProtect(patchStart, patchSpan, oldProtection, &ignored);
    return FearHidFixResult::Applied;
}

const char* FearHidFixResultName(const FearHidFixResult result) noexcept {
    switch (result) {
    case FearHidFixResult::Applied:
        return "applied";
    case FearHidFixResult::AlreadyApplied:
        return "already_applied";
    case FearHidFixResult::NotFear108:
        return "not_fear_108";
    case FearHidFixResult::ImageTooSmall:
        return "image_too_small";
    case FearHidFixResult::SignatureMismatch:
        return "signature_mismatch";
    case FearHidFixResult::ProtectionFailed:
        return "protection_failed";
    }
    return "unknown";
}

} // namespace fearvr
