#include "XrWGPUBridge.h"
#include <iostream>

#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr_platform.h>

struct XrWGPUBridge::VKInternals {
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice vkDevice = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
    XrGraphicsBindingVulkanKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    std::vector<XrSwapchainImageVulkanKHR> xrImages;
};

XrWGPUBridge::XrWGPUBridge() : m_vk(std::make_unique<VKInternals>()) {}

XrWGPUBridge::~XrWGPUBridge() {}

bool XrWGPUBridge::xrwgpuInitialize(WGPUInstance wgpuInstance, WGPUDevice wgpuDevice) {
    m_wgpuDevice = wgpuDevice;
    std::cout << "[XrWGPUBridge]: Extracting Vulkan handles from WebGPU..." << std::endl;

    /*
        TODO: extract these API handles in Rust source code

    m_vk->vkInstance = wgpuInstanceGetVulkanInstance(wgpuInstance);
    m_vk->vkDevice = wgpuDeviceGetVulkanDevice(wgpuDevice);
    m_vk->vkPhysicalDevice = wgpuDeviceGetVulkanPhysicalDevice(wgpuDevice);
    */

    m_vk->graphicsBinding.instance = m_vk->vkInstance;
    m_vk->graphicsBinding.physicalDevice = m_vk->vkPhysicalDevice;
    m_vk->graphicsBinding.device = m_vk->vkDevice;
    m_vk->graphicsBinding.queueFamilyIndex = m_vk->queueFamilyIndex;
    m_vk->graphicsBinding.queueIndex = 0;
    return true;
}

XrSession XrWGPUBridge::xrwgpuCreateSession(XrInstance xrInstance, XrSystemId xrSystemId) {
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &m_vk->graphicsBinding;
    sessionInfo.systemId = xrSystemId;
    XrResult res = xrCreateSession(xrInstance, &sessionInfo, &m_xrSession);
    if (XR_FAILED(res)) {
        std::cerr << "[XrWGPUBridge]: Failed to create OpenXR session with Vulkan binding" << std::endl;
        return XR_NULL_HANDLE;
    }
    return m_xrSession;
}
