#ifndef GEAROENIX_VULKAN_DESCRIPTOR_SET_HPP
#define GEAROENIX_VULKAN_DESCRIPTOR_SET_HPP
#include "../../core/gx-cr-build-configuration.hpp"
#ifdef USE_VULKAN
#include "../../core/cache/gx-cr-cache-cached.hpp"
#include "../gx-vk-linker.hpp"
namespace gearoenix {
namespace render {
    namespace command {
        class Buffer;
    }
    namespace pipeline {
        class Pipeline;
    }
    namespace descriptor {
        class SetLayout;
        class Pool;
        class Set : public core::cache::Cached {
        private:
            Pool* pool;
            VkDescriptorSetLayout layout;
            unsigned int layouts_count;
            VkDescriptorSet vulkan_data;

        public:
            Set(Pool* pool, SetLayout* lay, const VkDescriptorBufferInfo& buff_info);
            Set(Pool* pool, SetLayout* lay, const VkDescriptorBufferInfo& buff_info, const VkDescriptorImageInfo& img_info);
            ~Set();
            const Pool* get_pool() const;
            Pool* get_pool();
            unsigned int get_layouts_count() const;
            const VkDescriptorSetLayout& get_layout() const;
            const VkDescriptorSet& get_vulkan_data() const;
            void bind(pipeline::Pipeline* p, command::Buffer* c);
        };

    } // namespace descriptor
} // namespace render
} // namespace gearoenix
#endif
#endif // GEAROENIX_RENDER_DESCRIPTOR_SET_HPP
