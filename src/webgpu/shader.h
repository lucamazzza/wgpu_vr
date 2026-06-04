#pragma once

#include <webgpu.h>
#include <wgpu.h>

#include <shader.h>

class Shader {
public:
    Shader();
    ~Shader();
    bool loadFromFile(WGPUDevice device, const char *filepath);
    void destroy();
    WGPUShaderModule getModule() const { return m_module; }
private:
    WGPUShaderModule m_module;
};
