#include "weird-renderer/Texture.h"

namespace WeirdEngine
{

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#endif // STB_IMAGE_WRITE_IMPLEMENTATION

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
			: width(1), height(1)
		{
			float* textureData = new float[4];

			textureData[0] = color.r;
			textureData[1] = color.g;
			textureData[2] = color.b;
			textureData[3] = color.a;

			createTexture(textureData, width, height, TextureType::ColorAlpha);

			delete[] textureData;
		}


		Texture::Texture(int width, int height, TextureType type)
			: width(width), height(height)
		{
			createTexture(nullptr, width, height, type);
		}

		void Texture::createTexture(float* data, int width, int height, TextureType type)
		{
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

				glTexImage2D(
					GL_TEXTURE_2D,
					0,
					GL_RGB,
					width,
					height,
					0,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					data);

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

				glTexImage2D(
					GL_TEXTURE_2D,
					0,
					GL_RGBA,
					width,
					height,
					0,
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					data);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

				numColCh = 4;

				break;
			}
			case TextureType::SingleChannel:
			{

				glTexImage2D(
					GL_TEXTURE_2D,
					0,
					GL_R8,
					width,
					height,
					0,
					GL_RED,
					GL_UNSIGNED_BYTE,
					data);

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
				glTexImage2D(
					GL_TEXTURE_2D,
					0,
					GL_DEPTH_COMPONENT24, // 24-bit depth
					width,
					height,
					0,
					GL_DEPTH_COMPONENT,
					GL_FLOAT,
					nullptr);

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
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, data);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				numColCh = 4;
				break;
			}
			case TextureType::RetroColor:
			{
				// Handle retro-style color texture
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

		void Texture::unbind() const
		{
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void Texture::dispose() const
		{
			glDeleteTextures(1, &ID);
		}

		void Texture::saveToDisk(const char* fileName)
		{
			glBindTexture(GL_TEXTURE_2D, ID);

			// float* data = new  float[width * height * 4];  // Assuming 4 channels (RGBA)
			//
			// glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data);

			// for (size_t i = 0; i < width * height; i++)
			//{
			//	size_t idx = 4 * i;
			//	data[idx] = data[idx + 3];
			//	data[idx+1] = data[idx + 3];
			//	data[idx+2] = data[idx + 3];

			//	data[idx + 3] = 255;
			//}
			//
			// wstbi_write_png("output_texture.png", width, height, 4, data, width * 4);

			// delete[] data;

			// Create a buffer to hold the pixel data.
			float* pixels = new float[width * height * numColCh]; // 4 channels (RGBA) with float data type

			// Read the pixels from the texture into the buffer.
			glGetTexImage(GL_TEXTURE_2D, 0, numColCh == 3 ? GL_RGB : GL_RGBA, GL_FLOAT, pixels);

			// Convert float data to unsigned char since stb_image_write expects that format.
			unsigned char* pixels_uchar = new unsigned char[width * height * 4];
			for (int i = 0; i < width * height; i++)
			{
				size_t idx = 4 * i;
				/*pixels_uchar[idx] = static_cast<unsigned char>(pixels[idx + 3] * 255.0f);
				pixels_uchar[idx + 1] = static_cast<unsigned char>(pixels[idx + 3] * 255.0f);
				pixels_uchar[idx + 2] = static_cast<unsigned char>(pixels[idx + 3] * 255.0f);
				pixels_uchar[idx + 3] = 255.0f;*/

				pixels_uchar[idx] = static_cast<unsigned char>(pixels[idx] * 255.0f);
				pixels_uchar[idx + 1] = static_cast<unsigned char>(pixels[idx + 1] * 255.0f);
				pixels_uchar[idx + 2] = static_cast<unsigned char>(pixels[idx + 2] * 255.0f);
				pixels_uchar[idx + 3] = 255.0f;
			}

			// Save the image using stb_image_write. This will write it as a PNG.
			wstbi_write_png(fileName, width, height, 4, pixels_uchar, width * 4);

			// Free the allocated memory.
			delete[] pixels;
			delete[] pixels_uchar;
		}

	}
}
