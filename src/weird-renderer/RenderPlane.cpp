#include "weird-renderer/RenderPlane.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		RenderPlane::RenderPlane()
		{
			float f = 1.0f;

			// Vertices with positions (x, y, z) and texture coordinates (u, v)
			GLfloat vertices[] = {
				// positions        // UVs
				-f, -f, 0.0f, 0.0f, 0.0f, // Lower left corner
				-f, f, 0.0f, 0.0f, 1.0f, // Upper left corner
				f, f, 0.0f, 1.0f, 1.0f, // Upper right corner
				f, -f, 0.0f, 1.0f, 0.0f // Lower right corner
			};

			GLuint indices[] = {
				0, 1, 2, // First triangle
				0, 2, 3 // Second triangle
			};

			glGenVertexArrays(1, &VAO);
			glGenBuffers(1, &VBO);
			glGenBuffers(1, &EBO);

			glBindVertexArray(VAO);

			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

			// Position attribute
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);

			// UV attribute
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}


		void RenderPlane::draw(Shader& shader) const
		{
			// Bind the VAO so OpenGL knows to use it
			glBindVertexArray(VAO);
			// Draw the triangle using the GL_TRIANGLES primitive
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		}


		void RenderPlane::free()
		{
			glDeleteVertexArrays(1, &VAO);
			glDeleteBuffers(1, &VBO);
		}
	}
}