#include "XrWGPUBridge.h"
#include "openxr/openxr.h"

#include <iostream>
#include <cassert>
#include <stdexcept>

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

void XrWGPUBridge::InitDawnFromVulkan() {
    m_dawnInstance = std::make_unique<dawn::native::Instance>();
    /* XXX: Here the Vulkan adapter must be discovered, but the method have been removed...
    
    dawn::native::vulkan::AdapterDiscoveryOptions adapterOptions;
    adapterOptions.vkInstance = m_vkInstance;
    adapterOptions.vkPhysicalDevice = m_vkPhysicalDevice;
    m_dawnInstance->DiscoverAdapters(&adapterOptions);

    auto adapters = m_dawnInstance->GetAdapters();
    if (adapters.empty()) throw std::runtime_error("Dawn failed to wrap Vulkan resources");
    
    dawn::native::Adapter nativeAdapter = adapters[0];
    
    WGPUDeviceDescriptor deviceDesc = {};
    m_wgpuDevice = wgpu::Device::Acquire(nativeAdapter.CreateDevice(&deviceDesc));
    m_wgpuQueue = m_wgpuDevice.GetQueue();

    std::cout << "Dawn successfully hooked onto Vulkan context" << std::endl;
    */
}

void XrWGPUBridge::CreateSwapchainsAndRenderTargets() {
    m_eyeTargets.resize(2);
    for (int i = 0; i < 2; i++) {
        m_eyeTargets[i].width = 2000; // FIXME: Should come from ViewConfigurationViews
        m_eyeTargets[i].height = 2000; // FIXME: Should come from ViewConfigurationViews

        uint32_t imageCount;
        xrEnumerateSwapchainImages(m_eyeTargets[i].openxrSwapchain, 0, &imageCount, nullptr);
        m_eyeTargets[i].swapchainImages.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        xrEnumerateSwapchainImages(
            m_eyeTargets[i].openxrSwapchain,
            imageCount,
            &imageCount,
            (XrSwapchainImageBaseHeader*)m_eyeTargets[i].swapchainImages.data()
        );

        wgpu::TextureDescriptor texDesc = {
            .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
            .size = wgpu::Extent3D{ m_eyeTargets[i].width, m_eyeTargets[i].height, 1 },
            .format = wgpu::TextureFormat::RGBA8UnormSrgb,
        };

        m_eyeTargets[i].wgpuOffscreenTexture = m_wgpuDevice.CreateTexture(&texDesc);
        m_eyeTargets[i].wgpuTextureView = m_eyeTargets[i].wgpuOffscreenTexture.CreateView();

        dawn::native::vulkan::ExternalImageExportInfoVk exportInfo;

        bool ok = dawn::native::vulkan::ExportVulkanImage(
            m_eyeTargets[i].wgpuOffscreenTexture.Get(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            &exportInfo
        );
		if (!ok) throw std::runtime_error("Failed to export Dawn texture to Vulkan image");
    }
}

void XrWGPUBridge::SetupVulkanBlitCommand() {
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_vkQueueFamilyIndex;
    vkCreateCommandPool(m_vkDevice, &poolInfo, nullptr, &m_vkCmdPool);

    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    allocInfo.commandPool = m_vkCmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &m_vkCmdBuffer);
}

bool XrWGPUBridge::RenderFrame() {
    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    xrWaitFrame(m_xrSession, &frameWaitInfo, &frameState);

    XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(m_xrSession, &frameBeginInfo);

    if (frameState.shouldRender) {
        for (int i = 0; i < 2; i++) {
            uint32_t imageIndex;
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrAcquireSwapchainImage(m_eyeTargets[i].openxrSwapchain, &acquireInfo, &imageIndex);

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            xrWaitSwapchainImage(m_eyeTargets[i].openxrSwapchain, &waitInfo);

            VkImage targetXrImage = m_eyeTargets[i].swapchainImages[imageIndex].image;

            RenderWebGPUScene(i, m_eyeTargets[i].wgpuTextureView);
            m_wgpuDevice.Tick();

            CopyDawnToOpenXR(m_eyeTargets[i].extractedDawnImage, targetXrImage, m_eyeTargets[i].width, m_eyeTargets[i].height);
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(m_eyeTargets[i].openxrSwapchain, &releaseInfo);
        }
    }
    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    // TODO: frame end info setup with projection layers
    xrEndFrame(m_xrSession, &frameEndInfo);
    return true;
}

void XrWGPUBridge::RenderWebGPUScene(int eyeIndex, wgpu::TextureView targetView) {
    wgpu::CommandEncoder encoder = m_wgpuDevice.CreateCommandEncoder();
    wgpu::RenderPassColorAttachment colorAttachment = {
        .view = targetView,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = {0.1f, 0.2f, 0.4f, 1.0f},
    };
    wgpu::RenderPassDescriptor renderPassDesc = {
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    };
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    // TODO: Draw geometry
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    m_wgpuQueue.Submit(1, &commands);
}

void XrWGPUBridge::CopyDawnToOpenXR(VkImage srcDawnImage, VkImage dstOpenXRImage, uint32_t width, uint32_t height) {
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_vkCmdBuffer, &beginInfo);

    // TODO: Memory barriers

    VkImageCopy copyRegion = {
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .extent = {width, height, 1},
    };

    vkCmdCopyImage(m_vkCmdBuffer,
                   srcDawnImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstOpenXRImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copyRegion);

    // TODO: Memory barriers
    
    vkEndCommandBuffer(m_vkCmdBuffer);
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_vkCmdBuffer;
    vkQueueSubmit(m_vkQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_vkQueue);
}
