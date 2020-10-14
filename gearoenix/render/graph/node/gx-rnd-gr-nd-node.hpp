#ifndef GEAROENIX_RENDER_GRAPH_NODE_NODE_HPP
#define GEAROENIX_RENDER_GRAPH_NODE_NODE_HPP
#include "../../../core/graph/gx-cr-gr-node.hpp"
#include "../../../core/sync/gx-cr-sync-end-caller.hpp"
#include "../../pipeline/gx-rnd-pip-type.hpp"
#include "gx-rnd-gr-nd-type.hpp"
#include <map>
#include <memory>
#include <vector>

/// Node is responsible to record command buffers.
/// It gathers all the needed data like pipeline, descriptor-sets, ... .
/// Each node has input and output links.
/// Node must provide render-data that is contain none-shared objects, like command buffer, semaphores, ...
/// There are some predefined ids for links, in addition users of engine can specify their ids
/// Node must bring a structure for its output link, and must accept a trait as its dependency
/// Node will contain strong pointer for its providers and weak pointer for its consumers
/// A node must register its consumers and signal them whenever their needed data is gathered
/// So do not forget, provider is responsible for signaling its registered consumers
/// and consumers are responsible for waiting on signal of provider for using data
/// Each node will get data of the light, scene, camera, model, ...
/// Always there is and must be a single provider for each dependency
/// But there might be several or zero consumer for a data
/// This system can be used to push independent commands on separate command queues

namespace gearoenix::render {
namespace command {
    class Buffer;
}
namespace engine {
    class Engine;
}
namespace pipeline {
    class Pipeline;
}
namespace sync {
    class Semaphore;
}
namespace texture {
    class Texture;
    class TextureCube;
    class Target;
}
namespace graph::node {
    class Node : public core::graph::Node {
        GX_GET_CVAL_PRT(Type, render_node_type)
    protected:
        engine::Engine* const e;
        std::vector<texture::Texture*> input_textures;
        std::vector<std::shared_ptr<texture::Texture>> output_textures;
        std::vector<std::vector<sync::Semaphore*>> link_providers_frames_semaphores;
        std::vector<std::map<std::pair<core::Id, unsigned int>, std::vector<std::shared_ptr<sync::Semaphore>>>> links_consumers_frames_semaphores;
        std::vector<std::unique_ptr<command::Buffer>> frames_primary_cmd;
        /// These are for preventing redundant allocation & deallocation in render loop
        std::vector<std::vector<sync::Semaphore*>> previous_semaphores, next_semaphores;
        const texture::Target* render_target = nullptr;
        std::shared_ptr<pipeline::Pipeline> render_pipeline = nullptr;
        Node(
            std::string name,
            Type t,
            engine::Engine* e,
            pipeline::Type pipeline_type_id,
            unsigned int input_textures_count,
            unsigned int output_textures_count,
            const std::vector<std::string>& input_links,
            const std::vector<std::string>& output_links,
            const core::sync::EndCaller<core::sync::EndCallerIgnore>& call) noexcept;
        void update_next_semaphores() noexcept;
        void update_previous_semaphores() noexcept;

    public:
        ~Node() noexcept override;
        void set_provider(unsigned int input_link_index, core::graph::Node* o, unsigned int provider_output_link_index) noexcept final;
        void remove_provider(unsigned int input_link_index) noexcept final;
        void set_providers_count(std::size_t count) noexcept final;
        void remove_consumer(unsigned int output_link_index, core::Id node_id, unsigned int consumer_input_link_index) noexcept final;
        [[nodiscard]] const std::shared_ptr<texture::Texture>& get_output_texture(unsigned int index) const noexcept;
        virtual void set_input_texture(texture::Texture* t, unsigned int index) noexcept;
        virtual void set_render_target(const texture::Target* t) noexcept;
        virtual void update() noexcept;
        virtual void record(unsigned int kernel_index) noexcept;
        virtual void record_continuously(unsigned int kernel_index) noexcept;
        virtual void submit() noexcept;
        const std::vector<std::shared_ptr<sync::Semaphore>>& get_link_frames_semaphore(unsigned int output_link_index, core::Id consumer_id, unsigned int consumer_input_link_index) noexcept;
    };
}
}
#endif