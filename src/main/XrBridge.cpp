#include "XrBridge.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>

#ifdef _DEBUG
#define XRBRIDGE_DEBUG_OUT( message ) { std::cerr << "[XrBridge][DEBUG] " << message << std::endl; }
#else
#define XRBRIDGE_DEBUG_OUT( message ) {}
#endif

#define XRBRIDGE_ERROR_OUT( message ) { std::cerr << "[XrBridge][ERROR] " << message << std::endl; }
#define XRBRIDGE_WARNING_OUT( message ) { std::cerr << "[XrBridge][WARNING] " << message << std::endl; }


// Source: https://openxr-tutorial.com/linux/opengl/_downloads/f4aef9ec726fccc71e105bc0830d4ff3/xr_linear_algebra.h
// XrMatrix4x4f_CreateProjectionFov
// XrMatrix4x4f_CreateProjection
// We have to use a custom projection function because glm::projection does not handle
// fov the way we need it to.
static glm::mat4 createProjectionMatrix(const XrFovf fov, const float near_clipping_plane, const float far_clipping_plane) {
    const float tan_left = std::tanf(fov.angleLeft);
    const float tan_right = std::tanf(fov.angleRight);
    const float tan_down = std::tanf(fov.angleDown);
    const float tan_up = std::tanf(fov.angleUp);
    const float tan_width = tan_right - tan_left;
    const float tan_height = tan_up - tan_down;
    glm::mat4 result = glm::mat4(
        2.0f / tan_width,
        0.0f,
        0.0f,
        0.0f,

        0.0f,
        2.0f / tan_height,
        0.0f,
        0.0f,

        (tan_right + tan_left) / tan_width,
        (tan_up + tan_down) / tan_height,
        -(near_clipping_plane + far_clipping_plane) / (far_clipping_plane - near_clipping_plane),
        -1.0f,

        0.0f,
        0.0f,
        -(2.0f * near_clipping_plane * far_clipping_plane) / (far_clipping_plane - near_clipping_plane),
        0.0f
    );
    return result;
}

XrBridge::XrBridge()
    : m_width(1440)
    , m_height(1600)
    , m_nearPlane(0.1f)
    , m_farPlane(1000.0f)
    , m_instance(XR_NULL_HANDLE)
    , m_systemId(XR_NULL_SYSTEM_ID)
    , m_session(XR_NULL_HANDLE)
    , m_sessionState(XR_SESSION_STATE_UNKNOWN)
    , m_swapchains()
    , m_space(XR_NULL_HANDLE) {}

bool XrBridge::Init(WGPUInstance instance, WGPUDevice device, WGPUAdapter adapter, WGPUQueue queue) {
    XrInstanceCreateInfo xrInstanceInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy_s(xrInstanceInfo.applicationInfo.applicationName, "MAIN_Demo");
    xrInstanceInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    // -- Enable D3D12 Extension
    const char* extensions[] = { "XR_KHR_D3D12_enable" };
    xrInstanceInfo.enabledExtensionCount = 1;
    xrInstanceInfo.enabledExtensionNames = extensions;
    if (XR_FAILED(xrCreateInstance(&xrInstanceInfo, &m_instance))) {
        XRBRIDGE_ERROR_OUT("No OpenXR runtime found!");
        return false;
    }
    XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(m_instance, &systemInfo, &m_systemId))) {
        XRBRIDGE_ERROR_OUT("No VR Headset found!");
        return false;
    }

    if (!m_bridge.xrwgpuInitialize(instance, device, adapter, queue)) {
        return false;
    }
    m_session = m_bridge.xrwgpuCreateSession(m_instance, m_systemId);
    if (m_session == XR_NULL_HANDLE) return false;
    int64_t swapchainNativeFormat = static_cast<int64_t>(/*DXGI_FORMAT_B8G8R8A8_UNORM_SRGB*/91);
    m_bridge.xrwgpuCreateSwapchain(
        m_session,
        WGPUTextureFormat_BGRA8UnormSrgb,
        swapchainNativeFormat,
        m_width,
        m_height,
        2);
    XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    xrAttachSessionActionSets(m_session, &attachInfo);

    // XrSpace Creation
    XrReferenceSpaceCreateInfo referenceSpaceInfo = {};
    referenceSpaceInfo.type = XrStructureType::XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    referenceSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    referenceSpaceInfo.poseInReferenceSpace = {
        { 0.0f, 0.0f, 0.0f, 1.0f, }, // Orientation (Quaternion)
        { 0.0f, 0.0f, 0.0f },        // Position
    };
    if (XR_FAILED(xrCreateReferenceSpace(m_session, &referenceSpaceInfo, &m_space))) {
        XRBRIDGE_ERROR_OUT("Failed to create reference space!");
		return false;
    }
	return true;
}

bool XrBridge::Free() {
	if (m_space == XR_NULL_HANDLE && m_session == XR_NULL_HANDLE && m_instance == XR_NULL_HANDLE) {
        XRBRIDGE_ERROR_OUT("OpenXR resources are already freed!");
		return true;
	}
	if (xrDestroySpace(m_space) != XR_SUCCESS) {
		XRBRIDGE_ERROR_OUT("Failed to destroy reference space!");
		return false;
	}
	if (xrDestroySession(m_session) != XR_SUCCESS) {
		XRBRIDGE_ERROR_OUT("Failed to destroy OpenXR session!");
		return false;
	}
	if (xrDestroyInstance(m_instance) != XR_SUCCESS) {
		XRBRIDGE_ERROR_OUT("Failed to destroy OpenXR instance!");
		return false;
	}
	m_space = XR_NULL_HANDLE;
	m_session = XR_NULL_HANDLE;
	m_instance = XR_NULL_HANDLE;
	return true;
}

