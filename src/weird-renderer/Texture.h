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

		Texture() {};

		Texture(const char* image, std::string texType, GLuint slot);

		Texture(glm::vec4 color, std::string texType, GLuint slot);

		// Assigns a texture unit to a texture
		void texUnit(Shader& shader, const char* uniform, GLuint unit) const;
		// Binds a texture
		void bind(GLuint unit) const;
		// Unbinds a texture
		void unbind() const;
		// Deletes a texture
		void dispose() const;

	private:

	};
}

#endif