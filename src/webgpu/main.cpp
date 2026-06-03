#include <cstring>
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

std::string LoadShader(const char *fpath) {
    std::ifstream file(fpath);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open file " << fpath << std::endl;
        exit(-1);
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

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
    WGPUSurfaceSourceWindowsHWND hwndDesc = {};
    hwndDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    hwndDesc.hinstance = GetModuleHandle(NULL);
    hwndDesc.hwnd = glfwGetWin32Window(window);
    surfaceDesc.nextInChain = (WGPUChainedStruct*)&hwndDesc;
#elif defined (__linux__)
    char *envSession = std::getenv("XDG_SESSION_TYPE");
    std::string windowingSystem = envSession ? envSession : "x11";
    if (windowingSystem == "x11") {
        WGPUSurfaceSourceXlibWindow x11Desc = {};
        x11Desc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        x11Desc.display = glfwGetX11Display();
        x11Desc.window = glfwGetX11Window(window);
        surfaceDesc.nextInChain = (WGPUChainedStruct*)&x11Desc;
    } else if (windowingSystem == "wayland") {
        WGPUSurfaceSourceWaylandSurface waylandDesc = {};
        waylandDesc.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        waylandDesc.display = glfwGetWaylandDisplay();
        waylandDesc.surface = glfwGetWaylandWindow(window);
        surfaceDesc.nextInChain = (WGPUChainedStruct*)&waylandDesc;
    }
#elif defined (__APPLE__)
    // The following lines use ObjectiveC to ensure the presence of a Metal Layer in the NSWindow
    // glfwGetCocoaWindow is not sufficient, as it does not ensure that.

    // objc
    id nsWindow = glfwGetCocoaWindow(window);
    id nsView = ((id(*)(id, SEL))objc_msgSend)(nsWindow, sel_registerName("contentView"));
    id caMetalLayerClass = (id)objc_getClass("CAMetalLayer");
    id metalLayer = ((id(*)(id, SEL))objc_msgSend)(caMetalLayerClass, sel_registerName("layer"));
    ((void(*)(id, SEL, bool))objc_msgSend)(nsView, sel_registerName("setWantsLayer:"), true);
    ((void(*)(id, SEL, id))objc_msgSend)(nsView, sel_registerName("setLayer:"), metalLayer);
    // end objc

    WGPUSurfaceSourceMetalLayer metalDesc = {};
    metalDesc.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    metalDesc.layer = metalLayer;
    surfaceDesc.nextInChain = (WGPUChainedStruct*)&metalDesc;
#endif
    return wgpuInstanceCreateSurface(instance, &surfaceDesc);
}

int main() {
    // Window Initialization
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "WebGPU Demo", nullptr, nullptr);

    // WebGPU Base Initialization
    WGPUInstanceDescriptor instDesc = {};
    WGPUInstance instance = wgpuCreateInstance(&instDesc);
    WGPUSurface surface = GetSurfaceFromGLFW(instance, window);

    // Adapter Request (GPU)
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = surface;
    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterCallbackInfo adapterCbInfo = {};
    adapterCbInfo.nextInChain = nullptr;
    adapterCbInfo.callback =
    [](WGPURequestAdapterStatus status, WGPUAdapter res, WGPUStringView msg, void *userdata1, void *userdata2) {
        if (status == WGPURequestAdapterStatus_Success) {
            *(WGPUAdapter*)userdata1 = res;
        } else {
            std::string errMsg(msg.data, msg.length);
            std::cerr << "Adapter Error: " << errMsg << std::endl;
        }
    };
    adapterCbInfo.userdata1 = &adapter;
    wgpuInstanceRequestAdapter(instance, &adapterOpts, adapterCbInfo);

    // Device Request (Logical Interface)
    WGPUDeviceDescriptor deviceDesc = {};
    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo deviceCbInfo = {};
    deviceCbInfo.nextInChain = nullptr;
    deviceCbInfo.callback =
    [](WGPURequestDeviceStatus status, WGPUDevice res, WGPUStringView msg, void *userdata1, void *userdata2) {
        if (status == WGPURequestDeviceStatus_Success) {
            *(WGPUDevice*)userdata1 = res;
        } else {
            std::string errMsg(msg.data, msg.length);
            std::cerr << "Device Error: " << errMsg << std::endl;
        }
    };
    deviceCbInfo.userdata1 = &device;
    wgpuAdapterRequestDevice(adapter, &deviceDesc, deviceCbInfo);

    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // Surface Configuration
    WGPUSurfaceConfiguration surfaceConfig = {};
    surfaceConfig.device = device;
    surfaceConfig.format = WGPUTextureFormat_BGRA8Unorm;
    surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
    surfaceConfig.viewFormatCount = 0;
    surfaceConfig.viewFormats = nullptr;
    surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
    surfaceConfig.width = 800;
    surfaceConfig.height = 600;
    surfaceConfig.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &surfaceConfig);

    // Load & Compile Shader
    std::string shaderData = LoadShader("shader.wgsl");
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = WGPUStringView{ shaderData.data(), shaderData.length() };
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &wgslDesc.chain;
    shaderDesc.label = WGPUStringView("Shader", 6);
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    WGPURenderPipelineDescriptor pipelineDesc = {};

    // Vertex shader
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = WGPUStringView{ "vs_main", 7 };
    pipelineDesc.vertex.bufferCount = 0;

    // Fragment shader
    WGPUBlendState blendState = {};
    blendState.color.srcFactor = WGPUBlendFactor_One;
    blendState.color.dstFactor = WGPUBlendFactor_Zero;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_One;
    blendState.alpha.dstFactor = WGPUBlendFactor_Zero;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = WGPUStringView{ "fs_main", 7 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;

    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;

    WGPUPipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.nextInChain = nullptr;
    layoutDesc.bindGroupLayoutCount = 0;
    layoutDesc.bindGroupLayouts = nullptr;

    pipelineDesc.layout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    // Render Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        WGPUSurfaceTexture surfaceTexture;
        wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
        if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
            continue;
        }

        WGPUTextureViewDescriptor viewDesc = {};
        viewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
        viewDesc.dimension = WGPUTextureViewDimension_2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = 1;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = WGPUTextureAspect_All;
        WGPUTextureView nextTextureView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

        WGPUCommandEncoderDescriptor encoderDesc = {};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

        WGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.view = nextTextureView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = WGPUColor{ 0.05, 0.05, 0.05, 1.0 };
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

        WGPURenderPassDescriptor renderPassDesc = {};
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachment;

        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
        wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
        wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);

        WGPUCommandBufferDescriptor cmdBufferDesc = {};
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
        wgpuQueueSubmit(queue, 1, &command);

        wgpuCommandBufferRelease(command);
        wgpuTextureViewRelease(nextTextureView);
        wgpuSurfacePresent(surface);
    }

    wgpuSurfaceUnconfigure(surface);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(shaderModule);
    wgpuQueueRelease(queue);
    wgpuAdapterRelease(adapter);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
