#include "gx-gl-submission.hpp"
#ifdef GX_RENDER_OPENGL_ENABLED
#include "../core/ecs/gx-cr-ecs-world.hpp"
#include "../core/macro/gx-cr-mcr-profiler.hpp"
#include "../physics/collider/gx-phs-cld-aabb.hpp"
#include "../physics/collider/gx-phs-cld-frustum.hpp"
#include "../physics/gx-phs-transformation.hpp"
#include "../platform/gx-plt-application.hpp"
#include "../platform/macro/gx-plt-mcr-lock.hpp"
#include "../render/camera/gx-rnd-cmr-camera.hpp"
#include "../render/model/gx-rnd-mdl-model.hpp"
#include "../render/reflection/gx-rnd-rfl-baked.hpp"
#include "../render/reflection/gx-rnd-rfl-runtime.hpp"
#include "../render/scene/gx-rnd-scn-scene.hpp"
#include "../render/skybox/gx-rnd-sky-skybox.hpp"
#include "gx-gl-camera.hpp"
#include "gx-gl-check.hpp"
#include "gx-gl-constants.hpp"
#include "gx-gl-engine.hpp"
#include "gx-gl-loader.hpp"
#include "gx-gl-mesh.hpp"
#include "gx-gl-model.hpp"
#include "gx-gl-reflection.hpp"
#include "gx-gl-shader-deferred-pbr-transparent.hpp"
#include "gx-gl-shader-deferred-pbr.hpp"
#include "gx-gl-shader-final.hpp"
#include "gx-gl-shader-forward-pbr.hpp"
#include "gx-gl-shader-gbuffers-filler.hpp"
#include "gx-gl-shader-irradiance.hpp"
#include "gx-gl-shader-radiance.hpp"
#include "gx-gl-shader-skybox-equirectangular.hpp"
#include "gx-gl-shader-ssao-resolve.hpp"
#include "gx-gl-skybox.hpp"
#include "gx-gl-target.hpp"
#include "gx-gl-texture.hpp"
#include <algorithm>
#include <imgui_impl_opengl3.h>

#if defined(GX_PLATFORM_INTERFACE_ANDROID) || defined(GX_PLATFORM_THREAD_NOT_SUPPORTED)
#define GX_ALGORITHM_EXECUTION
#else
#include <execution>
#include <thread>

#define GX_ALGORITHM_EXECUTION std::execution::par_unseq,
#endif

#ifdef GX_PLATFORM_INTERFACE_ANDROID
#include "../platform/android/gx-plt-gl-context.hpp"
#endif

gearoenix::gl::SubmissionManager::CameraData::CameraData() noexcept
    : threads_opaque_models_data(std::thread::hardware_concurrency())
    , threads_tranclucent_models_data(std::thread::hardware_concurrency())
{
}

void gearoenix::gl::SubmissionManager::initialise_gbuffers() noexcept
{
    auto* const txt_mgr = e.get_texture_manager();
    const auto& cfg = e.get_platform_application().get_base().get_configuration().get_render_configuration();
    gbuffer_width = cfg.get_runtime_resolution_width();
    gbuffer_height = cfg.get_runtime_resolution_height();
    gbuffer_aspect_ratio = static_cast<float>(gbuffer_width) / static_cast<float>(gbuffer_height);
    gbuffer_uv_move_x = 1.0f / static_cast<float>(gbuffer_width);
    gbuffer_uv_move_y = 1.0f / static_cast<float>(gbuffer_height);
    gbuffer_viewport_clip = math::Vec4<sizei>(0, 0, static_cast<sizei>(gbuffer_width), static_cast<sizei>(gbuffer_height));

    const render::texture::TextureInfo position_depth_txt_info {
        .format = render::texture::TextureFormat::RgbaFloat32,
        .sampler_info = render::texture::SamplerInfo {
            .min_filter = render::texture::Filter::Nearest,
            .mag_filter = render::texture::Filter::Nearest,
            .wrap_s = render::texture::Wrap::ClampToEdge,
            .wrap_t = render::texture::Wrap::ClampToEdge,
            .wrap_r = render::texture::Wrap::ClampToEdge,
            .anisotropic_level = 0,
        },
        .width = gbuffer_width,
        .height = gbuffer_height,
        .depth = 0,
        .type = render::texture::Type::Texture2D,
        .has_mipmap = false,
    };
    position_depth_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-position-depth", {}, position_depth_txt_info, GX_DEFAULT_IGNORED_END_CALLER));

    auto albedo_metallic_txt_info = position_depth_txt_info;
    albedo_metallic_txt_info.format = render::texture::TextureFormat::RgbaFloat16;
    albedo_metallic_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-albedo-metallic", {}, albedo_metallic_txt_info, GX_DEFAULT_IGNORED_END_CALLER));

    auto normal_ao_txt_info = position_depth_txt_info;
    normal_ao_txt_info.format = render::texture::TextureFormat::RgbaFloat16;
    normal_ao_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-normal-ao", {}, normal_ao_txt_info, GX_DEFAULT_IGNORED_END_CALLER));

    auto emission_roughness_txt_info = position_depth_txt_info;
    emission_roughness_txt_info.format = render::texture::TextureFormat::RgbaFloat16;
    emission_roughness_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-emission-roughness", {}, emission_roughness_txt_info, GX_DEFAULT_IGNORED_END_CALLER));

    auto gbuffers_depth_txt_info = position_depth_txt_info;
    gbuffers_depth_txt_info.format = render::texture::TextureFormat::D32;
    gbuffers_depth_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-depth", {}, gbuffers_depth_txt_info, GX_DEFAULT_IGNORED_END_CALLER));

    std::vector<render::texture::Attachment> attachments(GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENTS_COUNT);
    attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_ALBEDO_METALLIC].var = render::texture::Attachment2D { .txt = albedo_metallic_texture };
    attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_POSITION_DEPTH].var = render::texture::Attachment2D { .txt = position_depth_texture };
    attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_NORMAL_AO].var = render::texture::Attachment2D { .txt = normal_ao_texture };
    attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_EMISSION_ROUGHNESS].var = render::texture::Attachment2D { .txt = emission_roughness_texture };
    attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_DEPTH].var = render::texture::Attachment2D { .txt = gbuffers_depth_texture };
    gbuffers_target = std::dynamic_pointer_cast<Target>(e.get_texture_manager()->create_target("gearoenix-gbuffers", std::move(attachments), GX_DEFAULT_IGNORED_END_CALLER));

    GX_LOG_D("GBuffers have been created.");
}

