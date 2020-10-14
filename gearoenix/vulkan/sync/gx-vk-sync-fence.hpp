#ifndef GEAROENIX_VULKAN_FENCE_HPP
#define GEAROENIX_VULKAN_FENCE_HPP
#include "../../core/gx-cr-build-configuration.hpp"
#ifdef GX_USE_VULKAN
#include "../../core/gx-cr-static.hpp"
#include "../gx-vk-loader.hpp"

namespace gearoenix::vulkan::device {
class Logical;
}

namespace gearoenix::vulkan::sync {
class Fence {
    GX_GET_REFC_PRV(std::shared_ptr<device::Logical>, logical_device)
    GX_GET_VAL_PRV(VkFence, vulkan_data, nullptr)

public:
    explicit Fence(std::shared_ptr<device::Logical> logical_device, bool signaled = false) noexcept;
    ~Fence() noexcept;
    void wait() noexcept;
    void reset() noexcept;
};
}
#endif
#endif
