#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <glad/glad.h>
#include <vector>

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

				constexpr GLsizei maxTexSize = 16384;

				size_t totalTexels = byteSize / texelBytes;
				GLsizei texW, texH;

				if (totalTexels <= static_cast<size_t>(maxTexSize))
				{
					texW = static_cast<GLsizei>(totalTexels);
					texH = 1;
				}
				else
				{
					// Wrap into 2D: use maxTexSize as row width, add enough rows
					texW = maxTexSize;
					texH = static_cast<GLsizei>((totalTexels + maxTexSize - 1) / maxTexSize);
				}

				// When wrapping to 2D, texW*texH may exceed totalTexels (due to ceiling
				// division). Pad with zeros so OpenGL never reads past the source buffer.
				const void* uploadPtr = data;
				size_t paddedTexels = static_cast<size_t>(texW) * static_cast<size_t>(texH);
				if (paddedTexels > totalTexels)
				{
					m_paddedScratch.assign(paddedTexels * 4, 0.0f); // 4 floats per RGBA32F texel
					std::memcpy(m_paddedScratch.data(), data, byteSize);
					uploadPtr = m_paddedScratch.data();
				}

				GLint previousActiveTexture = 0;
				GLint previousTextureBinding2D = 0;
				glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureBinding2D);
				glBindTexture(GL_TEXTURE_2D, m_texture);

				if (texW > m_allocatedWidth || texH > m_allocatedHeight || m_allocatedFormat != GL_RGBA32F)
				{
					GLsizei allocW = (texH > 1) ? maxTexSize : std::min(maxTexSize, nextPowerOfTwo(std::max(texW, 64)));
					GLsizei allocH = nextPowerOfTwo(std::max(texH, 1));
					if (m_allocatedFormat == GL_RGBA32F)
					{
						allocW = std::max(allocW, m_allocatedWidth);
						allocH = std::max(allocH, m_allocatedHeight);
					}

					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, allocW, allocH, 0, GL_RGBA, GL_FLOAT, nullptr);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					m_allocatedWidth = allocW;
					m_allocatedHeight = allocH;
					m_allocatedFormat = GL_RGBA32F;
				}

				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, GL_RGBA, GL_FLOAT, uploadPtr);

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

				const GLsizei texW = static_cast<GLsizei>(width);
				const GLsizei texH = static_cast<GLsizei>(height);

				GLint previousActiveTexture = 0;
				GLint previousTextureBinding2D = 0;
				glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureBinding2D);
				glBindTexture(GL_TEXTURE_2D, m_texture);

				if (texW > m_allocatedWidth || texH > m_allocatedHeight || m_allocatedFormat != GL_RGBA32F)
				{
					constexpr GLsizei maxTexSize = 16384;
					GLsizei allocW = std::min(maxTexSize, nextPowerOfTwo(std::max(texW, 1)));
					GLsizei allocH = std::min(maxTexSize, nextPowerOfTwo(std::max(texH, 1)));
					if (m_allocatedFormat == GL_RGBA32F)
					{
						allocW = std::max(allocW, m_allocatedWidth);
						allocH = std::max(allocH, m_allocatedHeight);
					}

					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, allocW, allocH, 0, GL_RGBA, GL_FLOAT, nullptr);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					m_allocatedWidth = allocW;
					m_allocatedHeight = allocH;
					m_allocatedFormat = GL_RGBA32F;
				}

				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, GL_RGBA, GL_FLOAT, data);

				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding2D));
				glActiveTexture(static_cast<GLenum>(previousActiveTexture));
			}

			// Uploads a 2D array of uint32_t pairs (start, count) as a compact GL_RG32UI texture (8 B/texel).
			void uploadRG32UI(const uint32_t* data, size_t width, size_t height) const
			{
				if (!data || width == 0 || height == 0)
					return;

				const GLsizei texW = static_cast<GLsizei>(width);
				const GLsizei texH = static_cast<GLsizei>(height);

				GLint previousActiveTexture = 0;
				GLint previousTextureBinding2D = 0;
				glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureBinding2D);
				glBindTexture(GL_TEXTURE_2D, m_texture);

				if (texW > m_allocatedWidth || texH > m_allocatedHeight || m_allocatedFormat != GL_RG32UI)
				{
					constexpr GLsizei maxTexSize = 16384;
					GLsizei allocW = std::min(maxTexSize, nextPowerOfTwo(std::max(texW, 1)));
					GLsizei allocH = std::min(maxTexSize, nextPowerOfTwo(std::max(texH, 1)));
					if (m_allocatedFormat == GL_RG32UI)
					{
						allocW = std::max(allocW, m_allocatedWidth);
						allocH = std::max(allocH, m_allocatedHeight);
					}

					glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, allocW, allocH, 0, GL_RG_INTEGER, GL_UNSIGNED_INT,
								 nullptr);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					m_allocatedWidth = allocW;
					m_allocatedHeight = allocH;
					m_allocatedFormat = GL_RG32UI;
				}

				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, GL_RG_INTEGER, GL_UNSIGNED_INT, data);

				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding2D));
				glActiveTexture(static_cast<GLenum>(previousActiveTexture));
			}

			// Uploads a 2D array of uint16_t indices as a compact GL_R16UI texture (2 B/texel).
			void uploadR16UI(const uint16_t* data, size_t width, size_t height) const
			{
				if (!data || width == 0 || height == 0)
					return;

				const GLsizei texW = static_cast<GLsizei>(width);
				const GLsizei texH = static_cast<GLsizei>(height);

				GLint previousActiveTexture = 0;
				GLint previousTextureBinding2D = 0;
				glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureBinding2D);
				glBindTexture(GL_TEXTURE_2D, m_texture);

				GLint prevAlign = 4;
				const bool needsAlign2 = (texW & 1) != 0;
				if (needsAlign2)
				{
					glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
					glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
				}

				if (texW > m_allocatedWidth || texH > m_allocatedHeight || m_allocatedFormat != GL_R16UI)
				{
					constexpr GLsizei maxTexSize = 16384;
					GLsizei allocW = std::min(maxTexSize, nextPowerOfTwo(std::max(texW, 1)));
					GLsizei allocH = std::min(maxTexSize, nextPowerOfTwo(std::max(texH, 1)));
					if (m_allocatedFormat == GL_R16UI)
					{
						allocW = std::max(allocW, m_allocatedWidth);
						allocH = std::max(allocH, m_allocatedHeight);
					}

					glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, allocW, allocH, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT,
								 nullptr);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					m_allocatedWidth = allocW;
					m_allocatedHeight = allocH;
					m_allocatedFormat = GL_R16UI;
				}

				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, GL_RED_INTEGER, GL_UNSIGNED_SHORT, data);

				if (needsAlign2)
				{
					glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
				}

				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTextureBinding2D));
				glActiveTexture(static_cast<GLenum>(previousActiveTexture));
			}

			GLuint getTexture() const
			{
				return m_texture;
			}

		private:
			static GLsizei nextPowerOfTwo(GLsizei v)
			{
				GLsizei p = 1;
				while (p < v && p < 16384)
				{
					p <<= 1;
				}
				return p;
			}

			GLuint m_texture = 0;
			mutable GLsizei m_allocatedWidth = 0;
			mutable GLsizei m_allocatedHeight = 0;
			mutable GLenum m_allocatedFormat = 0;
			mutable std::vector<float> m_paddedScratch;
		};

	} // namespace WeirdRenderer
} // namespace WeirdEngine
