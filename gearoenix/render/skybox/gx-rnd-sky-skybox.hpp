#ifndef GEAROENIX_RENDER_SKYBOX_SKYBOX_HPP
#define GEAROENIX_RENDER_SKYBOX_SKYBOX_HPP
#include "../../core/asset/gx-cr-asset.hpp"
#include "../../core/gx-cr-static.hpp"
#include "../../core/sync/gx-cr-sync-end-caller.hpp"
#include "gx-rnd-sky-type.hpp"

namespace gearoenix::system::stream {
class Stream;
}

namespace gearoenix::render::material {
class Material;
}

namespace gearoenix::render::mesh {
class Mesh;
}

namespace gearoenix::render::engine {
class Engine;
}

namespace gearoenix::render::reflection {
class Baked;
}

namespace gearoenix::render::skybox {
class Skybox : public core::asset::Asset {
    GX_GET_CREF_PRT(std::shared_ptr<mesh::Mesh>, msh)
    GX_GET_CREF_PRT(std::shared_ptr<reflection::Baked>, baked_reflection)
    GX_GET_CREF_PRT(std::shared_ptr<material::Material>, mat)
    GX_GET_CVAL_PRT(Type, skybox_type)
    GX_GETSET_VAL_PRT(double, layer, 0.0)
    GX_GETSET_VAL_PRT(bool, enabled, true)
protected:
    engine::Engine* const e;
    void init(const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept;

    Skybox(Type t, core::Id id, std::string name, system::stream::Stream* s, engine::Engine* e, const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept;
    Skybox(Type t, core::Id id, std::string name, engine::Engine* e, const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept;

public:
    ~Skybox() noexcept override;
    virtual void update() noexcept;
};
}
#endif
