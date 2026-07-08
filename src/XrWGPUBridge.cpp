#include "XrWGPUBridge.h"
#include <iostream>
#include <cassert>

XrWGPUBridge::XrWGPUBridge() {}

XrWGPUBridge::~XrWGPUBridge() {

}

void XrWGPUBridge::Initialize() {
    InitOpenXR();
    InitVulkanViaOpenXR();
    InitDawnFromVulkan();
    CreateSwapchainsAndRenderTargets();
    SetupVulkanBlitCommand();
}

void XrWGPUBridge::InitOpenXR() {
    // TODO:
    // * Create OpenXR Instance with xrCreateInstance, ensuring XR_KHR_vulkan_enable2
    // * get the system id from the instance and put it it m_xrSystemId
    std::cout << "OpenXR Initialized" << std::endl;
}

void XrWGPUBridge::InitVulkanViaOpenXR() {
    PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR;
    xrGetInstanceProcAddr(m_xrInstance, "xrGetVulkanGraphicsRequirements2KHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsRequirements2KHR);
    PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR;
    xrGetInstanceProcAddr(m_xrInstance, "xrCreateVulkanInstanceKHR", (PFN_xrVoidFunction*)&xrCreateVulkanInstanceKHR);
    PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR;
    xrGetInstanceProcAddr(m_xrInstance, "xrGetVulkanGraphicsDevice2KHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsDevice2KHR);
    PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR;
    xrGetInstanceProcAddr(m_xrInstance, "xrCreateVulkanDeviceKHR", (PFN_xrVoidFunction*)&xrCreateVulkanDeviceKHR);

    XrGraphicsRequirementsVulkanKHR vkReqs{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    xrGetVulkanGraphicsRequirements2KHR(m_xrInstance, m_xrSystemId, &vkReqs);

    const char *instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        // TODO: ADD OTHER EXTENSIONS HERE
    };

    VkInstanceCreateInfo vkInstanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vkInstanceCreateInfo.enabledExtensionCount = (uint32_t)sizeof(instanceExtensions)/sizeof(instanceExtensions[0]);
    vkInstanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;

    XrVulkanInstanceCreateInfoKHR createInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    createInfo.systemId = m_xrSystemId;
    createInfo.vulkanCreateInfo = &vkInstanceCreateInfo;

    VkResult vkResult;
    xrCreateVulkanInstanceKHR(m_xrInstance, &createInfo, &m_vkInstance, &vkResult);

    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    deviceGetInfo.systemId = m_xrSystemId;
    xrGetVulkanGraphicsDevice2KHR(m_xrInstance, &deviceGetInfo, &m_vkPhysicalDevice);

    const char *deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        // TODO: ADD OTHER EXTENSIONS HERE
    };

    VkDeviceCreateInfo vkDeviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    vkDeviceCreateInfo.enabledExtensionCount = (uint32_t)sizeof(deviceExtensions)/sizeof(deviceExtensions[0]);
    vkDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    // TODO: SETUP QUEUE FAMILIES

    XrVulkanDeviceCreateInfoKHR deviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    deviceCreateInfo.systemId = m_xrSystemId;
    deviceCreateInfo.vulkanCreateInfo = &vkDeviceCreateInfo;
    xrCreateVulkanDeviceKHR(m_xrInstance, &deviceCreateInfo, &m_vkDevice, &vkResult);
    vkGetDeviceQueue(m_vkDevice, m_vkQueueFamilyIndex, 0, &m_vkQueue);
    std::cout << "Vulkan Context Created via OpenXR" << std::endl;
}
