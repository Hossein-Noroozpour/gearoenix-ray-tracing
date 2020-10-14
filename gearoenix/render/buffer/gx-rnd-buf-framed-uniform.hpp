#ifndef GEAROENIX_RENDER_BUFFER_FRAMED_UNIFORM_HPP
#define GEAROENIX_RENDER_BUFFER_FRAMED_UNIFORM_HPP
#include "../engine/gx-rnd-eng-engine.hpp"
#include "gx-rnd-buf-uniform.hpp"
#include <memory>
#include <vector>
namespace gearoenix::render {
namespace engine {
    class Engine;
}
namespace buffer {
    class Uniform;
    class FramedUniform {
    protected:
        const engine::Engine* const e;
        std::vector<std::shared_ptr<Uniform>> uniforms;

    public:
        FramedUniform(std::size_t s, engine::Engine* e) noexcept;
        ~FramedUniform() noexcept;
        // Update current frame uniform buffer
        template <typename T>
        void update(const T& data) noexcept;
        // return current frame uniform buffer
        [[nodiscard]] const Uniform* get_buffer() const noexcept;
        [[nodiscard]] Uniform* get_buffer() noexcept;
    };

    template <typename T>
    inline void FramedUniform::update(const T& data) noexcept
    {
        uniforms[e->get_frame_number()]->set_data(data);
    }
}
}
#endif