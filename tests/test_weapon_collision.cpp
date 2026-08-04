#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "weapon_collision.h"

namespace {

constexpr std::uint64_t kFrameNs = 11'000'000ULL; // ~90 Hz

} // namespace

int main() {
    using namespace fearvr;

    assert(WeaponCollisionRetraction(80.0F, 200.0F, true) == 0.0F);
    assert(WeaponCollisionRetraction(80.0F, 0.0F, false) == 0.0F);

    {
        const float retraction = WeaponCollisionRetraction(80.0F, 60.0F, true);
        assert(std::fabs(retraction - 28.0F) < 0.001F);
    }

    assert(WeaponCollisionRetraction(80.0F, 0.0F, true) == 80.0F);
    assert(WeaponCollisionRetraction(80.0F, 20.0F, true, 8.0F, 30.0F) == 30.0F);
    assert(WeaponCollisionRetraction(0.0F, 20.0F, true) == 0.0F);
    assert(WeaponCollisionRetraction(
               std::numeric_limits<float>::infinity(), 20.0F, true) == 0.0F);
    assert(WeaponCollisionRetraction(
               80.0F, std::numeric_limits<float>::infinity(), true) == 0.0F);

    assert(WeaponCollisionPlaneCorrection(12.0F) == 0.0F);
    assert(WeaponCollisionPlaneCorrection(5.0F) == 3.0F);
    assert(WeaponCollisionPlaneCorrection(-10.0F) == 18.0F);
    assert(WeaponCollisionPlaneCorrection(-100.0F, 8.0F, 30.0F) == 30.0F);
    assert(WeaponCollisionPlaneCorrection(
               std::numeric_limits<float>::infinity()) == 0.0F);

    assert(!WeaponMuzzleObstructed(80.0F, 200.0F, true));
    assert(WeaponMuzzleObstructed(80.0F, 88.0F, true));
    assert(!WeaponMuzzleObstructed(80.0F, 0.0F, false));

    {
        WeaponCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        UpdateWeaponCollision(state, 0.0F, now);
        now += kFrameNs;
        const float first = UpdateWeaponCollision(state, 60.0F, now);
        assert(first > 0.0F && first < 60.0F);
        for (int frame = 0; frame < 12; ++frame) {
            now += kFrameNs;
            UpdateWeaponCollision(state, 60.0F, now);
        }
        assert(state.retractionUnits > 59.0F);
    }

    {
        WeaponCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        UpdateWeaponCollision(state, 50.0F, now);
        for (int frame = 0; frame < 5; ++frame) {
            now += kFrameNs;
            assert(UpdateWeaponCollision(state, 0.0F, now) == 50.0F);
        }
        bool partiallyReleased = false;
        for (int frame = 0; frame < 30; ++frame) {
            now += kFrameNs;
            const float value = UpdateWeaponCollision(state, 0.0F, now);
            if (value > 0.0F && value < 50.0F) {
                partiallyReleased = true;
            }
        }
        assert(partiallyReleased);
        assert(state.retractionUnits < 2.0F);
    }

    {
        WeaponCollisionState state{};
        std::uint64_t now = 1'000'000'000ULL;
        UpdateWeaponCollision(state, 40.0F, now);
        for (int frame = 0; frame < 40; ++frame) {
            now += kFrameNs;
            const float target = (frame % 2 == 0) ? 40.0F : 0.0F;
            UpdateWeaponCollision(state, target, now);
        }
        assert(state.retractionUnits > 35.0F);
    }

    {
        WeaponCollisionState state{};
        UpdateWeaponCollision(state, 40.0F, kFrameNs);
        ResetWeaponCollision(state);
        assert(state.retractionUnits == 0.0F);
        assert(!state.haveUpdate);
    }

    return 0;
}