void gearoenix::gl::SubmissionManager::initialise_ssao() noexcept
{
    auto* const txt_mgr = e.get_texture_manager();
    const render::texture::TextureInfo txt_info {
        .format = render::texture::TextureFormat::Float32,
        .sampler_info = render::texture::SamplerInfo {
            .min_filter = render::texture::Filter::Nearest,
            .mag_filter = render::texture::Filter::Nearest,
            .wrap_s = render::texture::Wrap::ClampToEdge,
            .wrap_t = render::texture::Wrap::ClampToEdge,
            .wrap_r = render::texture::Wrap::ClampToEdge,
            .anisotropic_level = 0,
        },
        .width = gbuffer_width,
        .height = gbuffer_height,
        .depth = 0,
        .type = render::texture::Type::Texture2D,
        .has_mipmap = false,
    };
    ssao_resolve_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-ssao-resolve", {}, txt_info, GX_DEFAULT_IGNORED_END_CALLER));

    std::vector<render::texture::Attachment> attachments(1);
    attachments[0].var = render::texture::Attachment2D { .txt = ssao_resolve_texture };
    ssao_resolve_target = std::dynamic_pointer_cast<Target>(e.get_texture_manager()->create_target("gearoenix-ssao", std::move(attachments), GX_DEFAULT_IGNORED_END_CALLER));

    GX_LOG_D("SSAO resolve buffer has been created.");
}

void gearoenix::gl::SubmissionManager::initialise_final() noexcept
{
    auto* const txt_mgr = e.get_texture_manager();
    const render::texture::TextureInfo txt_info {
        .format = render::texture::TextureFormat::RgbaFloat16,
        .sampler_info = render::texture::SamplerInfo {
            .min_filter = render::texture::Filter::Linear,
            .mag_filter = render::texture::Filter::Linear,
            .wrap_s = render::texture::Wrap::ClampToEdge,
            .wrap_t = render::texture::Wrap::ClampToEdge,
            .wrap_r = render::texture::Wrap::ClampToEdge,
            .anisotropic_level = 0,
        },
        .width = gbuffer_width,
        .height = gbuffer_height,
        .depth = 0,
        .type = render::texture::Type::Texture2D,
        .has_mipmap = false,
    };
    final_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-final", {}, txt_info, GX_DEFAULT_IGNORED_END_CALLER));

    std::vector<render::texture::Attachment> attachments(1);
    attachments[0].var = render::texture::Attachment2D { .txt = final_texture };
    final_target = std::dynamic_pointer_cast<Target>(e.get_texture_manager()->create_target("gearoenix-final", std::move(attachments), GX_DEFAULT_IGNORED_END_CALLER));

    GX_LOG_D("Final render target has been created.");
}

void gearoenix::gl::SubmissionManager::fill_scenes() noexcept
{
    e.get_world()->synchronised_system<render::scene::Scene>([&](const core::ecs::Entity::id_t scene_id, render::scene::Scene& scene) {
        if (!scene.enabled)
            return;
        const auto scene_pool_index = scene_pool.emplace([] { return SceneData(); });
        auto& scene_pool_ref = scene_pool[scene_pool_index];
        if (!scenes_bvhs.contains(scene_id))
            scenes_bvhs.emplace(scene_id, physics::accelerator::Bvh<ModelBvhData>());
        scene_pool_ref.skyboxes.clear();
        scene_pool_ref.cameras.clear();
        scene_pool_ref.reflections.clear();
        scene_pool_ref.reflection_cameras.clear();
        scene_pool_ref.shadow_cameras.clear();
        scene_pool_ref.dynamic_models.clear();
        e.get_world()->synchronised_system<render::camera::Camera>([&](const core::ecs::Entity::id_t camera_id, render::camera::Camera& camera) {
            if (!camera.enabled)
                return;
            if (camera.get_scene_id() != scene_id)
                return;
            const auto camera_pool_index = camera_pool.emplace([&] { return CameraData(); });
            switch (camera.get_usage()) {
            case render::camera::Camera::Usage::Main:
                scene_pool_ref.cameras.emplace(std::make_pair(camera.get_layer(), camera_id), camera_pool_index);
                break;
            case render::camera::Camera::Usage::ReflectionProbe:
                scene_pool_ref.reflection_cameras.emplace(camera_id, camera_pool_index);
                break;
            case render::camera::Camera::Usage::Shadow:
                scene_pool_ref.shadow_cameras.emplace(camera_id, camera_pool_index);
                break;
            }
        });
        e.get_world()->synchronised_system<render::skybox::Skybox, Skybox>([&](const core::ecs::Entity::id_t skybox_id, render::skybox::Skybox& render_skybox, Skybox& gl_skybox) {
            if (!render_skybox.enabled)
                return;
            if (render_skybox.get_scene_id() != scene_id)
                return;
            scene_pool_ref.skyboxes.emplace(
                std::make_tuple(render_skybox.get_layer(), skybox_id, render_skybox.is_equirectangular()),
                SkyboxData {
                    .vertex_object = gl_skybox.get_vertex_object(),
                    .albedo_txt = gl_skybox.get_texture_object(),
                });
        });
        if (scene.get_reflection_probs_changed()) {
            scene_pool_ref.default_reflection.second.size = -std::numeric_limits<double>::max();
            scene_pool_ref.default_reflection.second.irradiance = black_cube->get_object();
            scene_pool_ref.default_reflection.second.radiance = black_cube->get_object();
            e.get_world()->synchronised_system<render::reflection::Baked, Reflection>([&](const core::ecs::Entity::id_t reflection_id, render::reflection::Baked& render_baked, Reflection& gl_baked) {
                if (!render_baked.enabled)
                    return;
                if (render_baked.get_scene_id() != scene_id)
                    return;
                const ReflectionData r {
                    .irradiance = gl_baked.get_irradiance_v(),
                    .radiance = gl_baked.get_radiance_v(),
                    .box = render_baked.get_include_box(),
                    .size = render_baked.get_include_box().get_diameter().square_length(),
                };
                scene_pool_ref.reflections.emplace(reflection_id, r);
                if (r.size > scene_pool_ref.default_reflection.second.size) {
                    scene_pool_ref.default_reflection.first = reflection_id;
                    scene_pool_ref.default_reflection.second = r;
                }
            });
        }
        scenes.emplace(std::make_pair(scene.get_layer(), scene_id), scene_pool_index);
    });
}

