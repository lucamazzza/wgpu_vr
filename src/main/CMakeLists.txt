set(TARGET_NAME "demo_main")

add_executable(${TARGET_NAME} 
    buffer.h
    buffer.cpp
    shader.h
    shader.cpp
    XrWGPUBridge.h
    XrWGPUBridge.cpp
    main.cpp

    stb_image.h
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external/vulkan/include
)

target_link_libraries(${TARGET_NAME} PRIVATE
    OpenXR::openxr_loader
    wgpu_native
    glfw
    glm::glm
)

if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE
        ws2_32
        userenv
        bcrypt
        ntdll
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shader.wgsl")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/shader.wgsl"
        "${CMAKE_CURRENT_BINARY_DIR}/shader.wgsl"
        COPYONLY
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bricks.jpg")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/bricks.jpg"
        "${CMAKE_CURRENT_BINARY_DIR}/bricks.jpg"
        COPYONLY
    )
endif()
