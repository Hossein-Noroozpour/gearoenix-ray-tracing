#include "gx-rnd-pip-unlit-resource-set.hpp"
#include "../buffer/gx-rnd-buf-framed-uniform.hpp"
#include "../material/gx-rnd-mat-unlit.hpp"
#include "gx-rnd-pip-unlit.hpp"

gearoenix::render::pipeline::UnlitResourceSet::UnlitResourceSet(std::shared_ptr<Unlit const> pip) noexcept
    : ResourceSet(std::move(pip))
{
}

gearoenix::render::pipeline::UnlitResourceSet::~UnlitResourceSet() noexcept = default;

void gearoenix::render::pipeline::UnlitResourceSet::set_material(const material::Material* m) noexcept
{

    material_uniform_buffer = m->get_uniform_buffers()->get_buffer();
    auto* const unlit_mat = reinterpret_cast<const material::Unlit*>(m);
    color = unlit_mat->get_color_texture().get();
}

void gearoenix::render::pipeline::UnlitResourceSet::set_mesh(const mesh::Mesh* m) noexcept
{
    msh = m;
}

void gearoenix::render::pipeline::UnlitResourceSet::set_node_uniform_buffer(buffer::Uniform* const b) noexcept
{
    node_uniform_buffer = b;
}

void gearoenix::render::pipeline::UnlitResourceSet::clean() noexcept
{
    material_uniform_buffer = nullptr;
    node_uniform_buffer = nullptr;
    msh = nullptr;
    color = nullptr;
}
