set(TARGET_NAME "demo_wgpu")

add_executable(${TARGET_NAME} 
    buffer.h
    buffer.cpp
    shader.h
    shader.cpp
    main.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
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

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
