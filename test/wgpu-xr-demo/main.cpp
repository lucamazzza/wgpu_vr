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
#include "XrBridge.h"

// Includes for GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Includes for WebGPU
#include <webgpu.h>
#include <wgpu.h>

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

    XrBridge bridge;

    //std::vector<XrSwapchain> swapchains;
	if (!bridge.Init(instance, device, adapter, queue)) {
		std::cerr << "[ERROR] Failed to initialize OpenXR session." << std::endl;
		return -1;
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

        isRunning = bridge.Update(isSessionRunning);

        if (!isRunning) break;
        if (!isSessionRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        isRunning = bridge.Render([&](const XrBridge::Eye eye, glm::mat4 projection, glm::mat4 modelview, glm::mat4 normal, WGPUTextureView nextImage) {
            MatricesUniforms matrices = {
                .projection = projection,
                .modelview = modelview,
                .normal = normal,
            };

            WGPUCommandEncoderDescriptor encoderDesc = {};
            WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

            wgpuQueueWriteBuffer(queue, matricesBuffer.get(), 0, &matrices, sizeof(MatricesUniforms));
            // matricesBuffer.update(queue, &matrices, sizeof(MatricesUniforms));

            lightMat.lightPosition = matrices.modelview * glm::vec4(sin(time) * 50.0f, 20.0f, cos(time) * 50.0f, 1.0f);
            wgpuQueueWriteBuffer(queue, lightBuffer.get(), 0, &lightMat, sizeof(LightMaterialUniforms));
            // lightBuffer.update(queue, &lightMat, sizeof(LightMaterialUniforms));

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
            });
        time += 0.016f;
    }
    bridge.Free();

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
