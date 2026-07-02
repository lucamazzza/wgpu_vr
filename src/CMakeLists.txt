cmake_minimum_required(VERSION 3.20)
project(wgpu_openxr_demos VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
add_subdirectory(external/glm)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/openxr)

set(DAWN_SRC_DIR "C:/dev/dawn/dawn" CACHE PATH "Path to Dawn source directory")
set(DAWN_BUILD_DIR "${DAWN_SRC_DIR}/out/Release" CACHE PATH "Path to Dawn Ninja build directory")

add_executable(demo_main
    main.cpp
)

target_include_directories(demo_main PRIVATE
    ${DAWN_SRC_DIR}/include
    ${DAWN_SRC_DIR}/src
    ${DAWN_BUILD_DIR}/gen/include
    ${DAWN_BUILD_DIR}/gen/src
)

target_link_directories(demo_main PRIVATE 
    ${DAWN_BUILD_DIR}/obj/src/dawn
    ${DAWN_BUILD_DIR}/obj/src/dawn/native
)

target_link_libraries(demo_main PRIVATE
    webgpu_dawn
    dawn_native_static
    dawn_proc_static

    Vulkan::Vulkan
    glm::glm
    OpenXR::openxr_loader
)

target_compile_definitions(demo_main PRIVATE 
    XR_USE_GRAPHICS_API_VULKAN
    DAWN_ENABLE_BACKEND_VULKAN
)
