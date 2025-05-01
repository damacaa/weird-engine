#pragma once
#include <glad/glad.h>
#include "Shape2D.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class DataBuffer {
		public:

			DataBuffer() {};

			DataBuffer(GLuint bindingPoint)
				: m_bindingPoint(bindingPoint)
			{
				// Create and bind UBO
				glGenBuffers(1, &m_ubo);
				glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
				glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingPoint, m_ubo);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);

				// Create generic buffer and texture
				glGenBuffers(1, &m_buffer);
				glGenTextures(1, &m_texture);
			}

			~DataBuffer() {
				glDeleteBuffers(1, &m_ubo);
				glDeleteBuffers(1, &m_buffer);
				glDeleteTextures(1, &m_texture);
			}

			void bind() const
			{
				bind(m_bindingPoint);
			}

			void bind(GLuint bindingPoint) const
			{
				// Bind the buffer to the buffer texture
				glBindTexture(GL_TEXTURE_BUFFER, m_texture);
				glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_buffer);

				// Bind texture to slot 1
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_BUFFER, m_texture);
			}

			void sendData(Dot2D* shapes, size_t size) const
			{
				glBindBuffer(GL_TEXTURE_BUFFER, m_buffer);
				glBufferData(GL_TEXTURE_BUFFER, sizeof(Dot2D) * size, shapes, GL_STREAM_DRAW);
			}

			GLuint getUBO() const { return m_ubo; }
			GLuint getBuffer() const { return m_buffer; }
			GLuint getTexture() const { return m_texture; }

		private:
			GLuint m_ubo = 0;
			GLuint m_buffer = 0;
			GLuint m_texture = 0;
			GLuint m_bindingPoint = 0;
		};

	}
}