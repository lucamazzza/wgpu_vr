#pragma once

#include <openxr/openxr.h>
#include <glm/glm.hpp>
#include <wgpu.h>
#include <vector>
#include <functional>

#include "XrWGPUBridge.h"

class XrBridge {
public:
	XrBridge();
	enum class Eye { Left, Right };
	struct Buffers {
		WGPUBuffer matricesBuffer;
		WGPUBuffer lightsBuffer;
	};
	typedef std::function<void(const Eye eye, glm::mat4 projection, glm::mat4 modelview, glm::mat4 normal, WGPUTextureView nextImage)> RenderFunction_t;
	bool Init(WGPUInstance instance, WGPUDevice device, WGPUAdapter adapter, WGPUQueue queue);
	bool Free();
	bool Update(bool &isSessionRunning);
	bool Render(const RenderFunction_t &renderFunction);
	void SetPlanes(float nearPlane, float farPlane) { m_nearPlane = nearPlane; m_farPlane = farPlane; }
private:

	uint32_t m_width;
	uint32_t m_height;

	float m_nearPlane;
	float m_farPlane;

	XrInstance m_instance;
	XrSystemId m_systemId;
	XrSession m_session;
	XrSessionState m_sessionState;
	std::vector<XrSwapchain> m_swapchains;
	XrSpace m_space;

	XrWGPUBridge m_bridge;
};

