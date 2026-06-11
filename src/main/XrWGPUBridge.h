#pragma once

#include <openxr/openxr.h>
#include <webgpu.h>

#include <memory>

class XrWGPUBridge {
public:
	XrWGPUBridge();
	~XrWGPUBridge();
	bool xrwgpuInitialize(XrInstance xrInstance, XrSystemId xrSystemId);
	XrSession xrwgpuCreateSession();
	WGPUDevice xrwgpuCreateDevice();
private:
	struct  VKInternals;
	std::unique_ptr<VKInternals> m_internals;
};