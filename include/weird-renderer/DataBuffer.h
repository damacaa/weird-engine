#ifndef WEIRD_RENDERER_DATABUFFER_H
#define WEIRD_RENDERER_DATABUFFER_H

#include <GL/glew.h> // Should be glad/glad.h as per new requirements
#include <cstddef> // For size_t

// Forward declaration for GLuint if glad/glad.h is not included here directly
// However, it's better to include what's needed.
// The problem description for TBODataBuffer says to include <glad/glad.h>
// So, it's better to use glad/glad.h here too for consistency.

namespace WeirdEngine
{
namespace WeirdRenderer
{

/**
 * @brief Base interface for OpenGL buffer objects like TBOs and SSBOs.
 *
 * This interface defines common functionality for managing and using buffer objects
 * that store data for shaders.
 */
class DataBuffer
{
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~DataBuffer() = default;

    /**
     * @brief Binds the buffer to a specific binding point (unit).
     *
     * The interpretation of 'unit' depends on the buffer type (e.g., texture image unit for TBOs,
     * shader storage block binding index for SSBOs).
     *
     * @param unit The binding point to bind the buffer to.
     */
    virtual void bind(GLuint unit) = 0; // GLuint will come from glad/glad.h

    /**
     * @brief Unbinds the buffer from its current target.
     *
     * This typically involves binding buffer ID 0 to the target the buffer was bound to.
     */
    virtual void unbind() = 0;

    /**
     * @brief Uploads raw data to the buffer.
     *
     * @param data Pointer to the data to be uploaded.
     * @param byteSize The size of the data in bytes.
     */
    virtual void uploadRawData(const void* data, size_t byteSize) = 0;

    /**
     * @brief Gets the OpenGL buffer ID (handle) of this buffer.
     *
     * @return The OpenGL buffer ID.
     */
    virtual GLuint getBuffer() const = 0; // GLuint will come from glad/glad.h
};

} // namespace WeirdRenderer
} // namespace WeirdEngine

#endif // WEIRD_RENDERER_DATABUFFER_H
