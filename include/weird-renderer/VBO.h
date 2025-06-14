#ifndef VBO_CLASS_H
#define VBO_CLASS_H

#include<glm/glm.hpp>
#include<glad/glad.h>
#include<vector>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		// Structure to standardize the vertices used in the meshes
		struct Vertex
		{
			glm::vec3 position;
			glm::vec3 normal;
			glm::vec3 color;
			glm::vec2 texUV;
		};

		class VBO
		{
		public:
			// Reference ID of the Vertex Buffer Object
			GLuint ID;

			VBO() {};
			// Constructor that generates a Vertex Buffer Object and links it to vertices
			VBO(std::vector<Vertex>& vertices);

			// Binds the VBO
			void bind();
			// Unbinds the VBO
			void unbind();
			// Deletes the VBO
			void free();
		};

	}
}

#endif