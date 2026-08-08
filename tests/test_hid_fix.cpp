#include "fear_hid_fix.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    const std::size_t imageSize =
        fearvr::kRedundantHidDeviceRva +
        fearvr::kRedundantHidDeviceBytes.size();
    std::vector<std::uint8_t> image(imageSize, 0);

    std::copy(
        fearvr::kLegacyInputHookBytes.begin(),
        fearvr::kLegacyInputHookBytes.end(),
        image.begin() + fearvr::kLegacyInputHookRva);
    std::copy(
        fearvr::kRedundantHidDeviceBytes.begin(),
        fearvr::kRedundantHidDeviceBytes.end(),
        image.begin() + fearvr::kRedundantHidDeviceRva);

    assert(fearvr::MatchesFear108HidSignatures(
        image.data(), image.size()));
    assert(fearvr::HasCompatibleFear108HidPatchState(
        image.data(), image.size()));

    std::fill_n(
        image.begin() + fearvr::kLegacyInputHookRva,
        fearvr::kLegacyInputHookBytes.size(), std::uint8_t{0x90U});
    assert(!fearvr::MatchesFear108HidSignatures(
        image.data(), image.size()));
    assert(fearvr::HasCompatibleFear108HidPatchState(
        image.data(), image.size()));

    std::copy(
        fearvr::kLegacyInputHookBytes.begin(),
        fearvr::kLegacyInputHookBytes.end(),
        image.begin() + fearvr::kLegacyInputHookRva);
    image[fearvr::kRedundantHidDeviceRva + 3] ^= 0x01U;
    assert(!fearvr::MatchesFear108HidSignatures(
        image.data(), image.size()));
    assert(!fearvr::HasCompatibleFear108HidPatchState(
        image.data(), image.size()));
    assert(!fearvr::MatchesFear108HidSignatures(
        image.data(), fearvr::kLegacyInputHookRva));
    assert(!fearvr::HasCompatibleFear108HidPatchState(
        image.data(), fearvr::kLegacyInputHookRva));

    std::vector<std::uint8_t> steamImage(
        fearvr::kSteamFear108ImageSize, 0);
    std::copy(
        fearvr::kUnpackedLegacyInputHookBytes.begin(),
        fearvr::kUnpackedLegacyInputHookBytes.end(),
        steamImage.begin() + fearvr::kLegacyInputHookRva);
    std::copy(
        fearvr::kRedundantHidDeviceBytes.begin(),
        fearvr::kRedundantHidDeviceBytes.end(),
        steamImage.begin() + fearvr::kRedundantHidDeviceRva);
    assert(fearvr::MatchesFear108HidSignatures(
        steamImage.data(), steamImage.size()));
    assert(fearvr::HasCompatibleFear108HidPatchState(
        steamImage.data(), steamImage.size()));

    std::vector<std::uint8_t> gogImage(
        fearvr::kGogFear108ImageSize, 0);
    std::copy(
        fearvr::kUnpackedLegacyInputHookBytes.begin(),
        fearvr::kUnpackedLegacyInputHookBytes.end(),
        gogImage.begin() + fearvr::kLegacyInputHookRva);
    std::copy(
        fearvr::kRedundantHidDeviceBytes.begin(),
        fearvr::kRedundantHidDeviceBytes.end(),
        gogImage.begin() + fearvr::kRedundantHidDeviceRva);
    assert(fearvr::MatchesFear108HidSignatures(
        gogImage.data(), gogImage.size()));
    assert(fearvr::HasCompatibleFear108HidPatchState(
        gogImage.data(), gogImage.size()));

    std::copy(
        fearvr::kLegacyInputHookBytes.begin(),
        fearvr::kLegacyInputHookBytes.end(),
        gogImage.begin() + fearvr::kLegacyInputHookRva);
    assert(!fearvr::MatchesFear108HidSignatures(
        gogImage.data(), gogImage.size()));
    assert(!fearvr::HasCompatibleFear108HidPatchState(
        gogImage.data(), gogImage.size()));

    std::copy(
        fearvr::kUnpackedLegacyInputHookBytes.begin(),
        fearvr::kUnpackedLegacyInputHookBytes.end(),
        gogImage.begin() + fearvr::kLegacyInputHookRva);
    std::fill_n(
        gogImage.begin() + fearvr::kRedundantHidDeviceRva,
        fearvr::kRedundantHidDeviceBytes.size(), std::uint8_t{0x90U});
    assert(!fearvr::MatchesFear108HidSignatures(
        gogImage.data(), gogImage.size()));
    assert(fearvr::HasCompatibleFear108HidPatchState(
        gogImage.data(), gogImage.size()));
    return 0;
}
