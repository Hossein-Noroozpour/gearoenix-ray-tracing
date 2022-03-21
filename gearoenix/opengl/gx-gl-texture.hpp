#ifndef GEAROENIX_GL_TEXTURE_HPP
#define GEAROENIX_GL_TEXTURE_HPP
#include "../render/gx-rnd-build-configuration.hpp"
#ifdef GX_RENDER_OPENGL_ENABLED
#include "../render/texture/gx-rnd-txt-manager.hpp"
#include "../render/texture/gx-rnd-txt-texture-2d.hpp"
#include "gx-gl-types.hpp"

namespace gearoenix::gl {

struct Engine;
struct Uploader;

[[nodiscard]] sint convert_internal_format(render::texture::TextureFormat f) noexcept;
[[nodiscard]] enumerated convert_format(render::texture::TextureFormat f) noexcept;
[[nodiscard]] enumerated convert_data_format(render::texture::TextureFormat f) noexcept;
[[nodiscard]] float convert_min(render::texture::Filter f) noexcept;
[[nodiscard]] float convert_mag(render::texture::Filter f) noexcept;
[[nodiscard]] sint convert(render::texture::Wrap w) noexcept;

struct Texture2D final : public render::texture::Texture2D {
    friend struct TextureManager;

    Engine& e;
    GX_GET_VAL_PRV(uint, object, 0)

    Texture2D(
        Engine& e,
        const render::texture::TextureInfo& info,
        std::string name) noexcept;
    ~Texture2D() noexcept final;
    void bind(enumerated texture_unit) noexcept;
};

struct TextureManager final : public render::texture::Manager {
    Engine& e;

    TextureManager(Engine& e) noexcept;
    ~TextureManager() noexcept final;
    [[nodiscard]] std::shared_ptr<render::texture::Texture2D> create_2d_from_pixels(
        std::string name,
        std::vector<std::vector<std::uint8_t>> pixels,
        const render::texture::TextureInfo& info,
        const core::sync::EndCallerIgnored& c) noexcept final;
};
}

#endif
#endif