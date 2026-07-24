#pragma once

#include <cstdint>
#include <cstring>

#include "protocol.h"

namespace fearvr {

inline constexpr std::uint64_t PackLuid(std::uint32_t high,
                                        std::uint32_t low) noexcept {
    return (static_cast<std::uint64_t>(high) << 32U) |
           static_cast<std::uint64_t>(low);
}

inline constexpr std::uint32_t LuidHigh(std::uint64_t packed) noexcept {
    return static_cast<std::uint32_t>(packed >> 32U);
}

inline constexpr std::uint32_t LuidLow(std::uint64_t packed) noexcept {
    return static_cast<std::uint32_t>(packed & 0xFFFFFFFFULL);
}

inline bool IsProtocolHeaderValid(const FearVrSharedHeader& header) noexcept {
    return header.magic == FEARVR_PROTOCOL_MAGIC &&
           header.version == FEARVR_PROTOCOL_VERSION &&
           header.headerSize == sizeof(FearVrSharedHeader) &&
           header.slotStructSize == sizeof(FearVrSlot) &&
           header.slotsPerEye == FEARVR_SLOTS_PER_EYE;
}

inline void InitializeProtocolHeader(FearVrSharedHeader& header) noexcept {
    std::memset(&header, 0, sizeof(header));
    header.magic = FEARVR_PROTOCOL_MAGIC;
    header.version = FEARVR_PROTOCOL_VERSION;
    header.headerSize = sizeof(FearVrSharedHeader);
    header.slotStructSize = sizeof(FearVrSlot);
    header.slotsPerEye = FEARVR_SLOTS_PER_EYE;
}

struct ReadyPair {
    std::uint32_t slotIndex{0};
    std::uint64_t frameId{0};
    std::uint64_t generation{0};
    bool found{false};
};

inline ReadyPair FindNewestReadyPair(
    const FearVrSharedHeader& header) noexcept {
    ReadyPair newest;
    for (std::uint32_t index = 0; index < FEARVR_SLOTS_PER_EYE; ++index) {
        const FearVrSlot& left = header.slot[FEARVR_EYE_LEFT][index];
        const FearVrSlot& right = header.slot[FEARVR_EYE_RIGHT][index];
        if (left.state != FEARVR_SLOT_READY ||
            right.state != FEARVR_SLOT_READY ||
            left.frameId == 0 || left.frameId != right.frameId ||
            left.generation == 0 ||
            left.generation != right.generation) {
            continue;
        }
        if (!newest.found || left.frameId > newest.frameId ||
            (left.frameId == newest.frameId &&
             left.generation > newest.generation)) {
            newest.slotIndex = index;
            newest.frameId = left.frameId;
            newest.generation = left.generation;
            newest.found = true;
        }
    }
    return newest;
}

} // namespace fearvr
