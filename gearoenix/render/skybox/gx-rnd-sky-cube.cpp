#include "gx-rnd-sky-cube.hpp"
#include "../../core/asset/gx-cr-asset-manager.hpp"
#include "../../system/gx-sys-application.hpp"
#include "../engine/gx-rnd-eng-engine.hpp"
#include "../material/gx-rnd-mat-skybox-cube.hpp"
#include "../reflection/gx-rnd-rfl-baked.hpp"
#include "../texture/gx-rnd-txt-manager.hpp"
#include "../texture/gx-rnd-txt-texture-cube.hpp"

gearoenix::render::skybox::Cube::Cube(
    const core::Id my_id,
    std::string name,
    system::stream::Stream* const s,
    engine::Engine* const e,
    const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept
    : Skybox(Type::Cube, my_id, std::move(name), s, e, c)
    , mat_cube(new material::SkyboxCube(s, e, c))
{
    mat = mat_cube;
    auto* const txt_mgr = e->get_system_application()->get_asset_manager()->get_texture_manager();
    core::sync::EndCaller<texture::Texture> txt_call([c](const std::shared_ptr<texture::Texture>&) {});
    auto irr = std::dynamic_pointer_cast<texture::TextureCube>(txt_mgr->read_gx3d(
        this->name + "-irr", s, txt_call));
    auto rad = std::dynamic_pointer_cast<texture::TextureCube>(txt_mgr->read_gx3d(
        this->name + "-rad", s, txt_call));
    baked_reflection = std::make_shared<reflection::Baked>(this->name + "-ref", std::move(irr), std::move(rad));
}

gearoenix::render::skybox::Cube::Cube(
    const core::Id my_id,
    std::string name,
    engine::Engine* const e,
    const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept
    : Skybox(Type::Cube, my_id, std::move(name), e, c)
    , mat_cube(new material::SkyboxCube(e, c))
{
    mat = mat_cube;
}

gearoenix::render::skybox::Cube::~Cube() noexcept = default;
