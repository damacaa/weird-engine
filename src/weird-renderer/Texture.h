#ifndef TEXTURE_CLASS_H
#define TEXTURE_CLASS_H

#include <glad/glad.h>
#include <stb/stb_image.h>

#include "Shader.h"

#include <glm/vec4.hpp>

class Texture
{
public:
	GLuint ID = -1;
	const char* type;
	GLuint unit;

	Texture(const char* image, const char* texType, GLuint slot);

	// Assigns a texture unit to a texture
	void texUnit(Shader& shader, const char* uniform, GLuint unit) const;
	// Binds a texture
	void Bind() const;
	// Unbinds a texture
	void Unbind() const;
	// Deletes a texture
	void Delete() const;

	/*// Public static member function
	static Texture GetDefaultTexture(int d, const char* texType, GLuint slot) {


		GLuint textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// Create a 1x1 texture with the default color
		glm::u8vec3 color = glm::clamp(glm::vec3(1) * 255.0f, 0.0f, 255.0f);
		GLubyte pixels[] = { color.r, color.g, color.b };

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture

		return MyClass(d);
	}*/

	Texture(glm::vec4 color, const char* texType, GLuint slot);
private:
};
#endif