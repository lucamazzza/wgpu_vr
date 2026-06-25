#include "openxr/openxr.h"
#include "shader.h"
#include "buffer.h"
#include "XrWGPUBridge.h"

#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <thread>
#include <webgpu.h>
#include <wgpu.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <string>

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

int main() {

    // WebGPU Base Initialization
    WGPUInstanceDescriptor instDesc = {};
    WGPUInstance instance = wgpuCreateInstance(&instDesc);

    // Adapter Request (GPU)
    WGPURequestAdapterOptions adapterOpts = {};
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
    // -- Enable Vulkan Extension
    const char *extensions[] = {"XR_KHR_vulkan_enable"};
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
    if (!bridge.xrwgpuInitialize(instance, device, adapter)) {
        return -1;
    }
    XrSession session = bridge.xrwgpuCreateSession(xrInstance, systemId);
    if (session == XR_NULL_HANDLE) return -1;
    uint32_t width = 1440;
    uint32_t height = 1600;
    bridge.xrwgpuCreateSwapchain(session, WGPUTextureFormat_BGRA8Unorm, 43 /* VK_FORMAT_B8G8R8A8_SRGB*/, width, height);
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    xrAttachSessionActionSets(session, &attachInfo);
    XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    xrBeginSession(session, &beginInfo);

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
        .format = WGPUTextureFormat_RGBA8Unorm,
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
        .format = WGPUTextureFormat_RGBA8Unorm,
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
        .size = {800, 600, 1},
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
        .format = WGPUTextureFormat_BGRA8Unorm,
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
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    while (isRunning) {
        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(xrInstance, &eventData) == XR_SUCCESS) {
            if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                auto *stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                sessionState = stateEvent->state;
                std::cout << "OpenXR Session State Changed: " << sessionState << std::endl;
                if (sessionState == XR_SESSION_STATE_EXITING || sessionState == XR_SESSION_STATE_LOSS_PENDING) {
                    isRunning = false;
                }
            }
            eventData.type = XR_TYPE_EVENT_DATA_BUFFER;
        }
        if (!isRunning) break;
        if (sessionState != XR_SESSION_STATE_FOCUSED && sessionState != XR_SESSION_STATE_VISIBLE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        xrWaitFrame(session, &frameWaitInfo, &frameState);
        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        xrBeginFrame(session, &frameBeginInfo);

        if (frameState.shouldRender) {

            MatricesUniforms matrices = {};
            matrices.projection = glm::perspective(glm::radians(60.0f), 1024.0f / 512.0f, 0.1f, 1000.0f);
            matrices.modelview = glm::lookAt(glm::vec3(0, 20, 80), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            matrices.normal = glm::inverseTranspose(matrices.modelview);
            wgpuQueueWriteBuffer(queue, matricesBuffer.get(), 0, &matrices, sizeof(MatricesUniforms));
            // matricesBuffer.update(queue, &matrices, sizeof(MatricesUniforms));

            lightMat.lightPosition = matrices.modelview * glm::vec4(sin(time) * 50.0f, 20.0f, cos(time) * 50.0f, 1.0f);
            wgpuQueueWriteBuffer(queue, lightBuffer.get(), 0, &lightMat, sizeof(LightMaterialUniforms));
            // lightBuffer.update(queue, &lightMat, sizeof(LightMaterialUniforms));

            WGPUTextureView nextImage = bridge.xrwgpuAcquireNextImage();
            WGPUCommandEncoderDescriptor encoderDesc = {};
            WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

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
            bridge.present();
        }

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        frameEndInfo.layerCount = 0; // TODO: Set to stereo eyes layers
        frameEndInfo.layers = nullptr; 
        xrEndFrame(session, &frameEndInfo);
        time += 0.016f;
    }

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