void gearoenix::gl::SubmissionManager::update_scenes() noexcept
{
    e.get_world()->parallel_system<render::scene::Scene>([&](const core::ecs::Entity::id_t scene_id, render::scene::Scene& render_scene, const unsigned int /*kernel_index*/) noexcept {
        if (!render_scene.enabled)
            return;
        auto& scene_data = scene_pool[scenes[std::make_pair(render_scene.get_layer(), scene_id)]];
        update_scene(scene_id, scene_data, render_scene);
    });
}

void gearoenix::gl::SubmissionManager::update_scene(const core::ecs::Entity::id_t scene_id, SceneData& scene_data, render::scene::Scene& render_scene) noexcept
{
    scene_data.ssao_settings = render_scene.get_ssao_settings();
    auto& bvh = scenes_bvhs[scene_id];
    update_scene_bvh(scene_id, scene_data, render_scene, bvh);
    update_scene_dynamic_models(scene_id, scene_data);
    update_scene_reflection_probes(scene_data, render_scene, bvh);
    update_scene_cameras(scene_id, scene_data, bvh);
}

void gearoenix::gl::SubmissionManager::update_scene_bvh(const core::ecs::Entity::id_t scene_id, SceneData& scene_data, render::scene::Scene& render_scene, physics::accelerator::Bvh<ModelBvhData>& bvh) noexcept
{
    if (!render_scene.get_recreate_bvh())
        return;
    bvh.reset();
    e.get_world()->synchronised_system<physics::collider::Aabb3, render::model::Model, Model, physics::Transformation>(
        [&](const core::ecs::Entity::id_t, physics::collider::Aabb3& collider, render::model::Model& render_model, Model& model, physics::Transformation& model_transform) {
            if (!render_model.enabled || render_model.is_transformable || render_model.scene_id != scene_id)
                return;
            auto& mesh = *model.bound_mesh;
            bvh.add({ collider.get_updated_box(),
                ModelBvhData {
                    .blocked_cameras_flags = render_model.block_cameras_flags,
                    .model = ModelData {
                        .m = math::Mat4x4<float>(model_transform.get_matrix()),
                        .inv_m = math::Mat4x4<float>(model_transform.get_inverted_matrix().transposed()),
                        .albedo_factor = model.material.get_albedo_factor(),
                        .normal_metallic_factor = model.material.get_normal_metallic_factor(),
                        .emission_roughness_factor = model.material.get_emission_roughness_factor(),
                        .alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved = model.material.get_alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved(),
                        .vertex_object = mesh.vertex_object,
                        .vertex_buffer = mesh.vertex_buffer,
                        .index_buffer = mesh.index_buffer,
                        .indices_count = mesh.indices_count,
                        .albedo_txt = model.albedo->get_object(),
                        .normal_txt = model.normal->get_object(),
                        .emission_txt = model.emission->get_object(),
                        .metallic_roughness_txt = model.metallic_roughness->get_object(),
                        .occlusion_txt = model.occlusion->get_object(),
                        // Reflection probe data
                        .irradiance = scene_data.default_reflection.second.irradiance,
                        .radiance = scene_data.default_reflection.second.radiance,
                        .reflection_probe_size = scene_data.default_reflection.second.size,
                    } } });
        });
    bvh.create_nodes();
}

