#include "XrWGPUBridge.h"

#include <iostream>
#include <cstring>

#ifdef _WIN32
#define XR_USE_GRAPHICS_API_D3D12
#include <d3d12.h>
#include <dxgi.h>
#include <openxr/openxr_platform.h>
#include <wrl/client.h>
#endif

struct XrWGPUBridge::DX12Internals {
#ifdef _WIN32
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12Queue = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> d3d12RenderTarget = nullptr;
    XrGraphicsBindingD3D12KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    std::vector<XrSwapchainImageD3D12KHR> xrImages;
#endif
};

XrWGPUBridge::XrWGPUBridge() : m_dx12(std::make_unique<DX12Internals>()) {}

XrWGPUBridge::~XrWGPUBridge() {
#ifdef _WIN32
    if (m_dx12->dxgiAdapter != nullptr) {
        m_dx12->dxgiAdapter = nullptr;
    }
    if (m_dx12->d3d12Device != nullptr) {
        m_dx12->d3d12Device = nullptr;
    }
    if (m_dx12->d3d12Queue != nullptr) {
        m_dx12->d3d12Queue = nullptr;
    }
#endif
}

bool XrWGPUBridge::xrwgpuInitialize(WGPUInstance wgpuInstance, WGPUDevice wgpuDevice, WGPUAdapter wgpuAdapter, WGPUQueue wgpuQueue) {
#ifdef _WIN32
    m_wgpuDevice = wgpuDevice;
    std::cout << "[XrWGPUBridge]: Extracting D3D12 handles from WebGPU..." << std::endl;

    auto dxgiFactory = static_cast<IDXGIFactory*>(wgpuInstanceGetD3D12Instance(wgpuInstance));
    auto dxgiAdapter = static_cast<IDXGIAdapter*>(wgpuAdapterGetD3D12PhysicalDevice(wgpuAdapter));
    auto d3d12Device = static_cast<ID3D12Device*>(wgpuDeviceGetD3D12Device(wgpuDevice));
    auto d3d12Queue = static_cast<ID3D12CommandQueue*>(wgpuQueueGetD3D12CommandQueue(wgpuQueue));

    m_dx12->dxgiAdapter.Attach(dxgiAdapter);
    m_dx12->d3d12Device.Attach(d3d12Device);
    m_dx12->d3d12Queue.Attach(d3d12Queue);


    if (m_dx12->d3d12Device == nullptr) {
        std::cerr << "[XrWGPUBridge]: fatal - D3D12 device pointer is nullptr" << std::endl;
        return false;
    }

    m_dx12->graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    m_dx12->graphicsBinding.next = nullptr;
    m_dx12->graphicsBinding.device = m_dx12->d3d12Device.Get();
    m_dx12->graphicsBinding.queue = m_dx12->d3d12Queue.Get();

    std::cout << "[XrWGPUBridge]: D3D12 handles ready." << std::endl;
    std::cout << "\tDXGI Factory: " << dxgiFactory << std::endl;
    std::cout << "\tDXGI Adapter: " << m_dx12->dxgiAdapter.Get() << std::endl;
    std::cout << "\tD3D12 Device: " << m_dx12->d3d12Device.Get() << std::endl;
    std::cout << "\tD3D12 Queue:  " << m_dx12->d3d12Queue.Get() << std::endl;

    return true;
#else
    (void)wgpuInstance;
    (void)wgpuDevice;
    (void)wgpuAdapter;
    std::cerr << "[XrWGPUBridge]: DX12 bridge is only supported on Windows." << std::endl;
    return false;
#endif
}

