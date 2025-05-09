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

		Texture::Texture(const char* image, std::string	texType, GLuint slot)
		{
			// Assigns the type of the texture ot the texture object
			type = texType;

			// Stores the width, height, and the number of color channels of the image
			int widthImg = 0, heightImg = 0, numColCh = 0;
			// Flips the image so it appears right side up
			wstbi_set_flip_vertically_on_load(true);
			// Reads the image from a file and stores it in bytes
			unsigned char* bytes = wstbi_load(image, &widthImg, &heightImg, &numColCh, 0);

			// Generates an OpenGL texture object
			glGenTextures(1, &ID);
			// Assigns the texture to a Texture Unit
			glActiveTexture(GL_TEXTURE0 + slot);
			glBindTexture(GL_TEXTURE_2D, ID);

			// Configures the type of algorithm that is used to make the image smaller or bigger
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			// Configures the way the texture repeats (if it does at all)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			// Extra lines in case you choose to use GL_CLAMP_TO_BORDER
			// float flatColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
			// glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);

			// Check what type of color channels the texture has and load it accordingly
			if (numColCh == 4)
				glTexImage2D
				(
					GL_TEXTURE_2D,
					0,
					GL_RGBA,
					widthImg,
					heightImg,
					0,
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					bytes
				);
			else if (numColCh == 3)
				glTexImage2D
				(
					GL_TEXTURE_2D,
					0,
					GL_RGBA,
					widthImg,
					heightImg,
					0,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					bytes
				);
			else if (numColCh == 1)
				glTexImage2D
				(
					GL_TEXTURE_2D,
					0,
					GL_RGBA,
					widthImg,
					heightImg,
					0,
					GL_RED,
					GL_UNSIGNED_BYTE,
					bytes
				);
			else
				throw std::invalid_argument("Automatic Texture type recognition failed");

			// Generates MipMaps
			glGenerateMipmap(GL_TEXTURE_2D);

			// Deletes the image data as it is already in the OpenGL Texture object
			//TODO: fix this in cmake file wstbi_image_free(bytes);

			// Unbinds the OpenGL Texture object so that it can't accidentally be modified
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void Texture::texUnit(Shader& shader, const char* uniform, GLuint unit) const
		{
			// Gets the location of the uniform
			GLuint texUni = glGetUniformLocation(shader.ID, uniform);
			// Shader needs to be activated before changing the value of a uniform
			shader.activate();
			// Sets the value of the uniform
			glUniform1i(texUni, unit);
		}


		void Texture::bind() const
		{
			glBindTexture(GL_TEXTURE_2D, ID);
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
			int width, height;
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

			//float* data = new  float[width * height * 4];  // Assuming 4 channels (RGBA)
			//
			//glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, data);

			//for (size_t i = 0; i < width * height; i++)
			//{
			//	size_t idx = 4 * i;
			//	data[idx] = data[idx + 3];
			//	data[idx+1] = data[idx + 3];
			//	data[idx+2] = data[idx + 3];

			//	data[idx + 3] = 255;
			//}
			//
			//wstbi_write_png("output_texture.png", width, height, 4, data, width * 4);

			//delete[] data;

			 // Create a buffer to hold the pixel data.
			float* pixels = new float[width * height * 4];  // 4 channels (RGBA) with float data type

			// Read the pixels from the texture into the buffer.
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels);

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

		Texture::Texture(glm::vec4 color, std::string texType, GLuint slot)
		{
			type = texType;

			glGenTextures(1, &ID);
			glActiveTexture(GL_TEXTURE0 + slot);
			glBindTexture(GL_TEXTURE_2D, ID);

			// Create a 1x1 texture with the default color
			GLubyte pixels[] = { color.r, color.g, color.b };

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

			// Set texture parameters
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Configures the type of algorithm that is used to make the image smaller or bigger
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			// Configures the way the texture repeats (if it does at all)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
		}

		Texture::Texture(int width, int height, GLuint filterMode, bool isDepth)
		{
			glGenTextures(1, &ID);
			glBindTexture(GL_TEXTURE_2D, ID);

			if (isDepth)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
			else
			{
				float* textureData = new float[width * height * 4];
				for (size_t i = 0; i < width * height * 4; i++) textureData[i] = 1.0f;

				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, textureData);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				delete[] textureData;
			}

			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
}

