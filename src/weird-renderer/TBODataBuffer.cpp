#include "weird-renderer/TBODataBuffer.h"
#include <glad/glad.h> // For OpenGL functions
#include <cassert>     // For assert, if used (original had it)

// Standard library headers if needed for specific implementations, e.g. <iostream> for debugging
// For now, only glad/glad.h and its own header seem necessary.

namespace WeirdEngine
{
namespace WeirdRenderer
{

TBODataBuffer::TBODataBuffer() : m_buffer(0), m_texture(0)
{
    // Create buffer and texture for samplerBuffer
    glGenBuffers(1, &m_buffer);
    glGenTextures(1, &m_texture);

    // Bind texture and buffer to the correct targets, and associate them.
    // This initial setup is important.
    glBindTexture(GL_TEXTURE_BUFFER, m_texture);
    glBindBuffer(GL_TEXTURE_BUFFER, m_buffer);
    // Associate the buffer with the texture, specifying the format.
    // GL_RGBA32F is used as per the original code.
    // No data is uploaded here yet, just setting up the structure.
    // The buffer will be initially empty or undefined.
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_buffer);

    // Unbind to leave a clean state
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

TBODataBuffer::~TBODataBuffer()
{
    glDeleteBuffers(1, &m_buffer);
    glDeleteTextures(1, &m_texture);
    m_buffer = 0;
    m_texture = 0;
}

void TBODataBuffer::bind(GLuint unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_BUFFER, m_texture);
}

void TBODataBuffer::unbind()
{
    // To unbind a TBO, we typically unbind the texture from the target GL_TEXTURE_BUFFER
    // on the currently active texture unit (or ensure no specific TBO texture is bound).
    // Binding 0 to GL_TEXTURE_BUFFER is the common way.
    // This assumes that the correct texture unit was active if this unbind is specific to a unit.
    // The original DataBuffer::unbind was glBindBuffer(GL_TEXTURE_BUFFER, 0) which is different.
    // For a TBO, unbinding the texture makes more sense.
    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

void TBODataBuffer::uploadRawData(const void* data, size_t byteSize)
{
    // Original code had an alignment check for GL_RGBA32F
    // (byteSize % (4 * sizeof(float)) != 0)
    // 4 floats, each sizeof(float) bytes.
    // GL_RGBA32F means 4 components, each a 32-bit float.
    // So, each pixel/texel is 4 * 4 = 16 bytes.
    // The check `byteSize % (4 * sizeof(float)) != 0` is correct.
    if (byteSize > 0 && (byteSize % (4 * sizeof(float)) != 0)) {
        // This indicates a potential misuse or data that doesn't align with GL_RGBA32F.
        // Depending on error handling strategy, could throw, log, or assert.
        // Original code just returned, so we'll keep that behavior.
        // Consider adding a log message here in a real application.
        return;
    }
    
    if (byteSize == 0) {
        // Handle zero-size upload: can clear or leave as is.
        // Original code would skip if byteSize is 0 due to the condition above.
        // If byteSize is 0, glBufferData might deallocate.
        // We'll allow glBufferData to handle it, but ensure texture is updated.
    }

    glBindBuffer(GL_TEXTURE_BUFFER, m_buffer);
    glBufferData(GL_TEXTURE_BUFFER, byteSize, data, GL_STREAM_DRAW); // Upload data

    // After glBufferData, the association with the texture might need to be re-established,
    // especially if glBufferData reallocates the buffer store (which it can).
    glBindTexture(GL_TEXTURE_BUFFER, m_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_buffer); // Re-associate texture with buffer

    // Unbind buffer to leave a clean state, texture remains bound to its target for a moment.
    glBindBuffer(GL_TEXTURE_BUFFER, 0); 
    // Optionally unbind the texture as well if not immediately used.
    // glBindTexture(GL_TEXTURE_BUFFER, 0); // This was not in the original uploadRawData logic.
}

GLuint TBODataBuffer::getBuffer() const
{
    return m_buffer;
}

GLuint TBODataBuffer::getTexture() const
{
    return m_texture;
}

// The template method uploadData<T> is defined in the header file TBODataBuffer.h

} // namespace WeirdRenderer
} // namespace WeirdEngine
