#include <cassert>

#include "render_pacing.h"

int main() {
    using fearvr::ShouldWaitForNewRenderRequest;

    // The first stereo frame never waits.
    assert(!ShouldWaitForNewRenderRequest(0, 1));

    // Reaching the camera render again with the same OpenXR request is the
    // only case that pacing should block.
    assert(ShouldWaitForNewRenderRequest(41, 41));

    // A request that advanced since the previous stereo render is already
    // fresh. Waiting here caused the beta.8 half-rate regression.
    assert(!ShouldWaitForNewRenderRequest(41, 42));

    // A host restart may reset its frame counter. Treat the newly published
    // request as fresh instead of waiting on an unrelated numeric ordering.
    assert(!ShouldWaitForNewRenderRequest(900, 1));

    // Invalid request IDs are never pacing candidates.
    assert(!ShouldWaitForNewRenderRequest(41, 0));
    return 0;
}
