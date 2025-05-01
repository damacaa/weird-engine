#pragma once
#include <glad/glad.h>
#include <cassert>

namespace WeirdEngine {
	namespace WeirdRenderer {

		class DataBuffer {
		public:
			DataBuffer() 
			{
				// Create buffer and texture for samplerBuffer
				glGenBuffers(1, &m_buffer);
				glGenTextures(1, &m_texture);

				// Bind texture and buffer to the correct targets
				glBindTexture(GL_TEXTURE_BUFFER, m_texture);  // Bind the texture to the buffer type
				glBindBuffer(GL_TEXTURE_BUFFER, m_buffer);   // Bind the buffer to the texture
				glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_buffer); // Attach the buffer to the texture
				glBindTexture(GL_TEXTURE_BUFFER, 0);          // Unbind texture
				glBindBuffer(GL_TEXTURE_BUFFER, 0);           // Unbind buffer
			}

			explicit DataBuffer(GLuint bindingPoint)
				: m_bindingPoint(bindingPoint)
			{
				// Create buffer and texture for samplerBuffer
				glGenBuffers(1, &m_buffer);
				glGenTextures(1, &m_texture);

				// Bind texture and buffer to the correct targets
				glBindTexture(GL_TEXTURE_BUFFER, m_texture);  // Bind the texture to the buffer type
				glBindBuffer(GL_TEXTURE_BUFFER, m_buffer);   // Bind the buffer to the texture
				glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_buffer); // Attach the buffer to the texture
				glBindTexture(GL_TEXTURE_BUFFER, 0);          // Unbind texture
				glBindBuffer(GL_TEXTURE_BUFFER, 0);           // Unbind buffer
			}

			~DataBuffer() 
			{
				glDeleteBuffers(1, &m_buffer);
				glDeleteTextures(1, &m_texture);
			}

			void bind(GLuint unit) const
			{
				glActiveTexture(GL_TEXTURE0 + unit);  // Use the unit passed into the function
				glBindTexture(GL_TEXTURE_BUFFER, m_texture);  // Bind the texture for the specific unit
			}

			void unbind()
			{
				glBindBuffer(GL_TEXTURE_BUFFER, 0);  // Unbind the buffer
			}

			void uploadRawData(const void* data, size_t byteSize) const
			{
				if (byteSize == 0 || (byteSize % (4 * sizeof(float)) != 0)) {
					// Avoid invalid upload (alignment check)
					return;
				}

				glBindBuffer(GL_TEXTURE_BUFFER, m_buffer);  // Bind the buffer before uploading data
				glBufferData(GL_TEXTURE_BUFFER, byteSize, data, GL_STREAM_DRAW);  // Upload the buffer data

				// Attach the buffer to the texture
				glBindTexture(GL_TEXTURE_BUFFER, m_texture);  // Bind the texture
				glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_buffer);  // Attach the buffer to the texture
			}

			template<typename T>
			void uploadData(const T* data, size_t count) const
			{
				uploadRawData(data, count * sizeof(T));
			}

			GLuint getBuffer() const { return m_buffer; }
			GLuint getTexture() const { return m_texture; }

		private:
			GLuint m_buffer = 0;
			GLuint m_texture = 0;
			GLuint m_bindingPoint = 0; // This is unused, so it can be removed if not needed
		};

	}
}