void gearoenix::gl::SubmissionManager::update_scene_dynamic_models(const core::ecs::Entity::id_t scene_id, SceneData& scene_data) noexcept
{
    e.get_world()->synchronised_system<physics::collider::Aabb3, render::model::Model, Model, physics::Transformation>(
        [&](
            const core::ecs::Entity::id_t,
            physics::collider::Aabb3& collider,
            render::model::Model& render_model,
            Model& gl_model,
            physics::Transformation& model_transform) noexcept {
            if (!render_model.enabled || !render_model.is_transformable || render_model.scene_id != scene_id)
                return;
            const auto& mesh = *gl_model.bound_mesh;
            scene_data.dynamic_models.push_back(
                DynamicModelData {
                    .base = ModelBvhData {
                        .blocked_cameras_flags = render_model.block_cameras_flags,
                        .model = ModelData {
                            .m = math::Mat4x4<float>(model_transform.get_matrix()),
                            .inv_m = math::Mat4x4<float>(model_transform.get_inverted_matrix().transposed()),
                            .albedo_factor = gl_model.material.get_albedo_factor(),
                            .normal_metallic_factor = gl_model.material.get_normal_metallic_factor(),
                            .emission_roughness_factor = gl_model.material.get_emission_roughness_factor(),
                            .alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved = gl_model.material.get_alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved(),
                            .vertex_object = mesh.vertex_object,
                            .vertex_buffer = mesh.vertex_buffer,
                            .index_buffer = mesh.index_buffer,
                            .indices_count = mesh.indices_count,
                            .albedo_txt = gl_model.albedo->get_object(),
                            .normal_txt = gl_model.normal->get_object(),
                            .emission_txt = gl_model.emission->get_object(),
                            .metallic_roughness_txt = gl_model.metallic_roughness->get_object(),
                            .occlusion_txt = gl_model.occlusion->get_object(),
                            // Reflection probe data
                            .irradiance = scene_data.default_reflection.second.irradiance,
                            .radiance = scene_data.default_reflection.second.radiance,
                            .reflection_probe_size = scene_data.default_reflection.second.size,
                        },
                    },
                    .box = collider.get_updated_box(),
                });
        });
}

void gearoenix::gl::SubmissionManager::update_scene_reflection_probes(SceneData& scene_data, render::scene::Scene& render_scene, physics::accelerator::Bvh<ModelBvhData>& bvh) noexcept
{
    if (render_scene.get_recreate_bvh() || render_scene.get_reflection_probs_changed()) {
        for (const auto& reflection : scene_data.reflections) {
            bvh.call_on_intersecting(reflection.second.box, [&](std::remove_reference_t<decltype(bvh)>::Data& bvh_node_data) noexcept {
                auto& m = bvh_node_data.user_data.model;
                if (reflection.second.size > m.reflection_probe_size)
                    return;
                m.irradiance = reflection.second.irradiance;
                m.radiance = reflection.second.radiance;
                m.reflection_probe_size = reflection.second.size;
            });
        }
    }

    core::sync::ParallelFor::exec(scene_data.dynamic_models.begin(), scene_data.dynamic_models.end(), [&](DynamicModelData& m, const unsigned int) noexcept {
        for (const auto& reflection : scene_data.reflections) {
            if (!reflection.second.box.check_intersection(m.box))
                return;
            auto& mm = m.base.model;
            if (reflection.second.size > mm.reflection_probe_size)
                return;
            mm.irradiance = reflection.second.irradiance;
            mm.radiance = reflection.second.radiance;
            mm.reflection_probe_size = reflection.second.size;
        }
    });
}

void gearoenix::gl::SubmissionManager::update_scene_cameras(const core::ecs::Entity::id_t scene_id, SceneData& scene_data, physics::accelerator::Bvh<ModelBvhData>& bvh) noexcept
{
    e.get_world()->parallel_system<render::camera::Camera, Camera, physics::collider::Frustum, physics::Transformation>(
        [&](
            const core::ecs::Entity::id_t camera_id,
            render::camera::Camera& render_camera,
            Camera& gl_camera,
            physics::collider::Frustum& frustum,
            physics::Transformation& transform,
            const unsigned int /*kernel_index*/) noexcept {
            if (!render_camera.enabled || scene_id != render_camera.get_scene_id())
                return;
            const auto camera_location = transform.get_location();
            std::size_t camera_pool_index = static_cast<std::size_t>(-1);
            switch (render_camera.get_usage()) {
            case render::camera::Camera::Usage::Main:
                camera_pool_index = scene_data.cameras[std::make_pair(render_camera.get_layer(), camera_id)];
                break;
            case render::camera::Camera::Usage::ReflectionProbe:
                camera_pool_index = scene_data.reflection_cameras[camera_id];
                break;
            case render::camera::Camera::Usage::Shadow:
                camera_pool_index = scene_data.shadow_cameras[camera_id];
                break;
            }
            auto& camera_data = camera_pool[camera_pool_index];
            math::Vec2<std::size_t> target_dimension;
            if (nullptr != gl_camera.target) {
                camera_data.framebuffer = gl_camera.target->get_framebuffer();
                target_dimension = render_camera.get_target()->get_dimension();
            } else {
                camera_data.framebuffer = gbuffers_target->get_framebuffer();
                target_dimension = gbuffers_target->get_dimension();
            }
            camera_data.viewport_clip = math::Vec4<sizei>(render_camera.get_starting_clip_ending_clip() * math::Vec4<float>(target_dimension, target_dimension));
            camera_data.vp = render_camera.get_view_projection();
            camera_data.pos = math::Vec3<float>(camera_location);
            camera_data.skybox_scale = render_camera.get_far() / 1.732051f;
            camera_data.opaque_models_data.clear();
            camera_data.tranclucent_models_data.clear();
            camera_data.out_reference = render_camera.get_reference_id();

            for (auto& v : camera_data.threads_opaque_models_data)
                v.clear();
            for (auto& v : camera_data.threads_tranclucent_models_data)
                v.clear();

            // Recoding static models
            bvh.call_on_intersecting(frustum, [&](const std::remove_reference_t<decltype(bvh)>::Data& bvh_node_data) {
                if ((bvh_node_data.user_data.blocked_cameras_flags & render_camera.get_flag()) == 0)
                    return;
                const auto dir = camera_location - bvh_node_data.box.get_center();
                const auto dis = dir.square_length();
                camera_data.opaque_models_data.emplace_back(dis, bvh_node_data.user_data.model);
                // TODO opaque/translucent in ModelBvhData
            });

            // Recording dynamic models
            core::sync::ParallelFor::exec(scene_data.dynamic_models.begin(), scene_data.dynamic_models.end(), [&](DynamicModelData& m, const unsigned int kernel_index) noexcept {
                if ((m.base.blocked_cameras_flags & render_camera.get_flag()) == 0)
                    return;
                if (!frustum.check_intersection(m.box))
                    return;
                const auto dir = camera_location - m.box.get_center();
                const auto dis = dir.square_length();
                camera_data.threads_opaque_models_data[kernel_index].emplace_back(dis, m.base.model);
                // TODO opaque/translucent in ModelBvhData
            });

            for (auto& v : camera_data.threads_opaque_models_data)
                std::move(v.begin(), v.end(), std::back_inserter(camera_data.opaque_models_data));

            for (auto& v : camera_data.threads_tranclucent_models_data)
                std::move(v.begin(), v.end(), std::back_inserter(camera_data.tranclucent_models_data));

            std::sort(
                GX_ALGORITHM_EXECUTION
                    camera_data.opaque_models_data.begin(),
                camera_data.opaque_models_data.end(),
                [](const auto& rhs, const auto& lhs) { return rhs.first < lhs.first; });
            std::sort(
                GX_ALGORITHM_EXECUTION
                    camera_data.tranclucent_models_data.begin(),
                camera_data.tranclucent_models_data.end(),
                [](const auto& rhs, const auto& lhs) { return rhs.first > lhs.first; });
        });
}

