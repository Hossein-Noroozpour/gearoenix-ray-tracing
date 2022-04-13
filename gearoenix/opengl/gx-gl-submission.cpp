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
#include "../render/scene/gx-rnd-scn-scene.hpp"
#include "gx-gl-camera.hpp"
#include "gx-gl-check.hpp"
#include "gx-gl-constants.hpp"
#include "gx-gl-engine.hpp"
#include "gx-gl-final-effects-shader.hpp"
#include "gx-gl-gbuffers-filler-shader.hpp"
#include "gx-gl-loader.hpp"
#include "gx-gl-mesh.hpp"
#include "gx-gl-model.hpp"
#include "gx-gl-ssao-resolve-shader.hpp"
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
    const render::texture::TextureInfo txt_info {
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
    const core::sync::EndCallerIgnored txt_end([this] {
        gbuffers_target = std::make_unique<Target>(std::vector<Target::Attachment> {
            Target::Attachment { .texture = position_depth_texture },
            Target::Attachment { .texture = albedo_metallic_texture },
            Target::Attachment { .texture = normal_roughness_texture },
            Target::Attachment { .texture = emission_ambient_occlusion_texture },
        });
    });
    position_depth_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-position-depth", {}, txt_info, txt_end));
    albedo_metallic_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-albedo-metallic", {}, txt_info, txt_end));
    normal_roughness_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-normal-roughness", {}, txt_info, txt_end));
    emission_ambient_occlusion_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-gbuffer-emission-ambient-occlusion", {}, txt_info, txt_end));

    e.todos.unload();
}

