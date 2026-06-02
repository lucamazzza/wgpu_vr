set(TARGET_NAME "demo_main")

add_executable(${TARGET_NAME} main.cpp)

target_link_libraries(${TARGET_NAME} PRIVATE
    wgpu_native
    openxr_loader
    glfw
    glm::glm
)

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shader.wgsl")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/shader.wgsl"
        "${CMAKE_CURRENT_BINARY_DIR}/shader.wgsl"
        COPYONLY
    )
endif()
