#include "weird-renderer/RenderPlane.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		RenderPlane::RenderPlane()
		{
			float f = 1.0f;
			// Vertices coordinates
			GLfloat vertices[] =
			{
				-f, -f , 0.0f, // Lower left corner
				-f,  f , 0.0f, // Upper corner
				 f,  f , 0.0f, // Lower right corner
				 f, -f , 0.0f // Lower right corner
			};

			// Indices for vertices order
			GLuint indices[] =
			{
				0, 1, 2, // Lower left triangle
				0, 2, 3 // Upper triangle
			};

			// Generate the VAO, VBO, and EBO with only 1 object each
			glGenVertexArrays(1, &VAO);
			glGenBuffers(1, &VBO);
			glGenBuffers(1, &EBO);


			// Make the VAO the current Vertex Array Object by binding it
			glBindVertexArray(VAO);

			// Bind the VBO specifying it's infinityLoop GL_ARRAY_BUFFER
			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			// Introduce the vertices into the VBO
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			// Bind the EBO specifying it's infinityLoop GL_ELEMENT_ARRAY_BUFFER
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			// Introduce the indices into the EBO
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


			// Configure the Vertex Attribute so that OpenGL knows how to read the VBO
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			// Enable the Vertex Attribute so that OpenGL knows to use it
			glEnableVertexAttribArray(0);

			// Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
			// Bind the EBO to 0 so that we don't accidentally modify it
			// MAKE SURE TO UNBIND IT AFTER UNBINDING THE VAO, as the EBO is linked in the VAO
			// This does not apply to the VBO because the VBO is already linked to the VAO during glVertexAttribPointer
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		}


		void RenderPlane::Draw(Shader& shader) const
		{
			// Bind the VAO so OpenGL knows to use it
			glBindVertexArray(VAO);
			// Draw the triangle using the GL_TRIANGLES primitive
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		}


		void RenderPlane::Delete()
		{
			glDeleteVertexArrays(1, &VAO);
			glDeleteBuffers(1, &VBO);
		}
	}
}