void gearoenix::gl::SubmissionManager::fill_gbuffers(const std::size_t camera_pool_index) noexcept
{
}

void gearoenix::gl::SubmissionManager::render_shadows() noexcept
{
}

void gearoenix::gl::SubmissionManager::render_reflection_probes() noexcept
{
    e.get_world()->synchronised_system<Reflection, ReflectionRuntime, render::reflection::Runtime>([&](const core::ecs::Entity::id_t, Reflection& r, ReflectionRuntime& rr, render::reflection::Runtime& rrr) noexcept {
        constexpr std::array<std::array<math::Vec3<float>, 3>, 6> face_uv_axis {
            std::array<math::Vec3<float>, 3> { math::Vec3(0.0f, 0.0f, -1.0f), math::Vec3(0.0f, -1.0f, 0.0f), math::Vec3(1.0f, 0.0f, 0.0f) }, // PositiveX
            std::array<math::Vec3<float>, 3> { math::Vec3(0.0f, 0.0f, 1.0f), math::Vec3(0.0f, -1.0f, 0.0f), math::Vec3(-1.0f, 0.0f, 0.0f) }, // NegativeX
            std::array<math::Vec3<float>, 3> { math::Vec3(1.0f, 0.0f, 0.0f), math::Vec3(0.0f, 0.0f, 1.0f), math::Vec3(0.0f, 1.0f, 0.0f) }, // PositiveY
            std::array<math::Vec3<float>, 3> { math::Vec3(1.0f, 0.0f, 0.0f), math::Vec3(0.0f, 0.0f, -1.0f), math::Vec3(0.0f, -1.0f, 0.0f) }, // NegativeY
            std::array<math::Vec3<float>, 3> { math::Vec3(1.0f, 0.0f, 0.0f), math::Vec3(0.0f, -1.0f, 0.0f), math::Vec3(0.0f, 0.0f, 1.0f) }, // PositiveZ
            std::array<math::Vec3<float>, 3> { math::Vec3(-1.0f, 0.0f, 0.0f), math::Vec3(0.0f, -1.0f, 0.0f), math::Vec3(0.0f, 0.0f, -1.0f) }, // NegativeZ
        };
        switch (rrr.get_state()) {
        case render::reflection::Runtime::State::EnvironmentCubeMipMap:
            glBindTexture(GL_TEXTURE_CUBE_MAP, rr.get_environment_v());
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            return;
        case render::reflection::Runtime::State::IrradianceFace: {
            const auto fi = rrr.get_state_irradiance_face();
            auto& target = *rr.get_irradiance_targets()[fi];
            target.bind();
            const auto w = static_cast<sizei>(rrr.get_irradiance()->get_info().width);
            set_viewport_clip({ 0, 0, w, w });
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            irradiance_shader->bind();
            glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(irradiance_shader->get_environment_index()));
            glBindTexture(GL_TEXTURE_CUBE_MAP, rr.get_environment_v());
            irradiance_shader->set_m_data(reinterpret_cast<const float*>(&face_uv_axis[fi]));
            glBindVertexArray(screen_vertex_object);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            return;
        }
        case render::reflection::Runtime::State::IrradianceMipMap:
            glBindTexture(GL_TEXTURE_CUBE_MAP, r.get_irradiance_v());
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            return;
        case render::reflection::Runtime::State::RadianceFaceLevel: {
            const auto fi = rrr.get_state_radiance_face();
            const auto li = rrr.get_state_radiance_level();
            auto& target = *rr.get_radiance_targets()[fi][li];
            target.bind();
            const auto w = static_cast<sizei>(rrr.get_radiance()->get_info().width >> li);
            set_viewport_clip({ 0, 0, w, w });
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            radiance_shader->bind();
            glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(radiance_shader->get_environment_index()));
            glBindTexture(GL_TEXTURE_CUBE_MAP, rr.get_environment_v());
            radiance_shader->set_m_data(reinterpret_cast<const float*>(&face_uv_axis[fi]));
            const float roughness = rrr.get_roughnesses()[li];
            radiance_shader->set_roughness_data(reinterpret_cast<const float*>(&roughness));
            const float roughness_p_4 = roughness * roughness * roughness * roughness;
            radiance_shader->set_roughness_p_4_data(reinterpret_cast<const float*>(&roughness_p_4));
            const float resolution = static_cast<float>(rrr.get_environment()->get_info().width);
            const float sa_texel = (GX_PI / 1.5f) / (resolution * resolution);
            radiance_shader->set_sa_texel_data(reinterpret_cast<const float*>(&sa_texel));
            glBindVertexArray(screen_vertex_object);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            return;
        }
        }
    });
}

