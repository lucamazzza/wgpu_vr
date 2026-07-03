#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <string>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <webgpu/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <dawn/native/VulkanBackend.h>
#include "dawn/native/vulkan/DeviceVk.h"

#define XR_CHECK(res) if (XR_FAILED(res)) { \
    std::cerr << "OpenXR Error Code: " << res << " at line " << __LINE__ << std::endl; \
    exit(-1); \
}

int main() {
	std::cout << "Starting main demo..." << std::endl;

	// OpenXR Instance Initialization
	const char* requestedExtensions[] = {
		XR_KHR_VULKAN_ENABLE_EXTENSION_NAME
	};
	XrInstanceCreateInfo xrInstanceCreateInfo{XR_TYPE_INSTANCE_CREATE_INFO};
	strcpy_s(xrInstanceCreateInfo.applicationInfo.applicationName, "Main_Demo");
	xrInstanceCreateInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

	xrInstanceCreateInfo.enabledExtensionCount = sizeof(requestedExtensions) / sizeof(requestedExtensions[0]);
	xrInstanceCreateInfo.enabledExtensionNames = requestedExtensions;

	XrInstance xrInstance;
	XrResult res = xrCreateInstance(&xrInstanceCreateInfo, &xrInstance);
	if (XR_FAILED(res)) {
		std::cerr << "[OpenXR]: Failed to create OpenXR instance: " << res << std::endl;
		return -1;
	}
	std::cout << "[OpenXR]: OpenXR instance created successfully." << std::endl;

	XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrSystemId xrSystemId;
	XR_CHECK(xrGetSystem(xrInstance, &systemInfo, &xrSystemId));

	// TODO: Ask OpenXR for Vulkan requirements and init Dawn
	
	std::cout << "[Dawn]: Initializing Dawn with Vulkan backend..." << std::endl;

	XrGraphicsRequirementsVulkanKHR vkReqs{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
	PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
	xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsRequirementsKHR);
	XR_CHECK(pfnGetVulkanGraphicsRequirementsKHR(xrInstance, xrSystemId, &vkReqs));

	wgpu::InstanceDescriptor instanceDesc{};
	auto dawnInstance = std::make_unique<dawn::native::Instance>(&instanceDesc);
	dawn::native::Adapter vkAdapter;
	bool adapterFound = false;

	std::vector<dawn::native::Adapter> nativeAdapters = dawnInstance->EnumerateAdapters();
	for (auto& nativeAdapter : nativeAdapters) {
		wgpu::Adapter adapter = wgpu::Adapter(nativeAdapter.Get());
		wgpu::AdapterInfo info{};
		adapter.GetInfo(&info);
		if (info.backendType == wgpu::BackendType::Vulkan) {
			vkAdapter = nativeAdapter;
			adapterFound = true;
			std::cout << "[Dawn]: Vulkan adapter found" << std::endl;
			break;
		}
	}
	if (!adapterFound) {
		std::cerr << "[Dawn]: No Vulkan adapter found" << std::endl;
		return -1;
	}

	wgpu::DeviceDescriptor deviceDesc{};
	wgpu::Device device = vkAdapter.CreateDevice(&deviceDesc);

	WGPUDevice cDevice = device.Get();
	auto* internalVkDevice = reinterpret_cast<dawn::native::vulkan::Device*>(cDevice);
	VkDevice vkDevice = internalVkDevice->GetVkDevice();

	if (vkDevice == VK_NULL_HANDLE) {
		std::cerr << "[Dawn]: Failed to retrieve Vulkan device from Dawn" << std::endl;
		return -1;
	}
	std::cout << "[Dawn]: Vulkan device retrieved successfully from Dawn" << std::endl;
	
	// Main loop
	bool isRunning = true;
	while (isRunning) {
		// TODO: Render Loop
	}

	// Cleanup
	xrDestroyInstance(xrInstance);
	return 0;
}
