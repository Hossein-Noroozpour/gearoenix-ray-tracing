#ifndef GEAROENIX_RENDER_PIPELINE_TYPE_HPP
#define GEAROENIX_RENDER_PIPELINE_TYPE_HPP

#include <string>

namespace gearoenix::render::pipeline {
enum struct Type : unsigned int {
    DeferredPbr = 1,
    ForwardPbr = 2,
    GBufferFiller = 3,
    LightBlooming = 4,
    ShadowAccumulator = 5,
    ShadowMapper = 6,
    SSAO = 7,
    SSR = 8,
    Unlit = 9,
    SkyboxCube = 10,
    SkyboxEquirectangular = 11,
    IrradianceConvoluter = 12,
    RadianceConvoluter = 13,
};
}

namespace std {
[[nodiscard]] string to_string(const gearoenix::render::pipeline::Type t) noexcept;
}
#endif
