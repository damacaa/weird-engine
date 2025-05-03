#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include<string>
#include<json/json.h>

#include"VAO.h"
#include"EBO.h"
#include"Camera.h"
#include"Texture.h"
#include "Light.h"
#include "../weird-engine/ecs/ComponentManager.h"
#include "../weird-engine/ecs/Components/Transform.h"


namespace WeirdEngine
{
using namespace ECS;
	namespace WeirdRenderer
	{

		using MeshID = std::uint32_t;

		class Mesh
		{
		public:
			MeshID id;
			std::vector <Vertex> vertices;
			std::vector <GLuint> indices;
			std::vector <Texture> textures;
			// Store quadVAO in public so it can be used in the Draw function
			VAO m_vao;
			VBO m_vbo;
			EBO m_ebo;


			Mesh() {
				int b = 0;
			};

			// Initializes the mesh
			Mesh(MeshID id, std::vector <Vertex>& vertices, std::vector <GLuint>& indices, std::vector <Texture>& textures);
			~Mesh();

			// Draws the mesh
			void Draw
			(
				Shader& shader,
				Camera& camera,
				glm::vec3 translation,
				glm::vec3 rotation,
				glm::vec3 scale,
				const std::vector<Light>& lights
			) const;

			// Draws the mesh
			void DrawInstances
			(
				Shader& shader,
				Camera& camera,
				unsigned int instances,
				glm::vec3 translation,
				glm::vec3 rotation,
				glm::vec3 scale,
				const std::vector<Light>& lights
			) const;

			void Delete();

			bool m_isBillboard = false;

		private:

			void UploadUniforms
			(
				Shader& shader,
				Camera& camera,
				glm::vec3 translation,
				glm::vec3 rotation,
				glm::vec3 scale,
				const std::vector<Light>& lights
			) const;


			
		};
	}
}

#endif