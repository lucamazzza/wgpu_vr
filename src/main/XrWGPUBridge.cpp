#include "XrWGPUBridge.h"
#include "openxr/openxr.h"
#include "vulkan/vulkan_core.h"
#include <iostream>

#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr_platform.h>

struct XrWGPUBridge::VKInternals {
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice vkDevice = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
    VkQueue vkQueue = VK_NULL_HANDLE;
    VkCommandPool vkCmdPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCmdBuffer = VK_NULL_HANDLE;
    VkImage vkRenderTargetImage = VK_NULL_HANDLE;
    XrGraphicsBindingVulkanKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    std::vector<XrSwapchainImageVulkanKHR> xrImages;
};

XrWGPUBridge::XrWGPUBridge() : m_vk(std::make_unique<VKInternals>()) {}

XrWGPUBridge::~XrWGPUBridge() {}

bool XrWGPUBridge::xrwgpuInitialize(WGPUInstance wgpuInstance, WGPUDevice wgpuDevice, WGPUAdapter wgpuAdapter) {
    m_wgpuDevice = wgpuDevice;
    std::cout << "[XrWGPUBridge]: Extracting Vulkan handles from WebGPU..." << std::endl;
    m_vk->vkInstance = (VkInstance)wgpuInstanceGetVulkanInstance(wgpuInstance);
    m_vk->vkDevice = (VkDevice)wgpuDeviceGetVulkanDevice(wgpuDevice);
    m_vk->vkPhysicalDevice = (VkPhysicalDevice)wgpuAdapterGetVulkanPhysicalDevice(wgpuAdapter);
    memset(&m_vk->graphicsBinding, 0, sizeof(XrGraphicsBindingVulkanKHR));
    m_vk->graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
    m_vk->graphicsBinding.instance = m_vk->vkInstance;
    m_vk->graphicsBinding.physicalDevice = m_vk->vkPhysicalDevice;
    m_vk->graphicsBinding.device = m_vk->vkDevice;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_vk->vkPhysicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_vk->vkPhysicalDevice, &queueFamilyCount, queueFamilies.data());
    uint32_t graphicsQueueIndex = 0;
    bool queueFound = false;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueIndex = i;
            queueFound = true;
            break;
        }
    }
    if (!queueFound) {
        std::cerr << "[XrWGPUBridge]: No Graphics Queue found on GPU" << std::endl;
        return false;
    }
    std::cout << "[XrWGPUBridge]: Vulkan Graphics Queue found at index: " << graphicsQueueIndex << std::endl;
    m_vk->graphicsBinding.queueFamilyIndex = graphicsQueueIndex;
    m_vk->queueFamilyIndex = graphicsQueueIndex;
    m_vk->graphicsBinding.queueIndex = 0;
    vkGetDeviceQueue(m_vk->vkDevice, m_vk->queueFamilyIndex, 0, &m_vk->vkQueue);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = m_vk->queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(m_vk->vkDevice, &poolInfo, nullptr, &m_vk->vkCmdPool);
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = m_vk->vkCmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_vk->vkDevice, &allocInfo, &m_vk->vkCmdBuffer);
    return true;
}