XrSession XrWGPUBridge::xrwgpuCreateSession(XrInstance xrInstance, XrSystemId xrSystemId) {
#ifdef _WIN32
    std::cout << "[XrWGPUBridge]: Checking D3D12 pointers:" << std::endl;
    std::cout << "\tDXGI adapter: " << m_dx12->dxgiAdapter.Get() << std::endl;
    std::cout << "\tD3D12 device: " << m_dx12->d3d12Device.Get() << std::endl;
    std::cout << "\tD3D12 queue:  " << m_dx12->d3d12Queue.Get() << std::endl;
    if (m_dx12->d3d12Device.Get() == nullptr || m_dx12->d3d12Queue.Get() == nullptr) {
        std::cerr << "[XrWGPUBridge]: fatal - D3D12 pointers are nullptr" << std::endl;
        return XR_NULL_HANDLE;
    }

    {
        PFN_xrGetD3D12GraphicsRequirementsKHR getRequirements = nullptr;
        XrResult procRes = xrGetInstanceProcAddr(
            xrInstance,
            "xrGetD3D12GraphicsRequirementsKHR",
            reinterpret_cast<PFN_xrVoidFunction *>(&getRequirements));
        if (XR_FAILED(procRes) || getRequirements == nullptr) {
            std::cerr << "[XrWGPUBridge]: function xrGetD3D12GraphicsRequirementsKHR not found" << std::endl;
            return XR_NULL_HANDLE;
        }

        XrGraphicsRequirementsD3D12KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
        XrResult reqRes = getRequirements(xrInstance, xrSystemId, &graphicsRequirements);
        if (XR_FAILED(reqRes)) {
            char errorStr[XR_MAX_RESULT_STRING_SIZE];
            xrResultToString(xrInstance, reqRes, errorStr);
            std::cerr << "[XrWGPUBridge]: failed to query D3D12 graphics requirements: " << errorStr << std::endl;
            return XR_NULL_HANDLE;
        }
        
        if (m_dx12->dxgiAdapter != nullptr) {
            DXGI_ADAPTER_DESC adapterDesc{};
            if (SUCCEEDED(m_dx12->dxgiAdapter.Get()->GetDesc(&adapterDesc))) {
                const bool luidMatches =
                    adapterDesc.AdapterLuid.HighPart == graphicsRequirements.adapterLuid.HighPart &&
                    adapterDesc.AdapterLuid.LowPart == graphicsRequirements.adapterLuid.LowPart;
                if (!luidMatches) {
                    std::cerr << "[XrWGPUBridge]: GPU mismatch between WebGPU adapter and OpenXR-required adapter" << std::endl;
                    return XR_NULL_HANDLE;
                }
            }
        }

        std::cout << "[XrWGPUBridge]: D3D12 requirements are satisfied" << std::endl;
        
    }

    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &m_dx12->graphicsBinding;
    sessionInfo.systemId = xrSystemId;
    sessionInfo.createFlags = 0;
    XrResult res = xrCreateSession(xrInstance, &sessionInfo, &m_xrSession);
    if (XR_FAILED(res)) {
        char errorStr[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(xrInstance, res, errorStr);
        std::cerr << "[XrWGPUBridge]: Failed to create OpenXR session with D3D12 binding" << std::endl;
        std::cerr << "\tCause: " << errorStr << std::endl;
        return XR_NULL_HANDLE;
    }
    return m_xrSession;
#else
    (void)xrInstance;
    (void)xrSystemId;
    std::cerr << "[XrWGPUBridge]: DX12 bridge is only supported on Windows." << std::endl;
    return XR_NULL_HANDLE;
#endif
}

void XrWGPUBridge::xrwgpuCreateSwapchain(
    XrSession session,
    WGPUTextureFormat wgpuFormat,
    int64_t nativeFormat,
    uint32_t width,
    uint32_t height
) {
#ifdef _WIN32
    m_xrSession = session;
    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.format = nativeFormat;
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
    m_dx12->xrImages.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
    xrEnumerateSwapchainImages(
        m_xrSwapchain,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader *>(m_dx12->xrImages.data()));
    WGPUTextureDescriptor desc = {};
    desc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {width, height, 1};
    desc.format = wgpuFormat;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    m_renderTarget = wgpuDeviceCreateTexture(m_wgpuDevice, &desc);
    m_renderTargetView = wgpuTextureCreateView(m_renderTarget, nullptr);
    auto d3d12RenderTarget = static_cast<ID3D12Resource *>(wgpuTextureGetD3D12Image(m_renderTarget));
    m_dx12->d3d12RenderTarget.Attach(d3d12RenderTarget);
    std::cout << "[XrWGPUBridge] Swapchain ready. ID3D12Resource extracted: " << m_dx12->d3d12RenderTarget << std::endl;
#else
    (void)session;
    (void)wgpuFormat;
    (void)nativeFormat;
    (void)width;
    (void)height;
#endif
}

WGPUTextureView XrWGPUBridge::xrwgpuAcquireNextImage() {
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t imageIndex;
    xrAcquireSwapchainImage(m_xrSwapchain, &acquireInfo, &imageIndex);
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(m_xrSwapchain, &waitInfo);
    (void)imageIndex;
    return m_renderTargetView;
}

void XrWGPUBridge::present() {
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(m_xrSwapchain, &releaseInfo);
}
