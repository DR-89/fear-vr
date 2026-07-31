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
    return 0;
}