bool XrBridge::Update(bool& isSessionRunning) {
    XrEventDataBuffer eventData{ XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(m_instance, &eventData) == XR_SUCCESS) {
        if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
            m_sessionState = stateEvent->state;
            XRBRIDGE_DEBUG_OUT("OpenXR Session State Changed: " << m_sessionState);
            if (m_sessionState == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(xrBeginSession(m_session, &beginInfo))) {
                    isSessionRunning = true;
                }
            }
            else if (m_sessionState == XR_SESSION_STATE_STOPPING) {
                xrEndSession(m_session);
                isSessionRunning = false;
            }
            else if (m_sessionState == XR_SESSION_STATE_EXITING || m_sessionState == XR_SESSION_STATE_LOSS_PENDING) {
                return false;
            }
        }
        eventData.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
    return true;
}

bool XrBridge::Render(const RenderFunction_t& renderFunction) {
    // Wait for the next frame
    XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
    XrFrameState frameState{ XR_TYPE_FRAME_STATE };
    xrWaitFrame(m_session, &frameWaitInfo, &frameState);

    // Begin the frame
    XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
    xrBeginFrame(m_session, &frameBeginInfo);
    std::vector<XrCompositionLayerBaseHeader*> layers = {};

    // Check if the session is active
    const bool isSessionActive =
        m_sessionState == XrSessionState::XR_SESSION_STATE_SYNCHRONIZED ||
        m_sessionState == XrSessionState::XR_SESSION_STATE_VISIBLE ||
        m_sessionState == XrSessionState::XR_SESSION_STATE_FOCUSED;
    bool didRender = false;

    // Create the projection layer for rendering
    XrCompositionLayerProjection compositionLayerProjection = {};
    compositionLayerProjection.type = XrStructureType::XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    compositionLayerProjection.layerFlags = 0;
    compositionLayerProjection.space = m_space;
    compositionLayerProjection.viewCount = 0;   // This will be filled up later.
    compositionLayerProjection.views = nullptr; // This will be filled up later.
    std::vector<XrCompositionLayerProjectionView> compositionLayerProjectionViews = {};

    if (frameState.shouldRender) {
        didRender = true;

        // 3D view
        XrViewState viewState = {};
        viewState.type = XrStructureType::XR_TYPE_VIEW_STATE;
        XrViewLocateInfo viewLocateInfo = {};
        viewLocateInfo.type = XrStructureType::XR_TYPE_VIEW_LOCATE_INFO;
        viewLocateInfo.viewConfigurationType = XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = frameState.predictedDisplayTime;
        viewLocateInfo.space = m_space;

        // Get the number of views
        uint32_t viewCnt = 0;
        xrLocateViews(m_session, &viewLocateInfo, &viewState, 0, &viewCnt, nullptr);
        std::vector<XrView> views(viewCnt, { XrStructureType::XR_TYPE_VIEW });
        xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCnt, &viewCnt, views.data());

        for (uint32_t viewIdx = 0; viewIdx < views.size(); ++viewIdx)
        {
            const XrView& currentView = views.at(viewIdx);

            XrCompositionLayerProjectionView compositionLayerProjectionView = {};
            compositionLayerProjectionView.type = XrStructureType::XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            compositionLayerProjectionView.pose = currentView.pose;
            compositionLayerProjectionView.fov = currentView.fov;
            compositionLayerProjectionView.subImage.swapchain = m_bridge.xrwgpuGetSwapchainHandle(viewIdx);
            compositionLayerProjectionView.subImage.imageRect.offset.x = 0;
            compositionLayerProjectionView.subImage.imageRect.offset.y = 0;
            compositionLayerProjectionView.subImage.imageRect.extent.width = m_width;
            compositionLayerProjectionView.subImage.imageRect.extent.height = m_height;
            compositionLayerProjectionView.subImage.imageArrayIndex = 0;
            compositionLayerProjectionViews.push_back(compositionLayerProjectionView);

            // As specified by the OpenXR specification, the left eye has an index of 0 and the right eye an index of 1.
            // https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrViewConfigurationType.html
            const Eye eye = viewIdx == 0 ? Eye::Left : Eye::Right;

            glm::quat eyeOrientation(
                currentView.pose.orientation.w,
                currentView.pose.orientation.x,
                currentView.pose.orientation.y,
                currentView.pose.orientation.z
            );
            glm::vec3 eyePosition(
                currentView.pose.position.x,
                currentView.pose.position.y,
                currentView.pose.position.z
            );
            glm::mat4 rotation_matrix = glm::mat4_cast(eyeOrientation);
            glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), eyePosition);
            glm::mat4 eye_transform = translation_matrix * rotation_matrix;
            glm::mat4 view_matrix = glm::inverse(eye_transform);
            glm::mat4 model_matrix = glm::mat4(1.0f);
            glm::mat4 projection = createProjectionMatrix(currentView.fov, m_nearPlane, m_farPlane);
            glm::mat4 modelview = view_matrix * model_matrix;
            glm::mat4 normal = glm::inverseTranspose(modelview);

            renderFunction(eye, projection, modelview, normal, m_bridge.xrwgpuAcquireNextImage(viewIdx));

            m_bridge.xrwgpuPresent(viewIdx);
        }

        compositionLayerProjection.viewCount = static_cast<uint32_t>(compositionLayerProjectionViews.size());
        compositionLayerProjection.views = compositionLayerProjectionViews.data();

        if (didRender) {
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&compositionLayerProjection));
        }

    }
    // End the frame
    XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
    frameEndInfo.layers = layers.data();
    xrEndFrame(m_session, &frameEndInfo);
    return true;
}
