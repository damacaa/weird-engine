#include "weird-renderer/VAO.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		// Constructor that generates a quadVAO ID
		VAO::VAO()
		{
			glGenVertexArrays(1, &ID);
		}

		// Links a VBO Attribute such as a position or color to the quadVAO
		void VAO::LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset)
		{
			VBO.Bind();
			glVertexAttribPointer(layout, numComponents, type, GL_FALSE, stride, offset);
			glEnableVertexAttribArray(layout);
			VBO.Unbind();
		}

		// Binds the quadVAO
		void VAO::Bind() const
		{
			glBindVertexArray(ID);
		}

		// Unbinds the quadVAO
		void VAO::Unbind() const
		{
			glBindVertexArray(0);
		}

		// Deletes the quadVAO
		void VAO::Delete() const
		{
			glDeleteVertexArrays(1, &ID);
		}
	}
}