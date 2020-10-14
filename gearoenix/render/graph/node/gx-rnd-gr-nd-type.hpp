#ifndef GEAROENIX_RENDER_GRAPH_NODE_TYPE_HPP
#define GEAROENIX_RENDER_GRAPH_NODE_TYPE_HPP
#include "../../../core/gx-cr-types.hpp"
namespace gearoenix::render::graph::node {
enum struct Type : core::TypeId {
    ForwardPbr = 1,
    ShadowMapper = 2,
    Unlit = 3,
    SkyboxCube = 4,
    SkyboxEquirectangular = 5,
    IrradianceConvoluter = 6,
    RadianceConvoluter = 7,
    MipmapGenerator = 8,
};
}
#endif