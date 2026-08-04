#pragma once

#include <cstdint>

namespace fearvr {

// ClientServerShared.h assigns these bits to both the visible character model
// and its separate ray-hit proxy.  A first-person VR camera must collide with
// level/prop geometry, but never with either representation of a character:
// the proxy is deliberately much larger than the animated or ragdoll body.
constexpr std::uint32_t kCharacterUserFlag = 1U << 7;
constexpr std::uint32_t kCharacterHitBoxUserFlag = 1U << 11;

inline bool IsCharacterCollisionObject(
    std::uint32_t userFlags) noexcept {
    return (userFlags &
            (kCharacterUserFlag | kCharacterHitBoxUserFlag)) != 0;
}

} // namespace fearvr
