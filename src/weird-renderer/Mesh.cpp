#include "weird-renderer/Mesh.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		Mesh::Mesh(MeshID id, std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<Texture>& textures)
			: id(id)
		{
			Mesh::vertices = vertices;
			Mesh::indices = indices;
			Mesh::textures = textures;

			m_vao.bind();
			// Generates Vertex Buffer Object and links it to vertices
			m_vbo = VBO(vertices);
			// Generates Element Buffer Object and links it to indices
			m_ebo = EBO(indices);
			// Links VBO attributes such as coordinates and colors to quadVAO
			m_vao.LinkAttrib(m_vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
			m_vao.LinkAttrib(m_vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
			m_vao.LinkAttrib(m_vbo, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
			m_vao.LinkAttrib(m_vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

			// Unbind all to prevent accidentally modifying them
			m_vao.Unbind();
			m_vbo.unbind();
			m_ebo.Unbind();
		}

		Mesh::~Mesh()
		{
			vertices.clear();
		}

		const auto RIGHT = glm::vec3(1.0f, 0.0f, 0.0f);
		const auto UP = glm::vec3(0.0f, 1.0f, 0.0f);
		const auto FORWARD = glm::vec3(0.0f, 0.0f, 1.0f);

		void Mesh::draw(
			Shader& shader,
			Camera& camera,
			glm::vec3 translation,
			glm::vec3 rotation,
			glm::vec3 scale
		) const
		{
			UploadUniforms(shader, camera, translation, rotation, scale);

			// Draw the actual mesh
			glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		}

		void Mesh::drawInstances(
			Shader& shader,
			Camera& camera,
			unsigned int instances,
			glm::vec3 translation,
			glm::vec3 rotation,
			glm::vec3 scale) const
		{
			UploadUniforms(shader, camera, translation, rotation, scale);

			// Draw the actual mesh
			glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0, instances);
		}

		void Mesh::free()
		{
			m_vao.free();
			m_vbo.free();
			m_ebo.free();
		}

		void Mesh::UploadUniforms(Shader& shader, Camera& camera, glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale) const
		{
			// Bind shader to be able to access uniforms
			shader.use();
			m_vao.bind();

			// Keep track of how many of each type of textures we have
			unsigned int numDiffuse = 0;
			unsigned int numSpecular = 0;

			for (unsigned int i = 0; i < textures.size(); i++)
			{
				std::string num;
				std::string type = textures[i].type;
				unsigned int unit;
				if (type == "diffuse")
				{
					num = std::to_string(numDiffuse++);
					unit = 0;
				}
				else if (type == "specular")
				{
					num = std::to_string(numSpecular++);
					unit = 1;
				}

				textures[i].bind(i);
				textures[i].texUnit(shader, ("t_" + type + num).c_str(), unit);
			}

			// Compute model matrix
			glm::mat4 model = glm::translate(glm::mat4(1.0f), translation);

			if (m_isBillboard)
			{
				// Remove rotation and make the model face the camera
				glm::mat3 billboardRotation = glm::mat3(camera.view); // extract rotation
				// TODO: buffer inverted view for every billboard
				billboardRotation = glm::transpose(billboardRotation); // invert rotation
				model *= glm::mat4(billboardRotation);
			}
			else
			{
				model = glm::rotate(model, rotation.x, RIGHT);
				model = glm::rotate(model, rotation.y, UP);
				model = glm::rotate(model, rotation.z, FORWARD);
			}

			model = glm::scale(model, scale);

			// Send model matrix to shader
			glUniformMatrix4fv(glGetUniformLocation(shader.ID, "u_model"), 1, GL_FALSE, glm::value_ptr(model));

			// Compute and send normal matrix
			glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
			glUniformMatrix3fv(glGetUniformLocation(shader.ID, "u_normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
		}
	}
}