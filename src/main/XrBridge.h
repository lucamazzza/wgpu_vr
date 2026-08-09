/**
    \file XrBridge.h
    \brief OpenXR bridge header

    \author Luca Mazza
    \copyright 2026 Luca Mazza
*/
#pragma once

#include <openxr/openxr.h>
#include <glm/glm.hpp>
#include <wgpu.h>
#include <vector>
#include <functional>

#include "XrWGPUBridge.h"

/**
    \class XrBridge
    \brief Bridge that manages OpenXR instance, session, swapchains and integrates with the XrWGPUBridge for presentation.

    Responsible for creating the OpenXR instance and reference space, managing session state, preparing
    projection/view matrices and driving per-eye rendering via a supplied render callback.
*/
class XrBridge {
public:
    /**
        \brief Construct a new XrBridge and initialize default member values.
    */
    XrBridge();
    /**
        \brief Eye identifier for stereo rendering (left or right).
    */
    enum class Eye { Left, Right };
    /**
        \brief GPU-side buffers used by the renderer (matrices, lights, ...)
    */
    struct Buffers {
        WGPUBuffer matricesBuffer;
        WGPUBuffer lightsBuffer;
    };
    /**
        \brief Signature of the render callback invoked per-eye each frame.
        \param eye Which eye is being rendered (Left/Right)
        \param projection Projection matrix for the eye
        \param modelview View * Model matrix for the eye
        \param normal Normal matrix (inverse-transpose of modelview)
        \param nextImage WGPU texture view that should be rendered to
    */
    typedef std::function<void(const Eye eye, glm::mat4 projection, glm::mat4 modelview, glm::mat4 normal, WGPUTextureView nextImage)> RenderFunction_t;
    /**
        \brief Initialize OpenXR instance, system and create a session bound to the provided WebGPU (WGPU) objects.
        \param instance WebGPU instance
        \param device WebGPU device
        \param adapter WebGPU adapter
        \param queue WebGPU queue
        \return true on success, false on failure
    */
    bool Init(WGPUInstance instance, WGPUDevice device, WGPUAdapter adapter, WGPUQueue queue);
    /**
        \brief Release OpenXR resources (space, session, instance).
        \return true on success or if already freed, false on failure
    */
    bool Free();
    /**
        \brief Poll and process OpenXR events, updating session state.
        \param isSessionRunning Reference toggled to true when session becomes active.
        \return false when the runtime requests exit or loss pending, true otherwise
    */
    bool Update(bool &isSessionRunning);
    /**
        \brief Drive the per-frame OpenXR rendering flow and invoke the provided render callback per-eye.
        \param renderFunction Callback used to render each eye's view
        \return true on success
    */
    bool Render(const RenderFunction_t &renderFunction);
    /**
        \brief Set near and far clipping planes used for projection matrix generation.
    */
    void SetPlanes(float nearPlane, float farPlane) { m_nearPlane = nearPlane; m_farPlane = farPlane; }
private:

    uint32_t m_width;                       ///< Resolution width used when creating per-eye swapchain textures
    uint32_t m_height;                      ///< Resolution height used when creating per-eye swapchain textures
    float m_nearPlane;                      ///< Near clipping plane distance (meters)
    float m_farPlane;                       ///< Far clipping plane distance (meters)

    XrInstance m_instance;                  ///< OpenXR instance handle
    XrSystemId m_systemId;                  ///< OpenXR system id for the chosen form factor (HMD)
    XrSession m_session;                    ///< Active OpenXR session handle
    XrSessionState m_sessionState;          ///< Cached OpenXR session state
    std::vector<XrSwapchain> m_swapchains;  ///< Swapchain handles, one per view/eye
    XrSpace m_space;                        ///< Reference space used for view poses (stage/world tracking)
    XrWGPUBridge m_bridge;                  ///< Bridge that wraps WebGPU <-> OpenXR interop (swapchain creation, image acquisition/present)
};

