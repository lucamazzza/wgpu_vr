/**
    \file shader.h
    \brief A wrapper class for a WebGPU shader module.

    \author Luca Mazza
    \copyright 2026 Luca Mazza
*/
#pragma once

#include <webgpu.h>
#include <wgpu.h>

#include <shader.h>

/**
    \brief A wrapper class for a WebGPU shader module.

    \details This class provides a simple interface for loading and managing a
             WebGPU shader module. It handles the creation and destruction of
             the underlying WGPUShaderModule.
*/
class Shader {
public:

    /**
        \brief Constructor

        \details Initializes the Shader instance.
    */
    Shader();

    /**
        \brief Destructor

        \details Cleans up the Shader instance.
    */
    ~Shader();

    /**
        \brief Load a shader module from a file

        \details Loads a WebGPU shader module from the specified file path.

        \param device The WebGPU device
        \param filepath The path to the shader file
        \return true if the shader module was loaded successfully, false otherwise
    */
    bool loadFromFile(WGPUDevice device, const char *filepath);

    /**
        \brief Destroy the shader module

        \details Destroys the underlying WGPUShaderModule.
    */
    void destroy();

    /**
        \brief Get the underlying WGPUShaderModule

        \details Returns the underlying WGPUShaderModule.

        \return The WGPUShaderModule
    */
    WGPUShaderModule getModule() const { return m_module; }
private:
    WGPUShaderModule m_module; ///< The underlying WebGPU shader module
};
