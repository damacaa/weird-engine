#ifndef EBO_CLASS_H
#define EBO_CLASS_H

#include<glad/glad.h>
#include<vector>
namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class EBO
		{
		public:
			// ID reference of Elements Buffer Object
			GLuint ID;
			EBO() {};
			// Constructor that generates a Elements Buffer Object and links it to indices
			EBO(std::vector<GLuint>& indices);

			// Binds the EBO
			void bind();
			// Unbinds the EBO
			void Unbind();
			// Deletes the EBO
			void free();
		};
	}
}

#endif