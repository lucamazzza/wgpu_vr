cmake_minimum_required(VERSION 3.20)
project(wgpu_openxr_demos VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/glm)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/openxr)

add_library(wgpu_native SHARED IMPORTED)
set_target_properties(wgpu_native PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/external/wgpu/include"
)

if(WIN64)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION_DEBUG "${CMAKE_SOURCE_DIR}/external/wgpu/lib/x64/debug/wgpu_native.dll"
        IMPORTED_IMPLIB_DEBUG "${CMAKE_SOURCE_DIR}/external/wgpu/lib/x64/debug/wgpu_native.lib"

        IMPORTED_LOCATION_RELEASE "${CMAKE_SOURCE_DIR}/external/wgpu/lib/x64/release/wgpu_native.dll"
        IMPORTED_IMPLIB_RELEASE "${CMAKE_SOURCE_DIR}/external/wgpu/lib/x64/debug/wgpu_native.lib"
    )
elseif(APPLE)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION_DEBUG "${CMAKE_SOURCE_DIR}/external/wgpu/lib/macos/debug/libwgpu_native.dylib"
        IMPORTED_LOCATION_RELEASE "${CMAKE_SOURCE_DIR}/external/wgpu/lib/macos/release/libwgpu_native.dylib"
    )
else()
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION_DEBUG "${CMAKE_SOURCE_DIR}/external/wgpu/lib/linux/debug/libwgpu_native.so"
        IMPORTED_LOCATION_RELEASE "${CMAKE_SOURCE_DIR}/external/wgpu/lib/linux/release/libwgpu_native.so"
    )
endif()

add_subdirectory(main)
