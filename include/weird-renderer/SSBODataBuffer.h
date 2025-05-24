#ifndef WEIRD_RENDERER_SSBODATABUFFER_H
#define WEIRD_RENDERER_SSBODATABUFFER_H

#include <glad/glad.h>
#include "weird-renderer/DataBuffer.h" // Interface
#include <cstddef> // For size_t

namespace WeirdEngine
{
namespace WeirdRenderer
{

/**
 * @brief Implements DataBuffer for Shader Storage Buffer Objects (SSBOs).
 *
 * Manages an OpenGL buffer used for general purpose shader storage.
 */
class SSBODataBuffer : public DataBuffer
{
public:
    /**
     * @brief Constructs an SSBODataBuffer.
     *
     * Initializes the OpenGL buffer. An initial binding point can be provided.
     * @param initialBindingPoint The initial binding point to associate with this buffer.
     *                            This can be overridden in the bind() call.
     */
    explicit SSBODataBuffer(GLuint initialBindingPoint = 0);

    /**
     * @brief Destroys the SSBODataBuffer.
     *
     * Releases the OpenGL buffer resource.
     */
    virtual ~SSBODataBuffer();

    // Deleted copy constructor and assignment operator
    SSBODataBuffer(const SSBODataBuffer&) = delete;
    SSBODataBuffer& operator=(const SSBODataBuffer&) = delete;

    // Deleted move constructor and assignment operator
    SSBODataBuffer(SSBODataBuffer&&) = delete;
    SSBODataBuffer& operator=(SSBODataBuffer&&) = delete;

    /**
     * @brief Binds the SSBO to a specific shader storage binding point.
     *
     * This uses glBindBufferBase. The 'unit' parameter from the DataBuffer interface
     * is used as the 'bindingPoint' for the SSBO.
     *
     * @param bindingPoint The shader storage binding point index.
     */
    void bind(GLuint bindingPoint) override;

    /**
     * @brief Unbinds the currently bound SSBO from the GL_SHADER_STORAGE_BUFFER target.
     *
     * This binds buffer ID 0 to GL_SHADER_STORAGE_BUFFER.
     */
    void unbind() override;

    /**
     * @brief Uploads raw data to the underlying buffer.
     *
     * The data is uploaded with GL_DYNAMIC_DRAW usage hint by default.
     *
     * @param data Pointer to the data.
     * @param byteSize Size of the data in bytes.
     */
    void uploadRawData(const void* data, size_t byteSize) override;

    /**
     * @brief Template method to upload an array of typed data.
     *
     * Calculates the total byte size and calls uploadRawData.
     *
     * @tparam T The type of data elements.
     * @param data Pointer to the array of data.
     * @param count The number of elements in the array.
     */
    template<typename T>
    void uploadData(const T* data, size_t count)
    {
        uploadRawData(data, count * sizeof(T));
    }

    /**
     * @brief Gets the OpenGL buffer ID.
     * @return The buffer ID.
     */
    GLuint getBuffer() const override;

private:
    GLuint m_buffer;        // OpenGL buffer object ID
    GLuint m_bindingPoint;  // Stores the current binding point for this SSBO
};

} // namespace WeirdRenderer
} // namespace WeirdEngine

#endif // WEIRD_RENDERER_SSBODATABUFFER_H