void gearoenix::gl::SubmissionManager::render_reflection_probes(SceneData& scene) noexcept
{
    const auto default_irradiance = black_cube->get_object();
    const auto default_radiance = black_cube->get_object();
    for (auto& camera_pool_index : scene.reflection_cameras) {
        auto& camera = camera_pool[camera_pool_index.second];
        auto& reflection = scene.reflections[camera.out_reference];

        if (current_bound_framebuffer != camera.framebuffer) {
            current_bound_framebuffer = camera.framebuffer;
            glBindFramebuffer(GL_FRAMEBUFFER, camera.framebuffer);
        }

        set_viewport_clip(camera.viewport_clip);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        // Rendering skyboxes
        skybox_equirectangular_shader->bind();
        skybox_equirectangular_shader->set_vp_data(reinterpret_cast<const float*>(&camera.vp));
        const auto camera_pos_scale = math::Vec4(camera.pos, camera.skybox_scale);
        skybox_equirectangular_shader->set_camera_position_box_scale_data(reinterpret_cast<const float*>(&camera_pos_scale));
        for (const auto& key_skybox : scene.skyboxes) {
            const auto& skybox = key_skybox.second;
            glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(skybox_equirectangular_shader->get_albedo_index()));
            glBindTexture(GL_TEXTURE_2D, skybox.albedo_txt);
            glBindVertexArray(skybox.vertex_object);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }
        // Rendering forward pbr
        forward_pbr_shader->bind();
        const auto forward_pbr_txt_index_albedo = static_cast<enumerated>(forward_pbr_shader->get_albedo_index());
        const auto forward_pbr_txt_index_normal = static_cast<enumerated>(forward_pbr_shader->get_normal_index());
        const auto forward_pbr_txt_index_emission = static_cast<enumerated>(forward_pbr_shader->get_emission_index());
        const auto forward_pbr_txt_index_metallic_roughness = static_cast<enumerated>(forward_pbr_shader->get_metallic_roughness_index());
        const auto forward_pbr_txt_index_occlusion = static_cast<enumerated>(forward_pbr_shader->get_occlusion_index());
        const auto forward_pbr_txt_index_irradiance = static_cast<enumerated>(forward_pbr_shader->get_irradiance_index());
        const auto forward_pbr_txt_index_radiance = static_cast<enumerated>(forward_pbr_shader->get_irradiance_index());
        forward_pbr_shader->set_vp_data(reinterpret_cast<const float*>(&camera.vp));
        for (auto& distance_model_data : camera.opaque_models_data) {
            auto& model_data = distance_model_data.second;
            forward_pbr_shader->set_m_data(reinterpret_cast<const float*>(&model_data.m));
            forward_pbr_shader->set_inv_m_data(reinterpret_cast<const float*>(&model_data.inv_m));
            forward_pbr_shader->set_albedo_factor_data(reinterpret_cast<const float*>(&model_data.albedo_factor));
            forward_pbr_shader->set_normal_metallic_factor_data(reinterpret_cast<const float*>(&model_data.normal_metallic_factor));
            forward_pbr_shader->set_emission_roughness_factor_data(reinterpret_cast<const float*>(&model_data.emission_roughness_factor));
            forward_pbr_shader->set_alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved_data(reinterpret_cast<const float*>(&model_data.alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved));

            glActiveTexture(GL_TEXTURE0 + forward_pbr_txt_index_albedo);
            glBindTexture(GL_TEXTURE_2D, model_data.albedo_txt);
            glActiveTexture(GL_TEXTURE0 + forward_pbr_txt_index_normal);
            glBindTexture(GL_TEXTURE_2D, model_data.normal_txt);
            glActiveTexture(GL_TEXTURE0 + forward_pbr_txt_index_emission);
            glBindTexture(GL_TEXTURE_2D, model_data.emission_txt);
            glActiveTexture(GL_TEXTURE0 + forward_pbr_txt_index_metallic_roughness);
            glBindTexture(GL_TEXTURE_2D, model_data.metallic_roughness_txt);
            glActiveTexture(GL_TEXTURE0 + forward_pbr_txt_index_occlusion);
            glBindTexture(GL_TEXTURE_2D, model_data.occlusion_txt);
            glActiveTexture(GL_TEXTURE0 + forward_pbr_txt_index_irradiance);
            if (model_data.irradiance == reflection.irradiance)
                glBindTexture(GL_TEXTURE_CUBE_MAP, default_irradiance);
            else
                glBindTexture(GL_TEXTURE_CUBE_MAP, model_data.irradiance);
            glActiveTexture(GL_TEXTURE0 + forward_pbr_txt_index_radiance);
            if (model_data.radiance == reflection.radiance)
                glBindTexture(GL_TEXTURE_CUBE_MAP, default_radiance);
            else
                glBindTexture(GL_TEXTURE_CUBE_MAP, model_data.radiance);

            glBindVertexArray(model_data.vertex_object);
            glDrawElements(GL_TRIANGLES, model_data.indices_count, GL_UNSIGNED_INT, nullptr);
        }
    }
}

