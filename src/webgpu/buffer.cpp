#include "buffer.h"

#include <iostream>

WgpuBuffer::WgpuBuffer() : m_buffer(nullptr), m_alignedSize(0) {}

WgpuBuffer::~WgpuBuffer() { destroy(); }

void WgpuBuffer::destroy() {
    if (m_buffer) {
        wgpuBufferRelease(m_buffer);
        m_buffer = nullptr;
    }
}

bool WgpuBuffer::create(WGPUDevice device, WGPUQueue queue, const void *data, size_t size, WGPUBufferUsage usage) {
    destroy();
    WGPUBufferDescriptor desc = {};
    desc.usage = usage | WGPUBufferUsage_CopyDst;
    desc.size = (size + 3) & ~3;
    desc.mappedAtCreation = false;
    m_buffer = wgpuDeviceCreateBuffer(device, &desc);
    m_alignedSize = desc.size;
    if (!m_buffer) {
        std::cerr << "[ERROR] Failed to create WGPUBuffer" << std::endl;
        return false;
    }
    if (data && queue) {
        update(queue, data, size, 0);
    }
    return true;
}

void WgpuBuffer::update(WGPUQueue queue, const void *data, size_t size, size_t offset) {
    if (m_buffer && queue && data) {
        wgpuQueueWriteBuffer(queue, m_buffer, offset, data, size);
    }
}