void gearoenix::gl::SubmissionManager::initialise_ssao() noexcept
{
    auto* const txt_mgr = e.get_texture_manager();
    const render::texture::TextureInfo txt_info {
        .format = render::texture::TextureFormat::Float32,
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
    const core::sync::EndCallerIgnored txt_end([this] {
        ssao_resolve_target = std::make_unique<Target>(std::vector<Target::Attachment> {
            Target::Attachment { .texture = ssao_resolve_texture },
        });
    });
    ssao_resolve_texture = std::dynamic_pointer_cast<Texture2D>(txt_mgr->create_2d_from_pixels(
        "gearoenix-opengl-texture-ssao-resolve", {}, txt_info, txt_end));

    e.todos.unload();
}

gearoenix::gl::SubmissionManager::SubmissionManager(Engine& e) noexcept
    : e(e)
    , gbuffers_filler_shader(new GBuffersFillerShader(e))
    , ssao_resolve_shader(new SsaoResolveShader(e))
    , final_effects_shader(new FinalEffectsShader(e))
{
    initialise_gbuffers();
    initialise_ssao();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // Pipeline settings
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
}

gearoenix::gl::SubmissionManager::~SubmissionManager() noexcept = default;

void gearoenix::gl::SubmissionManager::update() noexcept
{
    auto* const world = e.get_world();

    camera_pool.clear();
    scene_pool.clear();
    scenes.clear();

    world->synchronised_system<render::scene::Scene>([&](const core::ecs::Entity::id_t scene_id, render::scene::Scene& scene) {
        if (!scene.enabled)
            return;
        const auto scene_pool_index = scene_pool.emplace([] { return SceneData(); });
        auto& scene_pool_ref = scene_pool[scene_pool_index];
        if (!scenes_bvhs.contains(scene_id))
            scenes_bvhs.emplace(scene_id, physics::accelerator::Bvh<ModelBvhData>());
        scene_pool_ref.cameras.clear();
        world->synchronised_system<render::camera::Camera>([&](const core::ecs::Entity::id_t camera_id, render::camera::Camera& camera) {
            if (!camera.enabled)
                return;
            if (camera.get_scene_id() != scene_id)
                return;
            const auto camera_pool_index = camera_pool.emplace([&] { return CameraData(); });
            scene_pool_ref.cameras.emplace(
                std::make_pair(camera.get_layer(), camera_id),
                camera_pool_index);
        });
        scenes.emplace(std::make_pair(scene.get_layer(), scene_id), scene_pool_index);
    });

    world->parallel_system<render::scene::Scene>([&](const core::ecs::Entity::id_t scene_id, render::scene::Scene& scene, const auto /*kernel_index*/) {
        if (!scene.enabled)
            return;
        auto& scene_data = scene_pool[scenes[std::make_pair(scene.get_layer(), scene_id)]];
        scene_data.ssao_settings = scene.get_ssao_settings();
        auto& bvh = scenes_bvhs[scene_id];
        if (scene.get_recreate_bvh()) {
            bvh.reset();
            world->synchronised_system<physics::collider::Aabb3, render::model::Model, Model, physics::Transformation>(
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
                            } } });
                });
            bvh.create_nodes();
        }
        world->parallel_system<render::camera::Camera, physics::collider::Frustum, physics::Transformation>(
            [&](
                const core::ecs::Entity::id_t camera_id,
                render::camera::Camera& render_camera,
                physics::collider::Frustum& frustum,
                physics::Transformation& transform,
                const auto /*kernel_index*/) noexcept {
                if (!render_camera.get_is_enabled() || scene_id != render_camera.get_scene_id())
                    return;
                const auto camera_location = transform.get_location();
                auto& camera_data = camera_pool[scene_data.cameras[std::make_pair(render_camera.get_layer(), camera_id)]];
                camera_data.vp = render_camera.get_view_projection();
                camera_data.pos = math::Vec3<float>(camera_location);
                camera_data.opaque_models_data.clear();
                camera_data.tranclucent_models_data.clear();
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
                world->parallel_system<physics::collider::Aabb3, render::model::Model, Model, physics::Transformation>(
                    [&](
                        const core::ecs::Entity::id_t,
                        physics::collider::Aabb3& collider,
                        render::model::Model& render_model,
                        Model& gl_model,
                        physics::Transformation& model_transform,
                        const auto kernel_index) noexcept {
                        if (!render_model.enabled || !render_model.is_transformable || render_model.scene_id != scene_id)
                            return;
                        const auto& box = collider.get_updated_box();
                        if (!frustum.check_intersection(box))
                            return;
                        const auto dir = camera_location - box.get_center();
                        const auto dis = dir.square_length();
                        const auto& mesh = *gl_model.bound_mesh;
                        camera_data.threads_opaque_models_data[kernel_index].emplace_back(
                            dis,
                            ModelData {
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
                            });
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
    });

    auto& os_app = e.get_platform_application();
    const auto& base_os_app = os_app.get_base();
    const float screen_uv_move_reserved[] = { gbuffer_uv_move_x, gbuffer_uv_move_y, 0.0f, 0.0f };
    GX_GL_CHECK_D;
    for (auto& scene_layer_entity_id_pool_index : scenes) {
        auto& scene = scene_pool[scene_layer_entity_id_pool_index.second];
        // Filling GBuffers -------------------------------------------------------------------------------------------------
        gbuffers_target->bind();
        glViewport(
            static_cast<sizei>(0), static_cast<sizei>(0),
            static_cast<sizei>(gbuffer_width), static_cast<sizei>(gbuffer_height));
        glScissor(
            static_cast<sizei>(0), static_cast<sizei>(0),
            static_cast<sizei>(gbuffer_width), static_cast<sizei>(gbuffer_height));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        gbuffers_filler_shader->bind();
        GX_GL_CHECK_D;
        const auto gbuffers_filler_txt_index_albedo = static_cast<enumerated>(gbuffers_filler_shader->get_albedo_index());
        const auto gbuffers_filler_txt_index_normal = static_cast<enumerated>(gbuffers_filler_shader->get_normal_index());
        const auto gbuffers_filler_txt_index_emission = static_cast<enumerated>(gbuffers_filler_shader->get_emission_index());
        const auto gbuffers_filler_txt_index_metallic_roughness = static_cast<enumerated>(gbuffers_filler_shader->get_metallic_roughness_index());
        const auto gbuffers_filler_txt_index_occlusion = static_cast<enumerated>(gbuffers_filler_shader->get_occlusion_index());

        for (auto& camera_layer_entity_id_pool_index : scene.cameras) {
            GX_GL_CHECK_D;
            auto& camera = camera_pool[camera_layer_entity_id_pool_index.second];
            gbuffers_filler_shader->set_vp_data(reinterpret_cast<const float*>(&camera.vp));
            gbuffers_filler_shader->set_camera_position_data(reinterpret_cast<const float*>(&camera.pos));
            GX_GL_CHECK_D;
            for (auto& distance_model_data : camera.opaque_models_data) {
                GX_GL_CHECK_D;
                auto& model_data = distance_model_data.second;
                gbuffers_filler_shader->set_m_data(reinterpret_cast<const float*>(&model_data.m));
                gbuffers_filler_shader->set_inv_m_data(reinterpret_cast<const float*>(&model_data.inv_m));
                gbuffers_filler_shader->set_albedo_factor_data(reinterpret_cast<const float*>(&model_data.albedo_factor));
                gbuffers_filler_shader->set_normal_metallic_factor_data(reinterpret_cast<const float*>(&model_data.normal_metallic_factor));
                gbuffers_filler_shader->set_emission_roughness_factor_data(reinterpret_cast<const float*>(&model_data.emission_roughness_factor));
                gbuffers_filler_shader->set_alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved_data(reinterpret_cast<const float*>(&model_data.alpha_cutoff_occlusion_strength_radiance_lod_coefficient_reserved));
                GX_GL_CHECK_D;
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
                GX_GL_CHECK_D;
                glBindVertexArray(model_data.vertex_object);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model_data.index_buffer);
                glDrawElements(GL_TRIANGLES, model_data.indices_count, GL_UNSIGNED_INT, nullptr);
                GX_GL_CHECK_D;
            }
        }
        // SSAO resolving ------------------------------------------------------------------------------------------
        ssao_resolve_target->bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        ssao_resolve_shader->bind();
        const auto* const gbuffers_attachments = gbuffers_target->get_attachments().data();
        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(ssao_resolve_shader->get_position_depth_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[0].texture->get_object());
        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(ssao_resolve_shader->get_normal_roughness_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[2].texture->get_object());
        ssao_resolve_shader->set_ssao_radius_move_start_end_data(reinterpret_cast<const float*>(&scene.ssao_settings));
        for (auto& camera_layer_entity_id_pool_index : scene.cameras) {
            GX_GL_CHECK_D;
            auto& camera = camera_pool[camera_layer_entity_id_pool_index.second];
            ssao_resolve_shader->set_vp_data(reinterpret_cast<const float*>(&camera.vp));
            glBindVertexArray(screen_vertex_object);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            GX_GL_CHECK_D;
        }
        // Final effects ----------------------------------------------------------------------------------------------
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        const float screen_ratio = static_cast<float>(base_os_app.get_window_size().x) / static_cast<float>(base_os_app.get_window_size().y);
        if (screen_ratio < gbuffer_aspect_ratio) {
            const auto screen_height = static_cast<sizei>(static_cast<float>(base_os_app.get_window_size().x) / gbuffer_aspect_ratio + 0.1f);
            const auto screen_y = (static_cast<sizei>(base_os_app.get_window_size().y) - screen_height) / 2;
            glViewport(
                static_cast<sizei>(0), screen_y,
                static_cast<sizei>(base_os_app.get_window_size().x), screen_height);
            glScissor(
                static_cast<sizei>(0), screen_y,
                static_cast<sizei>(base_os_app.get_window_size().x), screen_height);
        } else {
            const auto screen_width = static_cast<sizei>(static_cast<float>(base_os_app.get_window_size().y) * gbuffer_aspect_ratio + 0.1f);
            const auto screen_x = (static_cast<sizei>(base_os_app.get_window_size().x) - screen_width) / 2;
            glViewport(
                screen_x, static_cast<sizei>(0),
                screen_width, static_cast<sizei>(base_os_app.get_window_size().y));
            glScissor(
                screen_x, static_cast<sizei>(0),
                screen_width, static_cast<sizei>(base_os_app.get_window_size().y));
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        final_effects_shader->bind();

        const auto* const ssao_resolved_attachments = ssao_resolve_target->get_attachments().data();

        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(final_effects_shader->get_position_depth_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[0].texture->get_object());
        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(final_effects_shader->get_albedo_metallic_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[1].texture->get_object());
        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(final_effects_shader->get_normal_roughness_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[2].texture->get_object());
        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(final_effects_shader->get_emission_ambient_occlusion_index()));
        glBindTexture(GL_TEXTURE_2D, gbuffers_attachments[3].texture->get_object());
        glActiveTexture(GL_TEXTURE0 + static_cast<enumerated>(final_effects_shader->get_ssao_resolved_index()));
        glBindTexture(GL_TEXTURE_2D, ssao_resolved_attachments[0].texture->get_object());

        final_effects_shader->set_screen_uv_move_reserved_data(screen_uv_move_reserved);

        for (auto& camera_layer_entity_id_pool_index : scene.cameras) {
            GX_GL_CHECK_D;
            auto& camera = camera_pool[camera_layer_entity_id_pool_index.second];

            glBindVertexArray(screen_vertex_object);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            GX_GL_CHECK_D;
        }
    }
    GX_GL_CHECK_D;

    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glGetError();

#ifdef GX_PLATFORM_INTERFACE_SDL2
    SDL_GL_SwapWindow(os_app.get_window());
#elif defined(GX_PLATFORM_INTERFACE_ANDROID)
    os_app.get_gl_context()->swap();
#endif
    GX_GL_CHECK_D;
}

#endif