gearoenix::gl::SubmissionManager::SubmissionManager(Engine& e) noexcept
    : e(e)
    , final_shader(new ShaderFinal(e))
    , forward_pbr_shader(new ShaderForwardPbr(e))
    , gbuffers_filler_shader(new ShaderGBuffersFiller(e))
    , deferred_pbr_shader(new ShaderDeferredPbr(e))
    , deferred_pbr_transparent_shader(new ShaderDeferredPbrTransparent(e))
    , irradiance_shader(new ShaderIrradiance(e))
    , radiance_shader(new ShaderRadiance(e))
    , skybox_equirectangular_shader(new ShaderSkyboxEquirectangular(e))
    , ssao_resolve_shader(new ShaderSsaoResolve(e))
{
    GX_LOG_D("Creating submission manager.");
    initialise_gbuffers();
    initialise_ssao();
    initialise_final();

    black_cube = std::dynamic_pointer_cast<TextureCube>(e.get_texture_manager()->create_cube_from_colour(math::Vec4(0.0f)));

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // Pipeline settings
    // glEnable(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_STENCIL_TEST);

    const float screen_vertices[] = {
        -1.0f, 3.0f, // 1
        -1.0f, -1.0f, // 2
        3.0f, -1.0f, // 3
    };
    glGenVertexArrays(1, &screen_vertex_object);
    glBindVertexArray(screen_vertex_object);
    glGenBuffers(1, &screen_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, screen_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screen_vertices), screen_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    e.todos.unload();
}

gearoenix::gl::SubmissionManager::~SubmissionManager() noexcept = default;

