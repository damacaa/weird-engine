#ifndef VAO_CLASS_H
#define VAO_CLASS_H

#include<glad/glad.h>
#include"VBO.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class VAO
		{
		public:
			// ID reference for the Vertex Array Object
			GLuint ID;
			// Constructor that generates a quadVAO ID
			VAO();

			// Links a VBO Attribute such as a position or color to the quadVAO
			void LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset);
			// Binds the quadVAO
			void bind() const;
			// Unbinds the quadVAO
			void Unbind() const;
			// Deletes the quadVAO
			void free() const;
		};
	}
}

#endif