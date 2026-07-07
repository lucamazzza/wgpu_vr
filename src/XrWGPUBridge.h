#pragma once

#define XR_USE_GRAPHICS_API_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR

#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <webgpu/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <dawn/native/VulkanBackend.h>

#include <vector>
#include <memory>
#include <stdexcept>

struct EyeTarget {
    wgpu::Texture wgpuOffscreenTexture;
    wgpu::TextureView wgpuTextureView;
    VkImage extractedDawnImage;
    XrSwapchain openxrSwapchain;
    std::vector<XrSwapchainImageVulkanKHR> swapchainImages;
    uint32_t width, height;
};

class XrWGPUBridge {
public:
    XrWGPUBridge();
    ~XrWGPUBridge();
    
    void Initialize();
    bool RenderFrame();
    
    wgpu::Device GetWGPUDevice() const { return m_wgpuDevice; }
    wgpu::Queue GetWGPUQueue() const { return m_wgpuQueue; }

private:
    void InitOpenXR();
    void InitVulkanViaOpenXR();
    void InitDawnFromVulkan();
    void CreateSwapchainsAndRenderTargets();
    void SetupVulkanBlitCommand();

    void RenderWebGPUScene(int eyeIndex, wgpu::TextureView targetView);
    void CopyDawnToOpenXR(VkImage srcDawnImage, VkImage dstOpenXrImage, uint32_t width, uint32_t height);

    XrInstance m_xrInstance = XR_NULL_HANDLE;
    XrSession m_xrSession = XR_NULL_HANDLE;
    XrSystemId m_xrSystemId = XR_NULL_SYSTEM_ID;
    XrSpace m_xrAppSpace = XR_NULL_HANDLE;

    VkInstance m_vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkQueue m_vkQueue = VK_NULL_HANDLE;
    uint32_t m_vkQueueFamilyIndex = 0;

    VkCommandPool m_vkCmdPool = VK_NULL_HANDLE;
    VkCommandBuffer m_vkCmdBuffer = VK_NULL_HANDLE;

    std::unique_ptr<dawn::native::Instance> m_dawnInstance;
    wgpu::Device m_wgpuDevice;
    wgpu::Queue m_wgpuQueue;

    std::vector<EyeTarget> m_eyeTargets;

};