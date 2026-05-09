#include "weird-renderer/resources/Texture.h"

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <vector>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		const char* DIFFUSE = "diffuse";

		Texture::Texture(const char* image)
		{
			// Flips the image so it appears right side up
			wstbi_set_flip_vertically_on_load(true);
			// Reads the image from a file and stores it in bytes
			unsigned char* bytes = wstbi_load(image, &width, &height, &numColCh, 0);

			switch (numColCh)
			{
				case 4:
				{
					createTexture((float*)bytes, width, height, TextureType::ColorAlpha);
					break;
				}
				case 3:
				{
					createTexture((float*)bytes, width, height, TextureType::Color);
					break;
				}
				case 1:
				{
					createTexture((float*)bytes, width, height, TextureType::SingleChannel);
					break;
				}
				default:
					break;
			}

			// Generates MipMaps
			glGenerateMipmap(GL_TEXTURE_2D);

			// Deletes the image data as it is already in the OpenGL Texture object
			wstbi_image_free(bytes);

			// Unbinds the OpenGL Texture object so that it can't accidentally be modified
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		Texture::Texture(glm::vec4 color)
			: width(1)
			, height(1)
		{
			unsigned char* textureData = new unsigned char[4];

			// Multiply 0.0-1.0 range by 255 to get 0-255 range
			textureData[0] = (unsigned char)(color.r * 255.0f);
			textureData[1] = (unsigned char)(color.g * 255.0f);
			textureData[2] = (unsigned char)(color.b * 255.0f);
			textureData[3] = (unsigned char)(color.a * 255.0f);

			createTexture(textureData, width, height, TextureType::ColorAlpha);

			delete[] textureData;
		}

		Texture::Texture(int width, int height, TextureType type)
			: width(width)
			, height(height)
		{
			createTexture(nullptr, width, height, type);
		}

		void Texture::createTexture(void* data, int width, int height, TextureType type)
		{
			std::vector<unsigned char> zeroDataU8;
			std::vector<float> zeroDataF32;
			std::vector<unsigned int> zeroDataU32;

			auto getUploadDataU8 = [&](int channels) -> const void*
			{
				if (data != nullptr)
					return data;

				zeroDataU8.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels), 0u);
				return zeroDataU8.data();
			};

			auto getUploadDataF32 = [&](int channels) -> const void*
			{
				if (data != nullptr)
					return data;

				zeroDataF32.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels), 0.0f);
				return zeroDataF32.data();
			};

			auto getUploadDataDepth = [&]() -> const void*
			{
				zeroDataU32.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
				return zeroDataU32.data();
			};

			// Generates an OpenGL texture object
			glGenTextures(1, &ID);
			// Assigns the texture to a Texture Unit
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, ID);

			switch (type)
			{
				case TextureType::Color:
				{
					// Handle color texture
					// Use sized internal format GL_RGB8; GL_RGB (unsized) is not guaranteed
					// to be color-renderable as a framebuffer attachment in GLES 3.0.
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, getUploadDataU8(3));

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

					numColCh = 3;

					break;
				}
				case TextureType::ColorAlpha:
				{
					// Handle color with alpha texture
					// Use sized internal format GL_RGBA8; GL_RGBA (unsized) is not guaranteed
					// to be color-renderable as a framebuffer attachment in GLES 3.0.
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, getUploadDataU8(4));

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

					numColCh = 4;

					break;
				}
				case TextureType::SingleChannel:
				{

					glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, getUploadDataU8(1));

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

					numColCh = 1;

					break;
				}
				case TextureType::Depth:
				{
					// Handle depth texture
					// In GLES 3.0, GL_DEPTH_COMPONENT24 requires type GL_UNSIGNED_INT (not GL_FLOAT).
					// Use GL_DEPTH_COMPONENT32F + GL_FLOAT if a float depth buffer is needed.
					glTexImage2D(GL_TEXTURE_2D, 0,
								 GL_DEPTH_COMPONENT24, // 24-bit depth
								 width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, getUploadDataDepth());

					// Set texture parameters
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

					// Prevent sampling artifacts in shadow mapping
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE); // or GL_COMPARE_REF_TO_TEXTURE

					numColCh = 1;
					break;
				}
				case TextureType::Data:
				{
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, getUploadDataF32(4));
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

					numColCh = 4;
					break;
				}
				case TextureType::LinearData:
				{
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, getUploadDataF32(4));
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

					numColCh = 4;
					break;
				}
				case TextureType::AccumulationData:
				{
					// Use 32-bit floats for accumulation to avoid precision stalling over thousands of frames
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, getUploadDataF32(4));
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

					numColCh = 4;
					break;
				}
				case TextureType::RetroColor:
				{
					// Use sized internal format GL_RGB8; GL_RGB (unsized) is not guaranteed
					// to be color-renderable as a framebuffer attachment in GLES 3.0.
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, getUploadDataU8(3));

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

					numColCh = 3;
					break;
				}
				case TextureType::IntData:
				{
					// Handle integer data texture
					break;
				}
				default:
				{
					// Handle unknown type
					break;
				}
			}

			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void Texture::bind(GLuint unit) const
		{
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(GL_TEXTURE_2D, ID);
		}

		void Texture::dispose() const
		{
			glDeleteTextures(1, &ID);
		}

		void Texture::saveToDisk(const char* fileName)
		{
#if defined(GL_ES_VERSION_2_0)
			GLuint prevFBO = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&prevFBO);

			GLuint fbo;
			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ID, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				std::cout << "Texture saving failed: Framebuffer not complete." << std::endl;
				glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
				glDeleteFramebuffers(1, &fbo);
				return;
			}

			// In GLES, reading from floating-point or integer framebuffers using GL_UNSIGNED_BYTE 
			// can fail with GL_INVALID_OPERATION. We must query the attachment component type.
			GLint attachmentType = 0;
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &attachmentType);

			unsigned char* pixels_uchar = new unsigned char[width * height * 4];

			if (attachmentType == GL_FLOAT)
			{
				float* pixels_f = new float[width * height * 4];
				glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels_f);

				for (int i = 0; i < width * height * 4; ++i)
				{
					pixels_uchar[i] = static_cast<unsigned char>(glm::clamp(pixels_f[i], 0.0f, 1.0f) * 255.0f);
				}
				delete[] pixels_f;
			}
			else if (attachmentType == GL_INT)
			{
				int* pixels_i = new int[width * height * 4];
				glReadPixels(0, 0, width, height, GL_RGBA_INTEGER, GL_INT, pixels_i);

				for (int i = 0; i < width * height * 4; ++i)
				{
					pixels_uchar[i] = static_cast<unsigned char>(glm::clamp((float)pixels_i[i] / 255.0f, 0.0f, 1.0f) * 255.0f);
				}
				delete[] pixels_i;
			}
			else if (attachmentType == GL_UNSIGNED_INT)
			{
				unsigned int* pixels_ui = new unsigned int[width * height * 4];
				glReadPixels(0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixels_ui);

				for (int i = 0; i < width * height * 4; ++i)
				{
					pixels_uchar[i] = static_cast<unsigned char>(glm::clamp((float)pixels_ui[i] / 255.0f, 0.0f, 1.0f) * 255.0f);
				}
				delete[] pixels_ui;
			}
			else
			{
				// Default for GL_UNSIGNED_NORMALIZED, GL_SIGNED_NORMALIZED, etc.
				glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_uchar);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
			glDeleteFramebuffers(1, &fbo);

			unsigned char* flipped_pixels = new unsigned char[width * height * 4];
			const size_t pixel_size = 4;

			for (int y = 0; y < height; y++)
			{
				size_t src_row_start = pixel_size * width * y;
				size_t dest_row_start = pixel_size * width * (height - 1 - y);

				for (int x = 0; x < width; x++)
				{
					size_t src_idx = src_row_start + pixel_size * x;
					size_t dest_idx = dest_row_start + pixel_size * x;

					flipped_pixels[dest_idx] = pixels_uchar[src_idx];
					flipped_pixels[dest_idx + 1] = pixels_uchar[src_idx + 1];
					flipped_pixels[dest_idx + 2] = pixels_uchar[src_idx + 2];
					flipped_pixels[dest_idx + 3] = pixels_uchar[src_idx + 3];
				}
			}

			wstbi_write_png(fileName, width, height, 4, flipped_pixels, width * 4);

			delete[] pixels_uchar;
			delete[] flipped_pixels;
