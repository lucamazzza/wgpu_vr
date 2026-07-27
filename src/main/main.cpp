/**
    \file main.cpp
    \brief Main entry point for the application.

    \author Luca Mazza
    \copyright 2026 Luca Mazza
*/

// Internal includes
#include "openxr/openxr.h"
#include "shader.h"
#include "buffer.h"
#include "XrWGPUBridge.h"

// Includes for GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Includes for WebGPU
#include <webgpu.h>
#include <wgpu.h>

// Includes for DirectX 12
#include <dxgiformat.h>

// Include for stb_image
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Includes for standard libraries
#include <chrono>
#include <iostream>
#include <string>
#include <thread>


struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uvs;
};

struct MatricesUniforms {
    glm::mat4 projection;
    glm::mat4 modelview;
    glm::mat4 normal;
};

struct LightMaterialUniforms {
    glm::vec4 lightPosition;
    glm::vec4 lightAmbient;
    glm::vec4 lightDiffuse;
    glm::vec4 lightSpecular;
    glm::vec4 matAmbient;
    glm::vec4 matDiffuse;
    glm::vec4 matSpecular;
    float matShininess;
    float padding[3];
};

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

int main() {
    // WebGPU Base Initialization
    WGPUInstanceExtras instanceExtras = {};
    instanceExtras.chain.next = nullptr;
    instanceExtras.chain.sType = (WGPUSType) WGPUSType_InstanceExtras;
    instanceExtras.backends = WGPUInstanceBackend_DX12;
    WGPUInstanceDescriptor instDesc = {};
    instDesc.nextInChain = &instanceExtras.chain;
    WGPUInstance instance = wgpuCreateInstance(&instDesc);

    // Adapter Request (GPU)
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.backendType = WGPUBackendType_D3D12;
    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterCallbackInfo adapterCbInfo = {
        .nextInChain = nullptr,
        .callback =
        [](WGPURequestAdapterStatus status, WGPUAdapter res, WGPUStringView msg, void* userdata1, void* userdata2) {
            if (status == WGPURequestAdapterStatus_Success) {
                *(WGPUAdapter*)userdata1 = res;
            } else {
                std::string errMsg(msg.data, msg.length);
                std::cerr << "Adapter Error: " << errMsg << std::endl;
            }
        }
    };
    adapterCbInfo.userdata1 = &adapter;
    wgpuInstanceRequestAdapter(instance, &adapterOpts, adapterCbInfo);

    // Device Request (Logical Interface)
    WGPUDeviceDescriptor deviceDesc = {};
    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo deviceCbInfo = {
        .nextInChain = nullptr,
        .callback = [](WGPURequestDeviceStatus status, WGPUDevice res, WGPUStringView msg, void* userdata1, void* userdata2) {
            if (status == WGPURequestDeviceStatus_Success) {
                *(WGPUDevice*)userdata1 = res;
            } else {
                std::string errMsg(msg.data, msg.length);
                std::cerr << "Device Error: " << errMsg << std::endl;
            }
        }
    };
    deviceCbInfo.userdata1 = &device;
    wgpuAdapterRequestDevice(adapter, &deviceDesc, deviceCbInfo);
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // OpenXR Initialization
    XrInstanceCreateInfo xrInstanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(xrInstanceInfo.applicationInfo.applicationName, "MAIN_Demo");
    xrInstanceInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    // -- Enable D3D12 Extension
    const char *extensions[] = {"XR_KHR_D3D12_enable"};
    xrInstanceInfo.enabledExtensionCount = 1;
    xrInstanceInfo.enabledExtensionNames = extensions;
    XrInstance xrInstance;
    if (XR_FAILED(xrCreateInstance(&xrInstanceInfo, &xrInstance))) {
        std::cerr << "No OpenXR runtime found!" << std::endl;
        return -1;
    }
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId;
    if (XR_FAILED(xrGetSystem(xrInstance, &systemInfo, &systemId))) {
        std::cerr << "No VR Headset found!" << std::endl;
        return -1;
    }

    // XrWGPUBridge
    XrWGPUBridge bridge;
    if (!bridge.xrwgpuInitialize(instance, device, adapter, queue)) {
        return -1;
    }
    XrSession session = bridge.xrwgpuCreateSession(xrInstance, systemId);
    if (session == XR_NULL_HANDLE) return -1;
    uint32_t width = 1440;
    uint32_t height = 1600;
    int64_t swapchainNativeFormat = static_cast<int64_t>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
    bridge.xrwgpuCreateSwapchain(
        session,
        WGPUTextureFormat_BGRA8UnormSrgb,
        swapchainNativeFormat,
        width,
        height,
        2);
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    xrAttachSessionActionSets(session, &attachInfo);

	std::vector<XrSwapchain> swapchains;

	// XrSpace Creation
    XrSpace space = XR_NULL_HANDLE;
    XrReferenceSpaceCreateInfo referenceSpaceInfo = {};
    referenceSpaceInfo.type = XrStructureType::XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    referenceSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    referenceSpaceInfo.poseInReferenceSpace = {
		{ 0.0f, 0.0f, 0.0f, 1.0f, }, // Orientation (Quaternion)
        { 0.0f, 0.0f, 0.0f },        // Position
    };
    if (XR_FAILED(xrCreateReferenceSpace(session, &referenceSpaceInfo, &space))) {
        std::cerr << "Failed to create reference space!" << std::endl;
    }

    // Shader Loading
    Shader shader;
    shader.loadFromFile(device, "shader.wgsl");

    // Buffer Creation
    float size = 128.0f;
    std::vector<Vertex> vertices = {
        {{-size, 0.0f, -size}, {0.0f, 1.0f, 0.0f}, {0.0f,  0.0f }},
        {{-size, 0.0f,  size}, {0.0f, 1.0f, 0.0f}, {0.0f,  50.0f}},
        {{ size, 0.0f, -size}, {0.0f, 1.0f, 0.0f}, {50.0f, 0.0f }},
        {{ size, 0.0f,  size}, {0.0f, 1.0f, 0.0f}, {50.0f, 50.0f}}
    };
    WgpuBuffer vertexBuffer;
    vertexBuffer.create(device, queue, vertices.data(), sizeof(Vertex) * vertices.size(), WGPUBufferUsage_Vertex);
    WgpuBuffer matricesBuffer;
    matricesBuffer.create(device, queue, nullptr, sizeof(MatricesUniforms), WGPUBufferUsage_Uniform);
    LightMaterialUniforms lightMat = {
        .lightAmbient = glm::vec4(1.0f),
        .lightDiffuse = glm::vec4(1.0f),
        .lightSpecular = glm::vec4(1.0f),
        .matAmbient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f),
        .matDiffuse = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f),
        .matSpecular = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f),
        .matShininess = 128.0f
    };
    WgpuBuffer lightBuffer;
    lightBuffer.create(device, queue, &lightMat, sizeof(LightMaterialUniforms), WGPUBufferUsage_Uniform);

    // Texture Loading
    int texWidth, texHeight, texChannels;
    unsigned char* rawData = stbi_load("bricks.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!rawData) {
        std::cerr << "[ERROR] Impossible to load bricks.jpg\n";
        return -1;
    }
    WGPUTextureDescriptor texDesc = {
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {(uint32_t)texWidth, (uint32_t)texHeight, 1},
        .format = WGPUTextureFormat_BGRA8UnormSrgb,
        .mipLevelCount = 1,
        .sampleCount = 1
    };
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    WGPURenderPipelineDescriptor pipelineDesc = {};
    uint32_t bytesPerRow = (texWidth * 4 + 255) & ~255;
    std::vector<uint8_t> paddedData(bytesPerRow * texHeight, 0);
    for (int y = 0; y < texHeight; ++y) {
        memcpy(&paddedData[y * bytesPerRow], &rawData[y * texWidth * 4], texWidth * 4);
    }
    stbi_image_free(rawData);
    WGPUTexelCopyTextureInfo copyTex = {
        .texture = texture
    };
    WGPUTexelCopyBufferLayout texLayout = {
        .offset = 0,
        .bytesPerRow = bytesPerRow,
        .rowsPerImage = (uint32_t)texHeight
    };
    wgpuQueueWriteTexture(queue, &copyTex, paddedData.data(), paddedData.size(), &texLayout, &texDesc.size);
    WGPUTextureViewDescriptor viewDesc = {
        .format = WGPUTextureFormat_BGRA8UnormSrgb,
        .dimension = WGPUTextureViewDimension_2D,
        .mipLevelCount = 1,
        .arrayLayerCount = 1
    };
    WGPUTextureView textureView = wgpuTextureCreateView(texture, &viewDesc);
    WGPUSamplerDescriptor samplerDesc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .maxAnisotropy = 1
    };
    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &samplerDesc);
    WGPUTextureDescriptor depthTexDesc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {1440, 1600, 1},
        .format = WGPUTextureFormat_Depth24Plus,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    WGPUTexture depthTexture = wgpuDeviceCreateTexture(device, &depthTexDesc);
    WGPUTextureViewDescriptor depthViewDesc = {
        .format = WGPUTextureFormat_Depth24Plus,
        .dimension = WGPUTextureViewDimension_2D,
        .mipLevelCount = 1,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_DepthOnly
    };
    WGPUTextureView depthTextureView = wgpuTextureCreateView(depthTexture, &depthViewDesc);

    // Vertex shader
    pipelineDesc.vertex.module = shader.getModule();
    pipelineDesc.vertex.entryPoint = WGPUStringView{ "vs_main", WGPU_STRLEN };
    pipelineDesc.vertex.bufferCount = 0;
    WGPUVertexAttribute attribs[3] = {
        {.format = WGPUVertexFormat_Float32x3, .offset = offsetof(Vertex, position), .shaderLocation = 0 },
        {.format = WGPUVertexFormat_Float32x3, .offset = offsetof(Vertex, normal),   .shaderLocation = 1 },
        {.format = WGPUVertexFormat_Float32x2, .offset = offsetof(Vertex, uvs),      .shaderLocation = 2 }
    };
    WGPUVertexBufferLayout vertexLayout = {
        .stepMode = WGPUVertexStepMode_Vertex,
        .arrayStride = sizeof(Vertex),
        .attributeCount = 3,
        .attributes = attribs
    };
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexLayout;

    // Fragment shader
    WGPUColorTargetState colorTarget = {
        .format = WGPUTextureFormat_BGRA8UnormSrgb,
        .writeMask = WGPUColorWriteMask_All
    };
    WGPUFragmentState fragmentState = {
        .module = shader.getModule(),
        .entryPoint = WGPUStringView { "fs_main", WGPU_STRLEN },
        .targetCount = 1,
        .targets = &colorTarget
    };
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    WGPUDepthStencilState depthStencil = {
        .format = WGPUTextureFormat_Depth24Plus,
        .depthWriteEnabled = WGPUOptionalBool_True,
        .depthCompare = WGPUCompareFunction_Less
    };
    pipelineDesc.depthStencil = &depthStencil;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.layout = nullptr;
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    // Bind Groups
    WGPUBindGroupEntry bg0Entries[2] = {
        {.binding = 0, .buffer = matricesBuffer.get(), .size = sizeof(MatricesUniforms) },
        {.binding = 1, .buffer = lightBuffer.get(), .size = sizeof(LightMaterialUniforms) }
    };
    WGPUBindGroupDescriptor bg0Desc = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0),
        .entryCount = 2,
        .entries = bg0Entries
    };
    WGPUBindGroup bindGroup0 = wgpuDeviceCreateBindGroup(device, &bg0Desc);
    WGPUBindGroupEntry bg1Entries[2] = {
        {.binding = 0, .sampler = sampler },
        {.binding = 1, .textureView = textureView }
    };
    WGPUBindGroupDescriptor bg1Desc = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 1),
        .entryCount = 2,
        .entries = bg1Entries
    };
    WGPUBindGroup bindGroup1 = wgpuDeviceCreateBindGroup(device, &bg1Desc);

    // Render Loop
    float time = 0.0f;
    bool isRunning = true;
    bool isSessionRunning = false;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    while (isRunning) {

		// Poll OpenXR events
        XrEventDataBuffer eventData{ XR_TYPE_EVENT_DATA_BUFFER };
        while (xrPollEvent(xrInstance, &eventData) == XR_SUCCESS) {
            if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                auto* stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                sessionState = stateEvent->state;
                std::cout << "OpenXR Session State Changed: " << sessionState << std::endl;
                if (sessionState == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (XR_SUCCEEDED(xrBeginSession(session, &beginInfo))) {
                        isSessionRunning = true;
                    }
                }
                else if (sessionState == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(session);
                    isSessionRunning = false;
                }
                else if (sessionState == XR_SESSION_STATE_EXITING || sessionState == XR_SESSION_STATE_LOSS_PENDING) {
                    isRunning = false;
                }
            }
            eventData.type = XR_TYPE_EVENT_DATA_BUFFER;
        }
        if (!isRunning) break;
        if (!isSessionRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

		// Wait for the next frame
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        xrWaitFrame(session, &frameWaitInfo, &frameState);

		// Begin the frame
        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        xrBeginFrame(session, &frameBeginInfo);
        std::vector<XrCompositionLayerBaseHeader*> layers = {};
       
		// Check if the session is active
        const bool isSessionActive =
            sessionState == XrSessionState::XR_SESSION_STATE_SYNCHRONIZED ||
            sessionState == XrSessionState::XR_SESSION_STATE_VISIBLE ||
            sessionState == XrSessionState::XR_SESSION_STATE_FOCUSED;
        bool didRender = false;

		// Create the projection layer for rendering
        XrCompositionLayerProjection compositionLayerProjection = {};
        compositionLayerProjection.type = XrStructureType::XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        compositionLayerProjection.layerFlags = 0;
        compositionLayerProjection.space = space;
        compositionLayerProjection.viewCount = 0; // This will be filled up later.
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
            viewLocateInfo.space = space;

			// Get the number of views
            uint32_t viewCnt = 0;
            xrLocateViews(session, &viewLocateInfo, &viewState, 0, &viewCnt, nullptr);
            std::vector<XrView> views(viewCnt, { XrStructureType::XR_TYPE_VIEW });
            xrLocateViews(session, &viewLocateInfo, &viewState, viewCnt, &viewCnt, views.data());

            for (uint32_t viewIdx = 0; viewIdx < views.size(); ++viewIdx)
            {
                WGPUCommandEncoderDescriptor encoderDesc = {};
                WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

                const XrView& currentView = views.at(viewIdx);

                XrCompositionLayerProjectionView compositionLayerProjectionView = {};
                compositionLayerProjectionView.type = XrStructureType::XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                compositionLayerProjectionView.pose = currentView.pose;
                compositionLayerProjectionView.fov = currentView.fov;
                compositionLayerProjectionView.subImage.swapchain = bridge.xrwgpuGetSwapchainHandle(viewIdx);
                compositionLayerProjectionView.subImage.imageRect.offset.x = 0;
                compositionLayerProjectionView.subImage.imageRect.offset.y = 0;
                compositionLayerProjectionView.subImage.imageRect.extent.width = width;
                compositionLayerProjectionView.subImage.imageRect.extent.height = height;
                compositionLayerProjectionView.subImage.imageArrayIndex = 0;
                compositionLayerProjectionViews.push_back(compositionLayerProjectionView);

                // As specified by the OpenXR specification, the left eye has an index of 0 and the right eye an index of 1.
                // https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrViewConfigurationType.html
                const uint8_t eye = viewIdx == 0 ? /*Left*/ 0 : /*Right*/ 1;

                MatricesUniforms matrices = {};
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
                matrices.projection = createProjectionMatrix(currentView.fov, 0.1f, 1000.0f);
                matrices.modelview = view_matrix * model_matrix;
                matrices.normal = glm::inverseTranspose(matrices.modelview);

                wgpuQueueWriteBuffer(queue, matricesBuffer.get(), 0, &matrices, sizeof(MatricesUniforms));
                // matricesBuffer.update(queue, &matrices, sizeof(MatricesUniforms));

                lightMat.lightPosition = matrices.modelview * glm::vec4(sin(time) * 50.0f, 20.0f, cos(time) * 50.0f, 1.0f);
                wgpuQueueWriteBuffer(queue, lightBuffer.get(), 0, &lightMat, sizeof(LightMaterialUniforms));
                // lightBuffer.update(queue, &lightMat, sizeof(LightMaterialUniforms));

                WGPUTextureView nextImage = bridge.xrwgpuAcquireNextImage(viewIdx);

                WGPURenderPassColorAttachment colorAttachment = {
                    .view = nextImage,
                    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Store,
                    .clearValue = { 0.53, 0.81, 0.92, 1.0 }
                };
                WGPURenderPassDepthStencilAttachment depthAttachment = {
                    .view = depthTextureView,
                    .depthLoadOp = WGPULoadOp_Clear,
                    .depthStoreOp = WGPUStoreOp_Store,
                    .depthClearValue = 1.0f
                };
                WGPURenderPassDescriptor renderPassDesc = {
                    .colorAttachmentCount = 1,
                    .colorAttachments = &colorAttachment,
                    .depthStencilAttachment = &depthAttachment
                };

                WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
                wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
                wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup0, 0, nullptr);
                wgpuRenderPassEncoderSetBindGroup(renderPass, 1, bindGroup1, 0, nullptr);
                wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer.get(), 0, WGPU_WHOLE_SIZE);
                wgpuRenderPassEncoderDraw(renderPass, 4, 1, 0, 0);
                wgpuRenderPassEncoderEnd(renderPass);
                wgpuRenderPassEncoderRelease(renderPass);

                WGPUCommandBufferDescriptor cmdBufferDesc = {};
                WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
                wgpuQueueSubmit(queue, 1, &command);
                wgpuCommandBufferRelease(command);
                bridge.xrwgpuPresent(viewIdx);
            }

            compositionLayerProjection.viewCount = static_cast<uint32_t>(compositionLayerProjectionViews.size());
            compositionLayerProjection.views = compositionLayerProjectionViews.data();

            if (didRender) {
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&compositionLayerProjection));
            }

			// End the frame
            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.displayTime = frameState.predictedDisplayTime;
            frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
            frameEndInfo.layers = layers.data();
            xrEndFrame(session, &frameEndInfo);
            time += 0.016f;
        }
    }

    // WebGPU Cleanup
    wgpuRenderPipelineRelease(pipeline);
    wgpuBindGroupRelease(bindGroup0);
    wgpuBindGroupRelease(bindGroup1);
    wgpuTextureViewRelease(depthTextureView);
    wgpuTextureRelease(depthTexture);
    wgpuTextureViewRelease(textureView);
    wgpuSamplerRelease(sampler);
    wgpuTextureRelease(texture);

    return 0;
}