XrSession XrWGPUBridge::xrwgpuCreateSession(XrInstance xrInstance, XrSystemId xrSystemId) {
    std::cout << "[XrWGPUBridge]: Checking FFI WebGPU Pointers:" << std::endl;
    std::cout << "\tvkInstance:       " << m_vk->vkInstance << std::endl;
    std::cout << "\tvkPhysicalDevice: " << m_vk->vkPhysicalDevice << std::endl;
    std::cout << "\tvkDevice:         " << m_vk->vkDevice << std::endl;
    if (!m_vk->vkInstance || !m_vk->vkPhysicalDevice || !m_vk->vkDevice) {
        std::cerr << "[XrWGPUBridge]: fatal - FFI Pointers are nullptr" << std::endl;
        return XR_NULL_HANDLE;
    }
    {
        PFN_xrGetVulkanGraphicsRequirementsKHR checkRequirements = nullptr;
        xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirementsKHR",
                              (PFN_xrVoidFunction*)&checkRequirements);
        if (checkRequirements != nullptr) {
            XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
            checkRequirements(xrInstance, xrSystemId, &graphicsRequirements);
            std::cout << "[XrWGPUBridge]: Vulkan Requirements are satisfied" << std::endl;
        } else {
            std::cerr << "[XrWGPUBridge]: function xrGetVulkanGraphicsRequirementsKHR not found" << std::endl;
        }
    }
    {
        PFN_xrGetVulkanGraphicsDeviceKHR getVulkanDeviceFunc = nullptr;
        xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&getVulkanDeviceFunc);
        if (getVulkanDeviceFunc != nullptr) {
            VkPhysicalDevice openxrRequestedDevice = VK_NULL_HANDLE;
            XrResult devRes = getVulkanDeviceFunc(xrInstance, xrSystemId, m_vk->vkInstance, &openxrRequestedDevice);
            if (XR_SUCCEEDED(devRes)) {
                std::cout << "[XrWGPUBridge] OpenXR advises to use PhysicalDevice: " << openxrRequestedDevice << std::endl;
                if (openxrRequestedDevice != m_vk->vkPhysicalDevice) {
                    std::cerr << "[XrWGPUBridge] GPU Mismatch" << std::endl;
                    std::cerr << "WebGPU is using: " << m_vk->vkPhysicalDevice << " but OpenXR wants: " << openxrRequestedDevice << std::endl;
                }
            }
        }
    }
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &m_vk->graphicsBinding;
    sessionInfo.systemId = xrSystemId;
    sessionInfo.createFlags = 0;
    XrResult res = xrCreateSession(xrInstance, &sessionInfo, &m_xrSession);
    if (XR_FAILED(res)) {
        char errorStr[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(xrInstance, res, errorStr);
        std::cerr << "[XrWGPUBridge]: Failed to create OpenXR session with Vulkan binding" << std::endl;
        std::cerr << "\tCause: " << errorStr << std::endl;
        return XR_NULL_HANDLE;
    }
    return m_xrSession;
}

void XrWGPUBridge::xrwgpuCreateSwapchain(
    XrSession session,
    WGPUTextureFormat wgpuFormat,
    int64_t vulkanFormat,
    uint32_t width,
    uint32_t height
) {
    m_xrSession = session;
    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.format = vulkanFormat;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = width;
    swapchainInfo.height = height;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;
    XrResult res = xrCreateSwapchain(m_xrSession, &swapchainInfo, &m_xrSwapchain);
    if (XR_FAILED(res)) {
        std::cerr << "[XrWGPUBridge] Cannot create OpenXR swapchain" << std::endl;
        return;
    }
    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(m_xrSwapchain, 0, &imageCount, nullptr);
    std::cout << "[XrWGPUBridge] Successfully created swapchain with " << imageCount << " images" << std::endl;
    m_vk->xrImages.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
    xrEnumerateSwapchainImages(m_xrSwapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*) m_vk->xrImages.data());
    WGPUTextureDescriptor desc = {};
    desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {width, height, 1};
    desc.format = wgpuFormat;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    m_renderTarget = wgpuDeviceCreateTexture(m_wgpuDevice, &desc);
    m_renderTargetView = wgpuTextureCreateView(m_renderTarget, nullptr);
    m_vk->vkRenderTargetImage = (VkImage)wgpuTextureGetVulkanImage(m_renderTarget);
    std::cout << "[XrWGPUBridge] Swapchain ready. VkImage WebGPU extracted: " << m_vk->vkRenderTargetImage << std::endl;
}

WGPUTextureView XrWGPUBridge::xrwgpuAcquireNextImage() {
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t imageIndex;
    xrAcquireSwapchainImage(m_xrSwapchain, &acquireInfo, &imageIndex);
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(m_xrSwapchain, &waitInfo);
    return m_swapchainViews[imageIndex];
}

void XrWGPUBridge::present() {
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(m_xrSwapchain, &releaseInfo);
}