#else
			glBindTexture(GL_TEXTURE_2D, ID);

			GLenum format = GL_RGBA;
			int format_channels = 4;
			if (numColCh == 3)
			{
				format = GL_RGB;
				format_channels = 3;
			}
			else if (numColCh == 1)
			{
				format = GL_RED;
				format_channels = 1;
			}

			// Create a buffer to hold the pixel data.
			float* pixels = new float[(size_t)width * height * format_channels];

			// Read the pixels from the texture into the buffer.
			glGetTexImage(GL_TEXTURE_2D, 0, format, GL_FLOAT, pixels);

			unsigned char* pixels_uchar = new unsigned char[(size_t)width * height * 4];
			
			for (int y = 0; y < height; y++)
			{
				size_t src_row_start = (size_t)format_channels * width * y;
				size_t dest_row_start = 4 * (size_t)width * (height - 1 - y);

				for (int x = 0; x < width; x++)
				{
					size_t src_idx = src_row_start + format_channels * x;
					size_t dest_idx = dest_row_start + 4 * x;

					pixels_uchar[dest_idx] =
						static_cast<unsigned char>(glm::clamp(pixels[src_idx], 0.0f, 1.0f) * 255.0f);
					
					if (format_channels >= 3)
					{
						pixels_uchar[dest_idx + 1] =
							static_cast<unsigned char>(glm::clamp(pixels[src_idx + 1], 0.0f, 1.0f) * 255.0f);
						pixels_uchar[dest_idx + 2] =
							static_cast<unsigned char>(glm::clamp(pixels[src_idx + 2], 0.0f, 1.0f) * 255.0f);
					}
					else
					{
						pixels_uchar[dest_idx + 1] = pixels_uchar[dest_idx];
						pixels_uchar[dest_idx + 2] = pixels_uchar[dest_idx];
					}

					if (format_channels == 4)
					{
						pixels_uchar[dest_idx + 3] =
							static_cast<unsigned char>(glm::clamp(pixels[src_idx + 3], 0.0f, 1.0f) * 255.0f);
					}
					else
					{
						// Set the Alpha component
						pixels_uchar[dest_idx + 3] = 255;
					}
				}
			}

			// Save the image using stb_image_write. This will write it as a PNG.
			wstbi_write_png(fileName, width, height, 4, pixels_uchar, width * 4);

			// Free the allocated memory.
			delete[] pixels;
			delete[] pixels_uchar;
#endif
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
