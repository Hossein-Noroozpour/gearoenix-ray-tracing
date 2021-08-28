#ifndef GEAROENIX_DIRECT3DX_SWAPCHAIN_HPP
#define GEAROENIX_DIRECT3DX_SWAPCHAIN_HPP
#include "../render/gx-rnd-build-configuration.hpp"
#ifdef GX_RENDER_DIRECT3DX_ENABLED
#include "gx-d3d-loader.hpp"

namespace gearoenix::platform {
struct Application;
}

namespace gearoenix::direct3dx {
struct Device;
struct Queue;
struct Swapchain final {
    constexpr static UINT BACK_BUFFERS_COUNT = 2;
    constexpr static DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM;
    constexpr static DXGI_FORMAT DEPTH_BUFFER_FORMAT = DXGI_FORMAT_D32_FLOAT;

    struct Frame final {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
        Microsoft::WRL::ComPtr<ID3D12Resource> render_target;
        UINT64 fence_value = 0;
    };

    GX_GET_REFC_PRV(std::shared_ptr<Queue>, queue)
    GX_GET_REFC_PRV(std::shared_ptr<Device>, device)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, rtv_descriptor_heap)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>, dsv_descriptor_heap)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6>, command_list)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<ID3D12Fence>, fence)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<IDXGISwapChain4>, swapchain)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<ID3D12Resource>, depth_stencil)
    GX_GET_CREF_PRV(Microsoft::WRL::Wrappers::Event, fence_event)
    GX_GET_CREF_PRV(D3D12_VIEWPORT, screen_viewport)
    GX_GET_CREF_PRV(D3D12_RECT, scissor_rect)
    GX_GET_ARRC_PRV(Frame, frames, BACK_BUFFERS_COUNT)
    GX_GET_VAL_PRV(UINT, back_buffer_index, 0)
    GX_GET_VAL_PRV(UINT, rtv_descriptor_size, 0)

    explicit Swapchain(std::shared_ptr<Queue> queue) noexcept;
    ~Swapchain() noexcept;
    /// Returns true if device is lost.
    [[nodiscard]] bool set_window_size(const platform::Application&) noexcept;
    void wait_for_gpu() noexcept;
    [[nodiscard]] const Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& get_current_command_allocator() const noexcept;
    [[nodiscard]] const Microsoft::WRL::ComPtr<ID3D12Resource>& get_current_render_target() const noexcept;
    void prepare(D3D12_RESOURCE_STATES before_state) noexcept;
    [[nodiscard]] bool present(D3D12_RESOURCE_STATES before_state) noexcept;

private:
    void move_to_next_frame() noexcept;
};
}

#endif
#endif