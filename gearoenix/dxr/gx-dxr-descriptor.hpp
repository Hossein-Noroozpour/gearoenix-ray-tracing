#pragma once

#include "../render/gx-rnd-build-configuration.hpp"
#ifdef GX_RENDER_DXR_ENABLED
#include "gx-dxr-loader.hpp"

namespace gearoenix::core {
struct Allocator;
};

namespace gearoenix::dxr {

constexpr UINT GPU_DESCRIPTORS_COUNT = 10000;

struct Device;

struct DescriptorAllocator final {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    std::shared_ptr<core::Allocator> allocator;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_starting_handle {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_starting_handle {};
    UINT size_increment = 0;

    explicit DescriptorAllocator(Device& d) noexcept;
    ~DescriptorAllocator() noexcept;
};

struct Descriptor final {
    friend struct DescriptorManager;

    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;

private:
    const std::shared_ptr<core::Allocator> allocator;

    Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE&& cpu_handle, std::shared_ptr<core::Allocator>&& allocator) noexcept;

public:
    ~Descriptor() noexcept;
    Descriptor(Descriptor&&) noexcept;
};

struct DescriptorManager final {
    GX_GET_CREF_PRV(std::shared_ptr<Device>, device)

private:
    DescriptorAllocator allocator;

public:
    explicit DescriptorManager(std::shared_ptr<Device> device) noexcept;
    ~DescriptorManager() noexcept;
    [[nodiscard]] Descriptor allocate() noexcept;
};
}

#endif