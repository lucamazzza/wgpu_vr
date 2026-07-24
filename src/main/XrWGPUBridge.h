#pragma once

#include <openxr/openxr.h>
#include <webgpu.h>

#include <vector>
#include <memory>

class XrWGPUBridge {
public:
    XrWGPUBridge();
    ~XrWGPUBridge();
    bool xrwgpuInitialize(WGPUInstance wgpuInstance, WGPUDevice wgpuDevice, WGPUAdapter wgpuAdapter, WGPUQueue wgpuQueue);
    XrSession xrwgpuCreateSession(XrInstance xrInstance, XrSystemId xrSystemId);
    void xrwgpuCreateSwapchain(XrSession session, WGPUTextureFormat wgpuFormat, int64_t nativeFormat, uint32_t width, uint32_t height, uint32_t viewCount);
    XrSwapchain xrwgpuGetSwapchainHandle(uint32_t viewIdx) const;
    WGPUTextureView xrwgpuAcquireNextImage(uint32_t viewIdx);
    void xrwgpuPresent(uint32_t viewIdx);
private:
    struct DX12Internals;
    std::unique_ptr<DX12Internals> m_dx12;
    WGPUDevice m_wgpuDevice = nullptr;
    XrSession m_xrSession = XR_NULL_HANDLE;
    std::vector<WGPUTexture> m_swapchainTextures;
    std::vector<WGPUTextureView> m_swapchainViews;
    WGPUTexture m_renderTarget = nullptr;
    WGPUTextureView m_renderTargetView = nullptr;
};
