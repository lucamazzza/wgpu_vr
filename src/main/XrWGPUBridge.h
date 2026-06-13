#pragma once

#include <openxr/openxr.h>
#include <webgpu.h>

#include <vector>
#include <memory>

class XrWGPUBridge {
public:
    XrWGPUBridge();
    ~XrWGPUBridge();
    bool xrwgpuInitialize(WGPUInstance wgpuInstance, WGPUDevice wgpuDevice);
    XrSession xrwgpuCreateSession(XrInstance xrInstance, XrSystemId xrSystemId);
    void xrwgpuCreateSwapchain(XrSession session, int64_t format, uint32_t width, uint32_t height);
    WGPUTextureView xrwgpuAcquireNextImage();
    void present();
private:
    struct VKInternals;
    std::unique_ptr<VKInternals> m_vk;

    WGPUDevice m_wgpuDevice = nullptr;
    XrSession m_xrSession = XR_NULL_HANDLE;
    XrSwapchain m_xrSwapchain = XR_NULL_HANDLE;

    std::vector<WGPUTexture> m_swapchainTextures;
    std::vector<WGPUTextureView> m_swapchainViews;
};
