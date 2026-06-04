#pragma once

#include <webgpu.h>

#include <cstddef>

class WgpuBuffer {
public:
    WgpuBuffer();
    ~WgpuBuffer();
    bool create(WGPUDevice device, WGPUQueue queue, const void *data, size_t size, WGPUBufferUsage usage);
    void update(WGPUQueue queue, const void *data, size_t size, size_t offset = 0);
    void destroy();
    WGPUBuffer get() const { return m_buffer; }
    size_t size() const { return m_alignedSize; }
private:
    WGPUBuffer m_buffer;
    size_t m_alignedSize;
};
