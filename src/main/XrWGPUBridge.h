/*!
 * \file XrWGPUBridge.h
 * \brief Bridge between OpenXR and WebGPU
 * 
 * \author Luca Mazza
 * \copyright 2026 Luca Mazza
 */
#pragma once

#include <openxr/openxr.h>
#include <webgpu.h>

#include <vector>
#include <memory>

/*!
 * \brief Bridge between OpenXR and WebGPU
 * 
 * \details This class provides a bridge between OpenXR and WebGPU,
 * allowing for the creation of an OpenXR session and swapchains
 * that can be used with WebGPU. 
 * It handles the initialization of the necessary graphics bindings
 * and manages the swapchain images for rendering.
 */
class XrWGPUBridge {
public:

	/*!
	 * \brief Constructor
	 *
	 * \details Initializes the XrWGPUBridge instance.
	 */
    XrWGPUBridge();

	/*!
    * \brief Destructor
    *
    * \details Cleans up the XrWGPUBridge instance.
    */
    ~XrWGPUBridge();

    /*!
     * \brief Initialize the bridge
     *
     * \details Initializes the bridge with the provided WebGPU resources.
     *
     * \param wgpuInstance The WebGPU instance
     * \param wgpuDevice The WebGPU device
     * \param wgpuAdapter The WebGPU adapter
     * \param wgpuQueue The WebGPU queue
     * \return true if initialization was successful, false otherwise
     */
    bool xrwgpuInitialize(WGPUInstance wgpuInstance, WGPUDevice wgpuDevice, WGPUAdapter wgpuAdapter, WGPUQueue wgpuQueue);

	/*!
	 * \brief Create an OpenXR session
	 *
	 * \details Creates an OpenXR session using the provided instance and system ID.
	 *
	 * \param xrInstance The OpenXR instance
	 * \param xrSystemId The OpenXR system ID
	 * \return The created OpenXR session, or XR_NULL_HANDLE on failure
	 */
    XrSession xrwgpuCreateSession(XrInstance xrInstance, XrSystemId xrSystemId);

	/*!
	 * \brief Create an OpenXR swapchain
	 *
	 * \details Creates an OpenXR swapchain with the specified parameters.
	 *
	 * \param session The OpenXR session
	 * \param wgpuFormat The WebGPU texture format
	 * \param nativeFormat The native format for the swapchain
	 * \param width The width of the swapchain images
	 * \param height The height of the swapchain images
	 * \param viewCount The number of views (e.g., for stereo rendering)
	 */
    void xrwgpuCreateSwapchain(XrSession session, WGPUTextureFormat wgpuFormat, int64_t nativeFormat, uint32_t width, uint32_t height, uint32_t viewCount);

	/*!
	 * \brief Get the OpenXR swapchain handle for a specific view
	 *
	 * \details Retrieves the OpenXR swapchain handle for the specified view index.
	 *
	 * \param viewIdx The index of the view
	 * \return The OpenXR swapchain handle
	 */
    XrSwapchain xrwgpuGetSwapchainHandle(uint32_t viewIdx) const;

	/*!
	 * \brief Acquire the next image for rendering
	 *
	 * \details Acquires the next image from the swapchain for rendering.
	 *
	 * \param viewIdx The index of the view
	 * \return The acquired WebGPU texture view
	 */
    WGPUTextureView xrwgpuAcquireNextImage(uint32_t viewIdx);

	/*!
	 * \brief Present the rendered image
	 *
	 * \details Presents the rendered image for the specified view.
	 *
	 * \param viewIdx The index of the view
	 */
    void xrwgpuPresent(uint32_t viewIdx);

private:
	struct DX12Internals;							///< Internal structure to hold DirectX 12 resources
	std::unique_ptr<DX12Internals> m_dx12;          ///< Unique pointer to the internal DirectX 12 resources
	WGPUDevice m_wgpuDevice = nullptr;              ///< The WebGPU device
	XrSession m_xrSession = XR_NULL_HANDLE;         ///< The OpenXR session
	std::vector<WGPUTexture> m_swapchainTextures;   ///< Vector of WebGPU textures for the swapchain
	std::vector<WGPUTextureView> m_swapchainViews;  ///< Vector of WebGPU texture views for the swapchain
	WGPUTexture m_renderTarget = nullptr;           ///< The WebGPU render target texture
	WGPUTextureView m_renderTargetView = nullptr;   ///< The WebGPU render target texture view
};
