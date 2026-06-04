#include "shader.h"
#include "buffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <webgpu.h>
#include <wgpu.h>
#include <GLFW/glfw3.h>

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>

#if defined(__APPLE__)
    #include <objc/message.h>
    #include <objc/runtime.h>
#endif

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

struct WindowContext {
    bool resized = false;
    int width = 800;
    int height = 600;
};

/* Retrieves the Surface from the system's window manager

   This is very important, for these reasons:
      * WebGPU does not know anything about the system per-se, because it runs on top of the system's API
        and is designed for the web, it does not know what the surface of a window should be.
      * This work was before done by the OpenGL system driver, now that we don't directly rely on something
        that has the same structure in every operating system, we have to do the manual labour.

   This is the real price of WebGPU true multiplatformness, given that every window manager is different
   we have to adapt.

   For now these are supported:
      * Windows => HWND "Handle for a WiNDow"
      * Linux   => x11 and Wayland
      * macOS   => NSWindow with strict presence of a CAMetalLayer
*/
WGPUSurface GetSurfaceFromGLFW(WGPUInstance instance, GLFWwindow *window) {
    WGPUSurfaceDescriptor surfaceDesc= {};
#if defined(_WIN32)
    WGPUSurfaceSourceWindowsHWND hwndDesc = {
        .chain = {
            .sType = WGPUSType_SurfaceSourceWindowsHWND
        },
        .hinstance = GetModuleHandle(NULL),
        .hwnd = glfwGetWin32Window(window)
    };
    surfaceDesc.nextInChain = (WGPUChainedStruct*)&hwndDesc;
#elif defined (__linux__)
    char *envSession = std::getenv("XDG_SESSION_TYPE");
    std::string windowingSystem = envSession ? envSession : "x11";
    if (windowingSystem == "x11") {
        WGPUSurfaceSourceXlibWindow x11Desc = {
            .chain = { 
                .sType = WGPUSType_SurfaceSourceXlibWindow
            },
            .display = glfwGetX11Display(),
            .window = glfwGetX11Window(window)
        };
        surfaceDesc.nextInChain = (WGPUChainedStruct*)&x11Desc;
    } else if (windowingSystem == "wayland") {
        WGPUSurfaceSourceWaylandSurface waylandDesc = {
            .chain = {
                .sType = WGPUSType_SurfaceSourceWaylandSurface
            },
            .display = glfwGetWaylandDisplay(),
            .surface = glfwGetWaylandWindow(window)
        };
        surfaceDesc.nextInChain = (WGPUChainedStruct*)&waylandDesc;
    }
#elif defined(__APPLE__)
    // The following lines use ObjectiveC to ensure the presence of a Metal Layer in the NSWindow
    // glfwGetCocoaWindow is not sufficient, as it does not ensure that.

    // objc
    id nsWindow = glfwGetCocoaWindow(window);
    id nsView = ((id(*)(id, SEL))objc_msgSend)(nsWindow, sel_registerName("contentView"));
    id caMetalLayerClass = (id)objc_getClass("CAMetalLayer");
    id metalLayer = ((id(*)(id, SEL))objc_msgSend)(caMetalLayerClass, sel_registerName("layer"));
    ((void(*)(id, SEL, bool))objc_msgSend)(nsView, sel_registerName("setWantsLayer:"), true);
    ((void(*)(id, SEL, id))objc_msgSend)(nsView, sel_registerName("setLayer:"), metalLayer);
    ((void(*)(id, SEL, bool))objc_msgSend)(metalLayer, sel_registerName("setDisplaySyncEnabled:"), false);
    // end objc

    WGPUSurfaceSourceMetalLayer metalDesc = {
        .chain = {
            .sType = WGPUSType_SurfaceSourceMetalLayer
        },
        .layer = metalLayer
    };
    surfaceDesc.nextInChain = (WGPUChainedStruct*)&metalDesc;
#endif
    return wgpuInstanceCreateSurface(instance, &surfaceDesc);
}

