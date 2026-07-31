#include <cassert>

#include "render_scale.h"

int main() {
    using fearvr::CalculateRenderScaleSize;

    const auto native =
        CalculateRenderScaleSize(1280, 1024, 50, 8192, 8192);
    assert(native.width == 1280);
    assert(native.height == 1024);
    assert(native.percent == 100);

    const auto scaled =
        CalculateRenderScaleSize(1280, 1024, 150, 8192, 8192);
    assert(scaled.width == 1920);
    assert(scaled.height == 1536);
    assert(scaled.percent == 150);

    const auto capped =
        CalculateRenderScaleSize(1920, 1080, 200, 2560, 2048);
    assert(capped.width == 2554);
    assert(capped.height == 1436);
    assert(capped.percent == 133);

    const auto invalid =
        CalculateRenderScaleSize(2560, 1440, 150, 2048, 2048);
    assert(invalid.width == 0);
    assert(invalid.height == 0);
    return 0;
}
