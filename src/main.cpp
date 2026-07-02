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

	// TODO: Ask OpenXR for Vulkan requirements and init Dawn
	
	// Main loop
	bool isRunning = true;
	while (isRunning) {
		// TODO: Render Loop
	}

	// Cleanup
	xrDestroyInstance(xrInstance);
	return 0;
}
