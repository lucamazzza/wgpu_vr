/**
    \file shader.cpp
    \brief Implementation of the Shader class.

    \author Luca Mazza
    \copyright 2026 Luca Mazza
*/
#include "shader.h"

#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader() : m_module(nullptr) {}

Shader::~Shader() { destroy(); }

void Shader::destroy() {
    if (m_module) {
        wgpuShaderModuleRelease(m_module);
        m_module = nullptr;
    }
}

bool Shader::loadFromFile(WGPUDevice device, const char *filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Unable to open shader: " << filepath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = WGPUStringView{ code.data(), code.length() };
    WGPUShaderModuleDescriptor desc = {};
    desc.nextInChain = &wgslDesc.chain;
    m_module = wgpuDeviceCreateShaderModule(device, &desc);
    if (!m_module) {
        std::cerr << "[ERROR] Failed to compile shader: " << filepath << std::endl;
        return false;
    }
    return true;
}
