//#include "gx-rnd-shd-shader.hpp"
//#include "../../core/gx-cr-types.hpp"
//#include "../../system/stream/gx-sys-stm-stream.hpp"
//#include "../../system/gx-sys-log.hpp"
//#include "../material/gx-rnd-mat-depth.hpp"
//#include "../material/gx-rnd-mat-directional-d2-speculated-nonreflective-full-opaque.hpp"
//#include "../material/gx-rnd-mat-shadeless-colored-matte-nonreflective-shadowless-opaque.hpp"
//#include "../engine/gx-rnd-eng-engine.hpp"
//
//const std::vector<gearoenix::render::shader::stage::Id> gearoenix::render::shader::Shader::graphic_2_stage = {
//    gearoenix::render::shader::stage::VERTEX,
//    gearoenix::render::shader::stage::FRAGMENT
//};
//
//const std::vector<gearoenix::render::shader::ResourceDescription>
//    gearoenix::render::shader::Shader::buffer_rsc_des = {
//        { { gearoenix::render::shader::stage::VERTEX },
//            1,
//            gearoenix::render::shader::ResourceDescription::UNIFORM }
//    };
//
//const std::vector<gearoenix::render::shader::ResourceDescription>
//    gearoenix::render::shader::Shader::textured_rsc_des = {
//        { {
//              gearoenix::render::shader::stage::VERTEX,
//              gearoenix::render::shader::stage::FRAGMENT,
//          },
//            1,
//            gearoenix::render::shader::ResourceDescription::UNIFORM },
//        { { gearoenix::render::shader::stage::FRAGMENT },
//            1,
//            gearoenix::render::shader::ResourceDescription::SAMPER2D },
//    };
//
//const std::vector<gearoenix::render::shader::VertexAttribute>
//    gearoenix::render::shader::Shader::has_pos = {
//        gearoenix::render::shader::VEC3F32
//    };
//const std::vector<gearoenix::render::shader::VertexAttribute>
//    gearoenix::render::shader::Shader::has_pos_nrm = {
//        gearoenix::render::shader::VEC3F32,
//        gearoenix::render::shader::VEC3F32
//    };
//const std::vector<gearoenix::render::shader::VertexAttribute>
//    gearoenix::render::shader::Shader::has_pos_nrm_uv = {
//        gearoenix::render::shader::VEC3F32,
//        gearoenix::render::shader::VEC3F32,
//        gearoenix::render::shader::VEC2F32
//    };
//const std::vector<gearoenix::render::shader::VertexAttribute>
//    gearoenix::render::shader::Shader::has_pos_uv = {
//        gearoenix::render::shader::VEC3F32,
//        gearoenix::render::shader::VEC2F32
//    };
//
//gearoenix::render::shader::Shader::Shader(core::Id my_id)
//    : core::asset::Asset(my_id, core::asset::Type::SHADER)
//{
//}
//
//gearoenix::render::shader::Shader::~Shader() {}
//
//gearoenix::render::shader::Shader* gearoenix::render::shader::Shader::read(core::Id sid, system::stream::Stream* file, Engine* engine, core::sync::EndCaller<core::sync::EndCallerIgnore> end)
//{
//    return engine->create_shader(sid, file, end);
//}
//
//const std::vector<gearoenix::render::shader::ResourceDescription>& gearoenix::render::shader::Shader::get_resources_descriptions(Id id)
//{
//    switch (id) {
//    case DEPTH_POS:
//        return buffer_rsc_des;
//    default:
//        GXLOGF("Unexpected! id: " << id);
//    }
//}
//
//unsigned int gearoenix::render::shader::Shader::get_uniform_size(core::Id id)
//{
//    switch (id) {
//    case DEPTH_POS:
//        return sizeof(material::Depth::Uniform);
//    default:
//        GXLOGF("Unexpected! id: " << id);
//    }
//}
//
//const std::vector<gearoenix::render::shader::VertexAttribute>& gearoenix::render::shader::Shader::get_vertex_atributes(Id id)
//{
//    switch (id) {
//    case DEPTH_POS:
//        return has_pos;
//    default:
//        GXLOGF("Unexpected! id: " << id);
//    }
//}
//
//bool gearoenix::render::shader::Shader::is_shadow_caster(core::Id sid)
//{
//    shader::Id shdid = static_cast<shader::Id>(sid);
//    switch (shdid) {
//    case DIRECTIONAL_COLORED_MATTE_BAKED_CASTER_CUTOFF:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_CASTER_OPAQUE:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_FULL_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_BAKED_FULL_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_NONREFLECTIVE_FULL_OPAQUE:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_CASTER_OPAQUE:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_CASTER_OPAQUE:
//        return true;
//    case DIRECTIONAL_COLORED_MATTE_BAKED_SHADOWLESS_CUTOFF:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_SHADOWLESS_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case FONT_COLORED:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_SHADOWLESS_TRANSPARENT:
//    case SHADELESS_CUBE_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_SHADOWLESS_TRANSPARENT:
//        return false;
//    default:
//        GXUNIMPLEMENTED;
//    }
//}
//
//bool gearoenix::render::shader::Shader::is_transparent(core::Id sid)
//{
//    shader::Id shdid = static_cast<shader::Id>(sid);
//    switch (shdid) {
//    case DIRECTIONAL_COLORED_MATTE_BAKED_CASTER_CUTOFF:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_CASTER_OPAQUE:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_FULL_OPAQUE:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_SHADOWLESS_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_BAKED_FULL_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_NONREFLECTIVE_FULL_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_CASTER_OPAQUE:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case SHADELESS_CUBE_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_CASTER_OPAQUE:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//        return false;
//    case DIRECTIONAL_COLORED_MATTE_BAKED_CASTER_TRANSPARENT:
//    case FONT_COLORED:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_SHADOWLESS_TRANSPARENT:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_SHADOWLESS_TRANSPARENT:
//        return true;
//    default:
//        GXUNIMPLEMENTED;
//    }
//}
//
//gearoenix::core::Id gearoenix::render::shader::Shader::get_shadow_caster_shader_id(core::Id sid)
//{
//    switch (get_vertex_real_count(sid)) {
//    case 3:
//        return DEPTH_POS;
//    case 5:
//        return DEPTH_POS_UV;
//    case 6:
//        return DEPTH_POS_NRM;
//    case 8:
//        return DEPTH_POS_NRM_UV;
//    default:
//        GXUNEXPECTED;
//    }
//}
//
//unsigned int gearoenix::render::shader::Shader::get_vertex_real_count(core::Id id)
//{
//    switch (id) {
//    case DEPTH_POS:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_CASTER_OPAQUE:
//    case SHADELESS_COLORED_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case SHADELESS_CUBE_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//        return 3;
//    case DEPTH_POS_UV:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_CASTER_OPAQUE:
//    case SHADELESS_D2_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//        return 5;
//    case DEPTH_POS_NRM:
//    case DIRECTIONAL_COLORED_MATTE_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_CASTER_OPAQUE:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_FULL_OPAQUE:
//    case DIRECTIONAL_COLORED_SPECULATED_BAKED_SHADOWLESS_OPAQUE:
//    case DIRECTIONAL_COLORED_SPECULATED_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//        return 6;
//    case DEPTH_POS_NRM_UV:
//    case DIRECTIONAL_D2_SPECULATED_BAKED_FULL_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_NONREFLECTIVE_FULL_OPAQUE:
//    case DIRECTIONAL_D2_SPECULATED_NONREFLECTIVE_SHADOWLESS_OPAQUE:
//        return 8;
//    default:
//        GXLOGF("Unexpected! id: " << id);
//    }
//}
