#include "weird-renderer/Mesh.h"

namespace WeirdRenderer
{
	Mesh::Mesh(MeshID id, std::vector <Vertex>& vertices, std::vector <GLuint>& indices, std::vector <Texture>& textures) :
		id(id)
	{
		Mesh::vertices = vertices;
		Mesh::indices = indices;
		Mesh::textures = textures;

		m_vao.Bind();
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
		m_vbo.Unbind();
		m_ebo.Unbind();

	}

	Mesh::~Mesh()
	{
		vertices.clear();
	}


	const auto RIGHT = glm::vec3(1.0f, 0.0f, 0.0f);
	const auto UP = glm::vec3(0.0f, 1.0f, 0.0f);
	const auto FORWARD = glm::vec3(0.0f, 0.0f, 1.0f);

	void Mesh::Draw
	(
		Shader& shader,
		Camera& camera,
		glm::vec3 translation,
		glm::vec3 rotation,
		glm::vec3 scale,
		const std::vector<Light>& lights
	) const
	{
		// Bind shader to be able to access uniforms
		shader.activate();
		m_vao.Bind();

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
			textures[i].texUnit(shader, (type + num).c_str(), unit);
		}


		// Take care of the camera Matrix
		glUniform3f(glGetUniformLocation(shader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
		camera.Matrix(shader, "camMatrix");

		auto q = glm::quat(rotation);

		glm::vec3 direction = lights[0].rotation;
		// Calculate the inverse (conjugate for normalized quaternions)
		glm::quat inverseQuat = glm::conjugate(q);

		// Convert direction vector to a quaternion with zero w component
		glm::quat directionQuat(0, direction.x, direction.y, direction.z);

		// Apply the inverse rotation: inverseQuat * directionQuat * normalizedQuat
		glm::quat resultQuat = inverseQuat * directionQuat * q;

		// Extract the rotated direction vector
		glm::vec3 rotatedDirection = glm::vec3(resultQuat.x, resultQuat.y, resultQuat.z);


		glUniform3f(glGetUniformLocation(shader.ID, "directionalLightDirection"), rotatedDirection.x, rotatedDirection.y, rotatedDirection.z);


		// Initialize matrices
		/*glm::mat4 trans = glm::mat4(1.0f);
		glm::mat4 rot = glm::mat4(1.0f);
		glm::mat4 sca = glm::mat4(1.0f);

		// Transform the matrices to their correct form
		trans = glm::translate(trans, translation);
		rot = glm::mat4_cast(rotation);
		sca = glm::scale(sca, scale);

		auto matrix = trans * rot * sca;*/


		auto matrix = glm::translate(glm::mat4(1.0f), translation);

		matrix = glm::rotate(matrix, rotation.x, RIGHT);
		matrix = glm::rotate(matrix, rotation.y, UP);
		matrix = glm::rotate(matrix, rotation.z, FORWARD); // TODO: replace with consts

		matrix = glm::scale(matrix, scale);



		// Push the matrices to the vertex shader
		glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, glm::value_ptr(matrix));

		// Draw the actual mesh
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	}



