#include "weird-renderer/EBO.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		// Constructor that generates a Elements Buffer Object and links it to indices
		EBO::EBO(std::vector<GLuint>& indices)
		{
			glGenBuffers(1, &ID);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
		}

		// Binds the EBO
		void EBO::bind()
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
		}

		// Unbinds the EBO
		void EBO::Unbind()
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}

		// Deletes the EBO
		void EBO::free()
		{
			glDeleteBuffers(1, &ID);
		}
	}
}