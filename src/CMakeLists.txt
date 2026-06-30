cmake_minimum_required(VERSION 3.20)
project(wgpu_openxr_demos VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenXR REQUIRED)

set(DAWN_SRC_DIR "C:/dev/dawn/dawn" CACHE PATH "Path to Dawn source directory")
set(DAWN_BUILD_DIR "${DAWN_SRC_DIR}/out/Release" CACGHE PATH "Path to Dawn Ninja build directory")

add_executable(demo_main
    main.cpp
)

target_include_directories(demo_main PRIVATE
    ${DAWN_SRC_DIR}/include
    ${DAWN_SRC_DIR}/src
    ${DAWN_BUILD_DIR}/gen/include
)

target_link_directories(demo_main PRIVATE ${DAWN_BUILD_DIR})

target_link_libraries(demo_main PRIVATE
    webgpu_dawn
    dawn_native
    dawn_proc

    Vulkan::Vulkan
    glfw
    glm::glm
    OpenXR::openxr_loader
)

target_compile_definitions(demo_main PRIVATE 
    XR_USE_GRAPHICS_API_VULKAN
    DAWN_ENABLE_BACKEND_VULKAN
)
