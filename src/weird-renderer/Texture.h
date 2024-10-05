#ifndef TEXTURE_CLASS_H
#define TEXTURE_CLASS_H

#include <glad/glad.h>
#include <stb/stb_image.h>

#include "Shader.h"

#include <glm/vec4.hpp>

namespace WeirdRenderer
{
	using TextureID = std::uint32_t;

	class Texture
	{
	public:

		GLuint ID = -1;
		std::string type = "";

		Texture() {}; // I shouldn't neet this :S

		Texture(const char* image, std::string texType, GLuint slot);

		Texture(glm::vec4 color, std::string texType, GLuint slot);

		Texture(int width, int height, GLuint filterMode);

		// Assigns a texture unit to a texture
		void texUnit(Shader& shader, const char* uniform, GLuint unit) const;

		// Binds a texture
		void bind() const;
		void bind(GLuint unit) const;

		// Unbinds a texture
		void unbind() const;
		// Deletes a texture
		void dispose() const;

		void saveToDisk(const char* fileName);

	private:

		GLenum m_textureSlot;

	};
}

#endif