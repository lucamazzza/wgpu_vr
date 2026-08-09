/**
    \file buffer.h
    \brief A wrapper class for a WebGPU buffer.

    \author Luca Mazza
    \copyright 2026 Luca Mazza
*/
#pragma once

#include <webgpu.h>

/**
    \brief A wrapper class for a WebGPU buffer.

    \details This class provides a simple interface for creating, updating, and
             destroying a WebGPU buffer.
             It manages the underlying WGPUBuffer and ensures proper alignment
             of the buffer size.
*/
class WgpuBuffer {
public:

    /**
         \brief Constructor

         \details Initializes the WgpuBuffer instance.
    */
    WgpuBuffer();

    /**
         \brief Destructor

         \details Cleans up the WgpuBuffer instance.
    */
    ~WgpuBuffer();

    /**
         \brief Create a WebGPU buffer

         \details Creates a WebGPU buffer with the specified parameters.

         \param device The WebGPU device
         \param queue The WebGPU queue
         \param data Pointer to the initial data for the buffer
         \param size Size of the buffer in bytes
         \param usage Usage flags for the buffer
         \return true if the buffer was created successfully, false otherwise
    */
    bool create(WGPUDevice device, WGPUQueue queue, const void *data, size_t size, WGPUBufferUsage usage);

    /**
         \brief Update the WebGPU buffer

         \details Updates the contents of the WebGPU buffer with new data.

         \param queue The WebGPU queue
         \param data Pointer to the new data for the buffer
         \param size Size of the new data in bytes
         \param offset Offset in bytes where the new data should be written (default is 0)
    */
    void update(WGPUQueue queue, const void *data, size_t size, size_t offset = 0);

    /**
         \brief Destroy the WebGPU buffer

         \details Destroys the WebGPU buffer and releases any associated resources.
    */
    void destroy();

    /**
         \brief Get the underlying WGPUBuffer

         \details Returns the underlying WGPUBuffer managed by this class.

         \return The underlying WGPUBuffer
    */
    WGPUBuffer get() const { return m_buffer; }

    /**
         \brief Get the aligned size of the buffer

         \details Returns the aligned size of the buffer, which is the size
                  rounded up to the nearest multiple of 256 bytes.

         \return The aligned size of the buffer in bytes
    */
    size_t size() const { return m_alignedSize; }

private:
    WGPUBuffer m_buffer;  ///< The underlying WebGPU buffer
    size_t m_alignedSize; ///< The aligned size of the buffer (256 Bytes)
};
