#ifndef WEIRD_RENDERER_TBODATABUFFER_H
#define WEIRD_RENDERER_TBODATABUFFER_H

#include <glad/glad.h>
#include "weird-renderer/DataBuffer.h" // Interface
#include <cstddef> // For size_t
#include <cassert> // For assert, though not strictly in the original public API, good for internal checks

namespace WeirdEngine
{
namespace WeirdRenderer
{

/**
 * @brief Implements DataBuffer for Texture Buffer Objects (TBOs).
 *
 * Manages an OpenGL buffer and a texture that provides a view into the buffer,
 * typically used for large arrays of data in shaders.
 */
class TBODataBuffer : public DataBuffer
{
public:
    /**
     * @brief Constructs a TBODataBuffer.
     *
     * Initializes the OpenGL buffer and texture, and sets up their relationship.
     * The texture format is fixed to GL_RGBA32F.
     */
    TBODataBuffer();

    /**
     * @brief Destroys the TBODataBuffer.
     *
     * Releases the OpenGL buffer and texture resources.
     */
    virtual ~TBODataBuffer();

    // Deleted copy constructor and assignment operator to prevent accidental copying
    TBODataBuffer(const TBODataBuffer&) = delete;
    TBODataBuffer& operator=(const TBODataBuffer&) = delete;

    // Deleted move constructor and assignment operator for simplicity, can be added if needed
    TBODataBuffer(TBODataBuffer&&) = delete;
    TBODataBuffer& operator=(TBODataBuffer&&) = delete;

    /**
     * @brief Binds the TBO's texture to a specific texture unit.
     *
     * @param unit The texture unit to bind to (e.g., 0 for GL_TEXTURE0).
     */
    void bind(GLuint unit) override;

    /**
     * @brief Unbinds the TBO's texture from the GL_TEXTURE_BUFFER target for the active unit.
     *
     * This effectively unbinds the texture from whatever unit it was last bound to by glActiveTexture.
     * It binds texture ID 0 to the GL_TEXTURE_BUFFER target.
     */
    void unbind() override;

    /**
     * @brief Uploads raw data to the underlying buffer.
     *
     * The data is uploaded with GL_STREAM_DRAW usage hint.
     * The buffer is then re-attached to the texture with GL_RGBA32F format.
     * Includes an alignment check: byteSize must be a multiple of (4 * sizeof(float)).
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

    /**
     * @brief Gets the OpenGL texture ID associated with this TBO.
     * @return The texture ID.
     */
    GLuint getTexture() const;

private:
    GLuint m_buffer;  // OpenGL buffer object ID
    GLuint m_texture; // OpenGL texture object ID (for the TBO)
    // GLuint m_internalFormat; // Could be a constructor parameter if GL_RGBA32F is not always desired
};

} // namespace WeirdRenderer
} // namespace WeirdEngine

#endif // WEIRD_RENDERER_TBODATABUFFER_H
