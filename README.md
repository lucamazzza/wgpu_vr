<p align='center'>

<img src="https://capsule-render.vercel.app/api?type=venom&color=005A9C&height=200&section=header&text=WebGPU%20for%20VR&fontSize=80&fontColor=ffffff&animation=fadeIn&fontAlignY=35&desc=Feasibility%20Study%20for%20WebGPU+OpenXR&descAlignY=61&descAlign=50"/>

</p>

This repository contains a C++ prototype demonstrating interoperability between **WebGPU** (via `wgpu_native`) and **OpenXR**.
The goal of this project is to provide a modern, cross-platform, and explicit graphics stack for Virtual Reality
applications, replacing legacy OpenGL and OpenVR workflows.

# Structure

* **`src/main/`**: The core thesis prototype. An interoperability layer bridging WebGPU rendering with the OpenXR swapchain.
* **`src/webgpu/`**: A standalone WebGPU application to test graphics and windowing without VR overhead.
* **`src/openxr/`**: A standalone OpenXR application to verify headset detection and runtime configuration without graphics overhead.
* **`src/external/`**: Vendored dependencies (included in the repo for easy building).

# Dependencies

Most dependencies are vendored in the `src/external/` directory, meaning that you do not need to install them system-wide.
The vendored libraries include:

* [wgpu_native](https://github.com/gfx-rs/wgpu-native) (WebGPU implementation)
* [OpenXR-SDK](https://github.com/KhronosGroup/OpenXR-SDK) (VR Runtime Loader)
* [GLFW](https://github.com/glfw/glfw) (Windowing)
* [GLM](https://github.com/g-truc/glm) (Mathematics)

### System Requirements to Build

* **CMake** (`v3.20` or higher)
* **C++20** compatible compiler (GCC, Clang, MSVC)
* An active OpenXR Runtime installed on your system (e.g. SteamVR, Meta Quest Link, Monado)
* *Linux only*: Wayland/x11 development headers

# How to build

This project uses standard CMake out-of-source builds. Open your terminal in the root directory of the project and run:

```bash
# Create a build
cmake -B build -S .

# Compile executables
cmake --build build
```

# Usage

After a successful build, the executables will be located in the `src/build`

```bash
# Test WebGPU (Monitor Only)
cd src/build/webgpu && ./demo_webgpu

# Test OpenXR
cd src/build/openxr && ./demo_openxr

# Test Main
cd src/build/main && ./demo_main
```

# License

This project is developed for SUPSI and therefore licensed to the istitution.
