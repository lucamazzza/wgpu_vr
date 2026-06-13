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

if(WIN32)
    set(WGPU_NATIVE_DLL "${CMAKE_SOURCE_DIR}/external/wgpu/lib/x64/wgpu_native.dll")
    set(WGPU_NATIVE_IMPLIB "${CMAKE_SOURCE_DIR}/external/wgpu/lib/x64/wgpu_native.lib")
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_NATIVE_DLL}"
        IMPORTED_IMPLIB "${WGPU_NATIVE_IMPLIB}"
        IMPORTED_LOCATION_RELEASE "${WGPU_NATIVE_DLL}"
        IMPORTED_IMPLIB_RELEASE "${WGPU_NATIVE_IMPLIB}"
        IMPORTED_LOCATION_DEBUG "${WGPU_NATIVE_DLL}"
        IMPORTED_IMPLIB_DEBUG "${WGPU_NATIVE_IMPLIB}"
    )
elseif(APPLE)
    set(WGPU_NATIVE_DYLIB "${CMAKE_SOURCE_DIR}/external/wgpu/lib/macos/libwgpu_native.dylib")
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_NATIVE_DYLIB}"
        IMPORTED_LOCATION_RELEASE "${WGPU_NATIVE_DYLIB}"
        IMPORTED_LOCATION_DEBUG "${WGPU_NATIVE_DYLIB}"
    )
else()
    set(WGPU_NATIVE_SO "${CMAKE_SOURCE_DIR}/external/wgpu/lib/linux/libwgpu_native.so")
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_NATIVE_SO}"
        IMPORTED_LOCATION_RELEASE "${WGPU_NATIVE_SO}"
        IMPORTED_LOCATION_DEBUG "${WGPU_NATIVE_SO}"
    )
endif()

add_subdirectory(main)
add_subdirectory(openxr)
add_subdirectory(webgpu)