	void Mesh::DrawInstance(Shader& shader, Camera& camera, unsigned int instances, std::vector<glm::vec3> translations, std::vector<glm::vec3> rotations, std::vector<glm::vec3> scales, const std::vector<Light>& lights) const
	{
		// Bind shader to be able to access uniforms
		shader.activate();
		m_vao.Bind();

		// Keep track of how many of each type of textures we have
		unsigned int numDiffuse = 0;
		unsigned int numSpecular = 0;

		for (unsigned int i = 0; i < textures.size(); i++)
		{
			std::string num;
			std::string type = textures[i].type;
			if (type == "diffuse")
			{
				num = std::to_string(numDiffuse++);
			}
			else if (type == "specular")
			{
				num = std::to_string(numSpecular++);
			}

			textures[i].bind(i);
			textures[i].texUnit(shader, (type + num).c_str(), i);
		}


		// Take care of the camera Matrix
		glUniform3f(glGetUniformLocation(shader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
		camera.Matrix(shader, "camMatrix");

		// TODO: how do lights work?
		auto q = glm::quat(rotations[0]);
		glm::vec3 direction = lights[0].rotation;
		// Calculate the inverse (conjugate for normalized quaternions)
		glm::quat inverseQuat = glm::conjugate(q);
		// Convert direction vector to a quaternion with zero w component
		glm::quat directionQuat(0, direction.x, direction.y, direction.z);
		// Apply the inverse rotation: inverseQuat * directionQuat * normalizedQuat
		glm::quat resultQuat = inverseQuat * directionQuat * q;
		// Extract the rotated direction vector
		glm::vec3 rotatedDirection = glm::vec3(resultQuat.x, resultQuat.y, resultQuat.z);

		glUniform3f(glGetUniformLocation(shader.ID, "directionalLightDirection"), rotatedDirection.x, rotatedDirection.y, rotatedDirection.z);


		// Initialize matrices
		/*glm::mat4 trans = glm::mat4(1.0f);
		glm::mat4 rot = glm::mat4(1.0f);
		glm::mat4 sca = glm::mat4(1.0f);

		// Transform the matrices to their correct form
		trans = glm::translate(trans, translation);
		rot = glm::mat4_cast(rotation);
		sca = glm::scale(sca, scale);

		auto matrix = trans * rot * sca;*/




		glm::mat4* models = new glm::mat4[instances];

		// 60 fps drop
		for (size_t i = 0; i < instances; i++)
		{
			//auto matrix = glm::mat4(1.0f);

			auto matrix = glm::translate(glm::mat4(1.0f), translations[i]);

			matrix = glm::rotate(matrix, rotations[i].x, glm::vec3(1.0f, 0.0f, 0.0f));
			matrix = glm::rotate(matrix, rotations[i].y, glm::vec3(0.0f, 1.0f, 0.0f));
			matrix = glm::rotate(matrix, rotations[i].z, glm::vec3(0.0f, 0.0f, 1.0f)); // TODO: replace with consts

			matrix = glm::scale(matrix, scales[i]);

			models[i] = matrix;
		}


		shader.setUniform("u_models", models, instances);

		// Push the matrices to the vertex shader
		//glUniformMatrix4fv(glGetUniformLocation(shader.ID, "u_models"), MESHES_INSTANCIATED, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

		// Draw the actual mesh
		glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0, instances);
	}

	void Mesh::DrawInstance(Shader& shader, Camera& camera, unsigned int instances, std::vector<Transform>& transforms, const std::vector<Light>& lights) const
	{
		// Bind shader to be able to access uniforms
		shader.activate();
		m_vao.Bind();

		// Keep track of how many of each type of textures we have
		unsigned int numDiffuse = 0;
		unsigned int numSpecular = 0;

		for (unsigned int i = 0; i < textures.size(); i++)
		{
			std::string num;
			std::string type = textures[i].type;
			if (type == "diffuse")
			{
				num = std::to_string(numDiffuse++);
			}
			else if (type == "specular")
			{
				num = std::to_string(numSpecular++);
			}

			textures[i].bind(i);
			textures[i].texUnit(shader, (type + num).c_str(), i);
		}


		// Take care of the camera Matrix
		glUniform3f(glGetUniformLocation(shader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
		camera.Matrix(shader, "camMatrix");

		// TODO: how do lights work?
		auto q = glm::quat(transforms[0].rotation);
		glm::vec3 direction = lights[0].rotation;
		// Calculate the inverse (conjugate for normalized quaternions)
		glm::quat inverseQuat = glm::conjugate(q);
		// Convert direction vector to a quaternion with zero w component
		glm::quat directionQuat(0, direction.x, direction.y, direction.z);
		// Apply the inverse rotation: inverseQuat * directionQuat * normalizedQuat
		glm::quat resultQuat = inverseQuat * directionQuat * q;
		// Extract the rotated direction vector
		glm::vec3 rotatedDirection = glm::vec3(resultQuat.x, resultQuat.y, resultQuat.z);

		glUniform3f(glGetUniformLocation(shader.ID, "directionalLightDirection"), rotatedDirection.x, rotatedDirection.y, rotatedDirection.z);


		// Initialize matrices
		/*glm::mat4 trans = glm::mat4(1.0f);
		glm::mat4 rot = glm::mat4(1.0f);
		glm::mat4 sca = glm::mat4(1.0f);

		// Transform the matrices to their correct form
		trans = glm::translate(trans, translation);
		rot = glm::mat4_cast(rotation);
		sca = glm::scale(sca, scale);

		auto matrix = trans * rot * sca;*/



		const glm::vec3 right(1.0f, 0.0f, 0.0f);
		const glm::vec3 up(1.0f, 0.0f, 0.0f);
		const glm::vec3 forward(1.0f, 0.0f, 0.0f);

		glm::mat4* models = new glm::mat4[instances];

		unsigned int i = 0;
		for (const auto& t : transforms)
		{
			auto matrix = glm::translate(glm::mat4(1.0f), t.position);

			matrix = glm::rotate(matrix, t.rotation.x, right);
			matrix = glm::rotate(matrix, t.rotation.y, up);
			matrix = glm::rotate(matrix, t.rotation.z, forward);

			matrix = glm::scale(matrix, t.scale);

			models[i] = matrix;

			i++;
		}

		/*for (size_t i = 0; i < transforms.getSize(); i++)
		{
			const auto& t = transforms[i];

			auto matrix = glm::translate(glm::mat4(1.0f), t.position);

			matrix = glm::rotate(matrix, t.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
			matrix = glm::rotate(matrix, t.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
			matrix = glm::rotate(matrix, t.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // TODO: replace with consts

			matrix = glm::scale(matrix, t.scale);

			models[i] = matrix;

			i++;
		}*/



		shader.setUniform("u_models", models, instances);

		// Push the matrices to the vertex shader
		//glUniformMatrix4fv(glGetUniformLocation(shader.ID, "u_models"), MESHES_INSTANCIATED, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

		// Draw the actual mesh
		glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0, instances);
	}

	void Mesh::Delete()
	{
		m_vao.Delete();
		m_vbo.Delete();
		m_ebo.Delete();
	}
}
