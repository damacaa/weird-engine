#ifndef TEXTURE_CLASS_H
#define TEXTURE_CLASS_H

#include <glad/glad.h>
#include <glm/vec4.hpp>

#include "Shader.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		using TextureID = std::uint32_t;

		class Texture
		{
		public:

			enum class TextureType
			{
				Color,
				ColorAlpha,
				SingleChannel,
				RetroColor,
				Depth,
				Data,
				LinearData,
				IntData
			};

			GLuint ID = -1;

			Texture(): width(0), height(0) {};

			Texture(const char* image);

			Texture(glm::vec4 color);

			Texture(int width, int height, TextureType type);

			// Binds a texture
			void bind(GLuint unit = 0) const;

			// Unbinds a texture
			void unbind() const;
			// Deletes a texture
			void dispose() const;

			void saveToDisk(const char* fileName);

		private:
			int width, height, numColCh;

			void createTexture(void* data, int width, int height, TextureType type);

		public:
			int getWidth() const {return width;};
			int getHeight() const {return height;};
			int getNumColChannels() const {return numColCh;};
		};
	}
}

#endif