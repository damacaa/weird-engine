#pragma once
#include <cassert>
#include <cstring>
#include <vector>
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

			void uploadRawData(const void* data, size_t byteSize) const
			{
				const size_t texelBytes = 4 * sizeof(float); // one RGBA32F texel
				if (byteSize == 0 || (byteSize % texelBytes != 0))
					return;

				GLint maxTexSize = 16384;
				// glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

				size_t totalTexels = byteSize / texelBytes;
				GLsizei texW, texH;

				auto s = static_cast<GLsizei>(totalTexels);
				if (s <= maxTexSize)
				{
					texW = static_cast<GLsizei>(totalTexels);
					texH = 1;
				}
				else
				{
					// Wrap into 2D: use maxTexSize as row width, add enough rows
					texW = static_cast<GLsizei>(maxTexSize);
					texH = static_cast<GLsizei>((totalTexels + maxTexSize - 1) / maxTexSize);
				}

				// When wrapping to 2D, texW*texH may exceed totalTexels (due to ceiling
				// division). Pad with zeros so OpenGL never reads past the source buffer.
				const void* uploadPtr = data;
				std::vector<float> paddedData;
				size_t paddedTexels = static_cast<size_t>(texW) * static_cast<size_t>(texH);
				if (paddedTexels > totalTexels)
				{
					paddedData.resize(paddedTexels * 4, 0.0f); // 4 floats per RGBA32F texel
					std::memcpy(paddedData.data(), data, byteSize);
					uploadPtr = paddedData.data();
				}

				GLint previousActiveTexture = 0;
 				GLint previousTextureBinding2D = 0;
 				glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
 				glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureBinding2D);
 				glBindTexture(GL_TEXTURE_2D, m_texture);
 				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texW, texH, 0, GL_RGBA, GL_FLOAT, uploadPtr);
 				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding2D));
 				glActiveTexture(static_cast<GLenum>(previousActiveTexture));
			}

			template <typename T> void uploadData(const T* data, size_t count) const
			{
				uploadRawData(data, count * sizeof(T));
			}

			template <typename T> void uploadData2D(const T* data, size_t width, size_t height) const
			{
				size_t byteSize = width * height * sizeof(T);
				const size_t texelBytes = 4 * sizeof(float); // one RGBA32F texel
				if (byteSize == 0 || (byteSize % texelBytes != 0))
					return;

				GLint previousActiveTexture = 0;
				GLint previousTextureBinding2D = 0;
				glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureBinding2D);
				glBindTexture(GL_TEXTURE_2D, m_texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, data);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding2D));
				glActiveTexture(static_cast<GLenum>(previousActiveTexture));
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
