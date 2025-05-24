#include "weird-renderer/SSBODataBuffer.h"
#include <glad/glad.h> // For OpenGL functions

// Standard library headers if needed, e.g. for logging or asserts
// For now, only glad/glad.h and its own header seem necessary.

namespace WeirdEngine
{
namespace WeirdRenderer
{

SSBODataBuffer::SSBODataBuffer(GLuint initialBindingPoint) 
    : m_buffer(0), m_bindingPoint(initialBindingPoint)
{
    glGenBuffers(1, &m_buffer);
    // It's common practice to also initialize the buffer's data store here,
    // even if it's just to allocate it without specific data.
    // For example, one might do:
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, SOME_INITIAL_SIZE, nullptr, GL_DYNAMIC_DRAW);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    // However, the task doesn't specify initial allocation, so we'll just generate the ID.
    // The user is expected to call uploadRawData to allocate and fill.
}

SSBODataBuffer::~SSBODataBuffer()
{
    if (m_buffer != 0)
    {
        glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
    }
}

void SSBODataBuffer::bind(GLuint bindingPoint)
{
    // The 'unit' from DataBuffer::bind(GLuint unit) is used as 'bindingPoint' here.
    m_bindingPoint = bindingPoint; // Update stored binding point
    if (m_buffer != 0)
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_bindingPoint, m_buffer);
    }
}

void SSBODataBuffer::unbind()
{
    // As per task: "This should use glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0)".
    // This unbinds any buffer from the GL_SHADER_STORAGE_BUFFER target.
    // An alternative is glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_bindingPoint, 0);
    // which unbinds only the buffer at the stored m_bindingPoint.
    // Sticking to the requirement:
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // If we wanted to unbind only this buffer's specific binding point:
    // if (m_buffer != 0 && m_bindingPoint_was_ever_bound) { // Need a flag for this
    //     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_bindingPoint, 0);
    // }
}

void SSBODataBuffer::uploadRawData(const void* data, size_t byteSize)
{
    if (m_buffer == 0) return; // Or throw an error

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, byteSize, data, GL_DYNAMIC_DRAW); // Using GL_DYNAMIC_DRAW as a common default
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind after upload
}

GLuint SSBODataBuffer::getBuffer() const
{
    return m_buffer;
}

// The template method uploadData<T> is defined in the header file SSBODataBuffer.h

} // namespace WeirdRenderer
} // namespace WeirdEngine
