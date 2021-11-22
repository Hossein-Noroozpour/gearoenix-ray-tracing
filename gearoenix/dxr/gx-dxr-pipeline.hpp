#pragma once

#include "../render/gx-rnd-build-configuration.hpp"
#ifdef GX_RENDER_DXR_ENABLED
#include "gx-dxr-loader.hpp"

namespace gearoenix::dxr {
struct Device;
struct PipelineManager final {
    GX_GET_REFC_PRV(std::shared_ptr<Device>, device)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<ID3D12RootSignature>, g_buffer_filler_root_signature)
    GX_GET_CREF_PRV(Microsoft::WRL::ComPtr<ID3D12PipelineState>, g_buffer_filler_pipeline_state)

    PipelineManager(std::shared_ptr<Device> device) noexcept;
    ~PipelineManager() noexcept;
};
}

#endif