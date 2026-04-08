#pragma once
#include <cassert>
#include <glad/glad.h>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		// DataBuffer stores an array of vec4 values and exposes it as a plain 2D texture
		// (GL_RGBA32F, width = element count, height = 1) so that shaders can access it
		// via a highp sampler2D without requiring GL_OES_texture_buffer.
		class DataBuffer
		{
		public:
			DataBuffer()
			{
				glGenTextures(1, &m_texture);
			}

			explicit DataBuffer(GLuint /*bindingPoint*/)
			{
				glGenTextures(1, &m_texture);
			}

			~DataBuffer()
			{
				glDeleteTextures(1, &m_texture);
			}

			void bind(GLuint unit) const
			{
				glActiveTexture(GL_TEXTURE0 + unit);
				glBindTexture(GL_TEXTURE_2D, m_texture);
			}

			void unbind()
			{
				glBindTexture(GL_TEXTURE_2D, 0);
			}

			void uploadRawData(const void* data, size_t byteSize) const
			{
				const size_t texelBytes = 4 * sizeof(float); // one RGBA32F texel
				if (byteSize == 0 || (byteSize % texelBytes != 0))
					return;

				GLsizei width = static_cast<GLsizei>(byteSize / texelBytes);

				glBindTexture(GL_TEXTURE_2D, m_texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, 1, 0, GL_RGBA, GL_FLOAT, data);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glBindTexture(GL_TEXTURE_2D, 0);
			}

			template <typename T> void uploadData(const T* data, size_t count) const
			{
				uploadRawData(data, count * sizeof(T));
			}

			GLuint getTexture() const
			{
				return m_texture;
			}

		private:
			GLuint m_texture = 0;
		};

	} // namespace WeirdRenderer
} // namespace WeirdEngine