void gearoenix::gl::SubmissionManager::update() noexcept
{
    auto* const world = e.get_world();

    camera_pool.clear();
    scene_pool.clear();
    scenes.clear();

    fill_scenes();
    update_scenes();

    auto& os_app = e.get_platform_application();
    const auto& base_os_app = os_app.get_base();
    const float screen_uv_move_reserved[] = { gbuffer_uv_move_x, gbuffer_uv_move_y, 0.0f, 0.0f };
    const auto* const gbuffers_attachments = gbuffers_target->get_gl_attachments().data();
    const auto* const ssao_resolved_attachments = ssao_resolve_target->get_gl_attachments().data();
    const auto* const final_attachments = final_target->get_gl_attachments().data();
    for (auto& scene_layer_entity_id_pool_index : scenes) {
        auto& scene = scene_pool[scene_layer_entity_id_pool_index.second];

        gbuffers_filler_shader->bind();
        const auto gbuffers_filler_txt_index_albedo = static_cast<enumerated>(gbuffers_filler_shader->get_albedo_index());
        const auto gbuffers_filler_txt_index_normal = static_cast<enumerated>(gbuffers_filler_shader->get_normal_index());
        const auto gbuffers_filler_txt_index_emission = static_cast<enumerated>(gbuffers_filler_shader->get_emission_index());
        const auto gbuffers_filler_txt_index_metallic_roughness = static_cast<enumerated>(gbuffers_filler_shader->get_metallic_roughness_index());
        const auto gbuffers_filler_txt_index_occlusion = static_cast<enumerated>(gbuffers_filler_shader->get_occlusion_index());
        const auto gbuffers_filler_txt_index_irradiance = static_cast<enumerated>(gbuffers_filler_shader->get_irradiance_index());
        const auto gbuffers_filler_txt_index_radiance = static_cast<enumerated>(gbuffers_filler_shader->get_radiance_index());

        glDisable(GL_BLEND); // TODO find the best place for it
        gbuffers_target->bind();
        for (auto& camera_layer_entity_id_pool_index : scene.cameras) {
            auto& camera = camera_pool[camera_layer_entity_id_pool_index.second];

            set_viewport_clip(camera.viewport_clip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            gbuffers_filler_shader->set_vp_data(reinterpret_cast<const float*>(&camera.vp));
            for (auto& distance_model_data : camera.opaque_models_data) {
                auto& model_data = distance_model_data.second;
                gbuffers_filler_shader->set_m_data(reinterpret_cast<const float*>(&model_data.m));
                gbuffers_filler_shader->set_inv_m_data(reinterpret_cast<const float*>(&model_data.inv_m));
                gbuffers_filler_shader->set_albedo_factor_data(reinterpret_cast<const float*>(&model_data.albedo_factor));
                gbuffers_filler_shader->set_normal_metallic_factor_data(reinterpret_cast<const float*>(&model_data.normal_metallic_factor));
                gbuffers_filler_shader->set_emission_roughness_factor_data(reinterpret_cast<const float*>(&model_data.emission_roughness_factor));
                gbuffers_filler_shader->set_alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved_data(reinterpret_cast<const float*>(&model_data.alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved));
                glActiveTexture(GL_TEXTURE0 + gbuffers_filler_txt_index_albedo);
                glBindTexture(GL_TEXTURE_2D, model_data.albedo_txt);
                glActiveTexture(GL_TEXTURE0 + gbuffers_filler_txt_index_normal);
                glBindTexture(GL_TEXTURE_2D, model_data.normal_txt);
                glActiveTexture(GL_TEXTURE0 + gbuffers_filler_txt_index_emission);
                glBindTexture(GL_TEXTURE_2D, model_data.emission_txt);
                glActiveTexture(GL_TEXTURE0 + gbuffers_filler_txt_index_metallic_roughness);
                glBindTexture(GL_TEXTURE_2D, model_data.metallic_roughness_txt);
                glActiveTexture(GL_TEXTURE0 + gbuffers_filler_txt_index_occlusion);
                glBindTexture(GL_TEXTURE_2D, model_data.occlusion_txt);
                glActiveTexture(GL_TEXTURE0 + gbuffers_filler_txt_index_irradiance);
                glBindTexture(GL_TEXTURE_CUBE_MAP, model_data.irradiance);
                glActiveTexture(GL_TEXTURE0 + gbuffers_filler_txt_index_radiance);
                glBindTexture(GL_TEXTURE_CUBE_MAP, model_data.radiance);
                glBindVertexArray(model_data.vertex_object);
                glDrawElements(GL_TRIANGLES, model_data.indices_count, GL_UNSIGNED_INT, nullptr);
            }
        }
    }

    // render_shadows();
    render_reflection_probes();

    for (auto& scene_layer_entity_id_pool_index : scenes) {
        auto& scene = scene_pool[scene_layer_entity_id_pool_index.second];
        render_reflection_probes(scene);

        // SSAO resolving -------------------------------------------------------------------------------------
        ssao_resolve_target->bind();

        set_viewport_clip(gbuffer_viewport_clip);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        ssao_resolve_shader->bind();

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(ssao_resolve_shader->get_position_depth_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_POSITION_DEPTH].texture_object);

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(ssao_resolve_shader->get_normal_ao_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_NORMAL_AO].texture_object);

        ssao_resolve_shader->set_ssao_radius_move_start_end_data(reinterpret_cast<const float*>(&scene.ssao_settings));
        for (auto& camera_layer_entity_id_pool_index : scene.cameras) {
            auto& camera = camera_pool[camera_layer_entity_id_pool_index.second];
            ssao_resolve_shader->set_vp_data(reinterpret_cast<const float*>(&camera.vp));
            glBindVertexArray(screen_vertex_object);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        // PBR ------------------------------------------------------------------------------------------------
        final_target->bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        skybox_equirectangular_shader->bind();

        for (auto& camera_layer_entity_id_pool_index : scene.cameras) {
            auto& camera = camera_pool[camera_layer_entity_id_pool_index.second];
            skybox_equirectangular_shader->set_vp_data(reinterpret_cast<const float*>(&camera.vp));
            const auto camera_pos_scale = math::Vec4(camera.pos, camera.skybox_scale);
            skybox_equirectangular_shader->set_camera_position_box_scale_data(reinterpret_cast<const float*>(&camera_pos_scale));
            for (const auto& key_skybox : scene.skyboxes) {
                const auto& skybox = key_skybox.second;
                glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(skybox_equirectangular_shader->get_albedo_index()));
                glBindTexture(GL_TEXTURE_2D, skybox.albedo_txt);
                glBindVertexArray(skybox.vertex_object);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
        }

        deferred_pbr_shader->bind();

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(deferred_pbr_shader->get_position_depth_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_POSITION_DEPTH].texture_object);

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(deferred_pbr_shader->get_albedo_metallic_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_ALBEDO_METALLIC].texture_object);

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(deferred_pbr_shader->get_normal_ao_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_NORMAL_AO].texture_object);

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(deferred_pbr_shader->get_emission_roughness_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[GEAROENIX_GL_GBUFFER_FRAMEBUFFER_ATTACHMENT_INDEX_EMISSION_ROUGHNESS].texture_object);

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(deferred_pbr_shader->get_ssao_resolved_index()));
        glBindTexture(GL_TEXTURE_2D, ssao_resolved_attachments[0].texture_object);

        deferred_pbr_shader->set_screen_uv_move_reserved_data(screen_uv_move_reserved);

        glBindVertexArray(screen_vertex_object);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Final ----------------------------------------------------------------------------------------------
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        const float screen_ratio = static_cast<float>(base_os_app.get_window_size().x) / static_cast<float>(base_os_app.get_window_size().y);
        if (screen_ratio < gbuffer_aspect_ratio) {
            const auto screen_height = static_cast<sizei>(static_cast<float>(base_os_app.get_window_size().x) / gbuffer_aspect_ratio + 0.1f);
            const auto screen_y = (static_cast<sizei>(base_os_app.get_window_size().y) - screen_height) / 2;
            set_viewport_clip({ static_cast<sizei>(0), screen_y, static_cast<sizei>(base_os_app.get_window_size().x), screen_height });
        } else {
            const auto screen_width = static_cast<sizei>(static_cast<float>(base_os_app.get_window_size().y) * gbuffer_aspect_ratio + 0.1f);
            const auto screen_x = (static_cast<sizei>(base_os_app.get_window_size().x) - screen_width) / 2;
            set_viewport_clip({ screen_x, static_cast<sizei>(0), screen_width, static_cast<sizei>(base_os_app.get_window_size().y) });
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        final_shader->bind();

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(final_shader->get_albedo_index()));
        glBindTexture(GL_TEXTURE_2D, final_attachments[0].texture_object);

        glBindVertexArray(screen_vertex_object);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    GX_GL_CHECK_D;

    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    set_viewport_clip({ 0, 0, static_cast<sizei>(io.DisplaySize.x), static_cast<sizei>(io.DisplaySize.y) });
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glGetError();

#ifdef GX_PLATFORM_INTERFACE_SDL2
    SDL_GL_SwapWindow(os_app.get_window());
#elif defined(GX_PLATFORM_INTERFACE_ANDROID)
    os_app.get_gl_context()->swap();
#endif
}

void gearoenix::gl::SubmissionManager::set_viewport_clip(const math::Vec4<sizei>& viewport_clip) noexcept
{
    if (current_viewport_clip == viewport_clip)
        return;
    current_viewport_clip = viewport_clip;
    glViewport(
        current_viewport_clip.x, current_viewport_clip.y,
        current_viewport_clip.z, current_viewport_clip.w);
    glScissor(
        current_viewport_clip.x, current_viewport_clip.y,
        current_viewport_clip.z, current_viewport_clip.w);
}

#endif
