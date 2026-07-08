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

    const char *instanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        // TODO: ADD OTHER EXTENSIONS HERE
    };

    VkInstanceCreateInfo vkInstanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vkInstanceCreateInfo.enabledExtensionCount = (uint32_t)sizeof(instanceExtensions)/sizeof(instanceExtensions[0]);
    vkInstanceCreateInfo.ppEnabledExtensionNames = &instanceExtensions;
}
