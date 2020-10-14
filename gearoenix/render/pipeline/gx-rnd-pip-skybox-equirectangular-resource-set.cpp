#include "gx-rnd-pip-skybox-equirectangular-resource-set.hpp"
#include "../buffer/gx-rnd-buf-framed-uniform.hpp"
#include "../camera/gx-rnd-cmr-camera.hpp"
#include "../material/gx-rnd-mat-skybox-equirectangular.hpp"
#include "gx-rnd-pip-skybox-equirectangular.hpp"

gearoenix::render::pipeline::SkyboxEquirectangularResourceSet::SkyboxEquirectangularResourceSet(
    std::shared_ptr<SkyboxEquirectangular const> pip) noexcept
    : ResourceSet(std::move(pip))
{
}

gearoenix::render::pipeline::SkyboxEquirectangularResourceSet::~SkyboxEquirectangularResourceSet() noexcept = default;

void gearoenix::render::pipeline::SkyboxEquirectangularResourceSet::set_material(const material::SkyboxEquirectangular* m) noexcept
{
    material_uniform_buffer = m->get_uniform_buffers()->get_buffer();
    color = m->get_color_texture().get();
}

void gearoenix::render::pipeline::SkyboxEquirectangularResourceSet::set_mesh(const mesh::Mesh* m) noexcept
{
    msh = m;
}

void gearoenix::render::pipeline::SkyboxEquirectangularResourceSet::set_node_uniform_buffer(buffer::Uniform* b) noexcept
{
    node_uniform_buffer = b;
}

void gearoenix::render::pipeline::SkyboxEquirectangularResourceSet::clean() noexcept
{
    material_uniform_buffer = nullptr;
    node_uniform_buffer = nullptr;
    msh = nullptr;
    color = nullptr;
}

void gearoenix::render::pipeline::SkyboxEquirectangularResourceSet::set_camera(const camera::Camera* const c) noexcept
{
    camera_uniform_buffer = c->get_uniform_buffers()->get_buffer();
}