int main() {
    // Window Initialization
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "WebGPU Demo", nullptr, nullptr);
    WindowContext winCtx;
    glfwSetWindowUserPointer(window, &winCtx);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
        if (width == 0 || height == 0) return; // Ignora se la finestra è minimizzata
        WindowContext* ctx = (WindowContext*)glfwGetWindowUserPointer(w);
        ctx->resized = true;
        ctx->width = width;
        ctx->height = height;
    });
    double lastTime = glfwGetTime();
    int nbFrames = 0;

    // WebGPU Base Initialization
    WGPUInstanceDescriptor instDesc = {};
    WGPUInstance instance = wgpuCreateInstance(&instDesc);
    WGPUSurface surface = GetSurfaceFromGLFW(instance, window);

    // Adapter Request (GPU)
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = surface;
    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterCallbackInfo adapterCbInfo = {
        .nextInChain = nullptr,
        .callback =
        [](WGPURequestAdapterStatus status, WGPUAdapter res, WGPUStringView msg, void *userdata1, void *userdata2) {
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
        .callback = [](WGPURequestDeviceStatus status, WGPUDevice res, WGPUStringView msg, void *userdata1, void *userdata2) {
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

    // Surface Configuration
    WGPUSurfaceConfiguration surfaceConfig = {
        .device = device,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .usage = WGPUTextureUsage_RenderAttachment,
        .width = 800,
        .height = 600,
        .viewFormatCount = 0,
        .viewFormats = nullptr,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .presentMode = WGPUPresentMode_Immediate
    };
    wgpuSurfaceConfigure(surface, &surfaceConfig);

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
        .lightAmbient  = glm::vec4(1.0f),
        .lightDiffuse  = glm::vec4(1.0f),
        .lightSpecular = glm::vec4(1.0f),
        .matAmbient    = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f),
        .matDiffuse    = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f),
        .matSpecular   = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f),
        .matShininess  = 128.0f
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
        { .format = WGPUVertexFormat_Float32x3, .offset = offsetof(Vertex, position), .shaderLocation = 0 },
        { .format = WGPUVertexFormat_Float32x3, .offset = offsetof(Vertex, normal),   .shaderLocation = 1 },
        { .format = WGPUVertexFormat_Float32x2, .offset = offsetof(Vertex, uvs),      .shaderLocation = 2 }
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
        { .binding = 0, .buffer = matricesBuffer.get(), .size = sizeof(MatricesUniforms) },
        { .binding = 1, .buffer = lightBuffer.get(), .size = sizeof(LightMaterialUniforms) }
    };
    WGPUBindGroupDescriptor bg0Desc = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0),
        .entryCount = 2,
        .entries = bg0Entries
    };
    WGPUBindGroup bindGroup0 = wgpuDeviceCreateBindGroup(device, &bg0Desc);
    WGPUBindGroupEntry bg1Entries[2] = {
        { .binding = 0, .sampler = sampler },
        { .binding = 1, .textureView = textureView }
    };
    WGPUBindGroupDescriptor bg1Desc = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 1),
        .entryCount = 2,
        .entries = bg1Entries
    };
    WGPUBindGroup bindGroup1 = wgpuDeviceCreateBindGroup(device, &bg1Desc);

    // Render Loop
    float time = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        time += 0.016f;
        double currentTime = glfwGetTime();
        nbFrames++;
        if (currentTime - lastTime >= 3.0) {
            std::string title = "WebGPU Demo - FPS: " + std::to_string(nbFrames);
            glfwSetWindowTitle(window, title.c_str());
            nbFrames = 0;
            lastTime += 1.0;
        }

        MatricesUniforms matrices = {};
        matrices.projection = glm::perspective(glm::radians(60.0f), 1024.0f / 512.0f, 0.1f, 1000.0f);
        matrices.modelview = glm::lookAt(glm::vec3(0, 20, 80), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        matrices.normal = glm::inverseTranspose(matrices.modelview);
        matricesBuffer.update(queue, &matrices, sizeof(MatricesUniforms));

        if (winCtx.resized) {
            surfaceConfig.width = winCtx.width;
            surfaceConfig.height = winCtx.height;
            wgpuSurfaceConfigure(surface, &surfaceConfig);
            wgpuTextureViewRelease(depthTextureView);
            wgpuTextureRelease(depthTexture);
            depthTexDesc.size = { (uint32_t)winCtx.width, (uint32_t)winCtx.height, 1 };
            depthTexture = wgpuDeviceCreateTexture(device, &depthTexDesc);
            depthTextureView = wgpuTextureCreateView(depthTexture, &depthViewDesc);
            matrices.projection = glm::perspective(glm::radians(60.0f), (float)winCtx.width / (float)winCtx.height, 0.1f, 1000.0f);
            winCtx.resized = false;
        }

        lightMat.lightPosition = matrices.modelview * glm::vec4(sin(time)*50.0f, 20.0f, cos(time)*50.0f, 1.0f);
        lightBuffer.update(queue, &lightMat, sizeof(LightMaterialUniforms));

        WGPUSurfaceTexture surfaceTexture;
        wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
        if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
            continue;
        }
        WGPUTextureViewDescriptor currentViewDesc = {
            .format = WGPUTextureFormat_BGRA8Unorm,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1
        };
        WGPUTextureView nextTextureView = wgpuTextureCreateView(surfaceTexture.texture, &currentViewDesc);
        WGPUCommandEncoderDescriptor encoderDesc = {};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

        WGPURenderPassColorAttachment colorAttachment = {
            .view = nextTextureView,
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
        wgpuTextureViewRelease(nextTextureView);
        wgpuSurfacePresent(surface);
    }

    wgpuRenderPipelineRelease(pipeline);
    wgpuBindGroupRelease(bindGroup0);
    wgpuBindGroupRelease(bindGroup1);
    wgpuTextureViewRelease(depthTextureView);
    wgpuTextureRelease(depthTexture);
    wgpuTextureViewRelease(textureView);
    wgpuSamplerRelease(sampler);
    wgpuTextureRelease(texture);
    glfwTerminate();

    return 0;
}
