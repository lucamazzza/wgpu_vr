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
