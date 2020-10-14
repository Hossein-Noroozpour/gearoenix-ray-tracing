#include "gx-rnd-msh-mesh.hpp"
#include "../buffer/gx-rnd-buf-manager.hpp"
#include "../buffer/gx-rnd-buf-static.hpp"
#include "../engine/gx-rnd-eng-engine.hpp"
#include "../material/gx-rnd-mat-material.hpp"
#include <vector>

gearoenix::render::mesh::Mesh::Mesh(const core::Id my_id, std::string name, const Type t) noexcept
    : core::asset::Asset(my_id, core::asset::Type::Mesh, std::move(name))
    , mesh_type(t)
{
}

gearoenix::render::mesh::Mesh::Mesh(
    const core::Id my_id,
    std::string name,
    system::stream::Stream* const f,
    engine::Engine* const e,
    const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept
    : core::asset::Asset(my_id, core::asset::Type::Mesh, std::move(name))
    , mesh_type(Type::Basic)
{
    const auto vertices_count = f->read<core::Count>();
    std::vector<math::BasicVertex> vertices(vertices_count);
    for (math::BasicVertex& v : vertices) {
        v.read(f);
    }
    std::vector<std::uint32_t> indices;
    f->read(indices);
    box.read(f);
    set_vertices(e, vertices, indices, box, c);
}

gearoenix::render::mesh::Mesh::Mesh(
    const core::Id my_id,
    std::string name,
    const std::vector<math::BasicVertex>& vertices,
    const std::vector<std::uint32_t>& indices,
    const math::Aabb3& b,
    engine::Engine* const e,
    const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept
    : core::asset::Asset(my_id, core::asset::Type::Mesh, std::move(name))
    , mesh_type(Type::Basic)
{
    set_vertices(e, vertices, indices, b, c);
}

gearoenix::render::mesh::Mesh::~Mesh() noexcept
{
    vertex_buffer = nullptr;
    index_buffer = nullptr;
}

void gearoenix::render::mesh::Mesh::set_vertices(
    engine::Engine* const e,
    const std::vector<math::BasicVertex>& vertices,
    const std::vector<std::uint32_t>& indices,
    const math::Aabb3& b,
    const core::sync::EndCaller<core::sync::EndCallerIgnore>& c) noexcept
{
    auto& buf_mgr = *e->get_buffer_manager();
    vertex_buffer = buf_mgr.create_static(vertices, c);
    index_buffer = buf_mgr.create_static(indices, c);
    box = b;
}