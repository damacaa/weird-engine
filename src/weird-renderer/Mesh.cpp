#include "Mesh.h"

Mesh::Mesh(std::vector <Vertex>& vertices, std::vector <GLuint>& indices, std::vector <Texture>& textures)
{
	Mesh::vertices = vertices;
	Mesh::indices = indices;
	Mesh::textures = textures;

	VAO.Bind();
	// Generates Vertex Buffer Object and links it to vertices
	VBO VBO(vertices);
	// Generates Element Buffer Object and links it to indices
	EBO EBO(indices);
	// Links VBO attributes such as coordinates and colors to quadVAO
	VAO.LinkAttrib(VBO, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	VAO.LinkAttrib(VBO, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	VAO.LinkAttrib(VBO, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	VAO.LinkAttrib(VBO, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

	// Unbind all to prevent accidentally modifying them
	VAO.Unbind();
	VBO.Unbind();
	EBO.Unbind();
	
}


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
	shader.Activate();
	VAO.Bind();

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

		textures[i].Bind(i);
		textures[i].texUnit(shader, (type + num).c_str(), i);
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

	matrix = glm::rotate(matrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	matrix = glm::rotate(matrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	matrix = glm::rotate(matrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // TODO: replace with consts

	matrix = glm::scale(matrix, scale);



	// Push the matrices to the vertex shader
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, glm::value_ptr(matrix));

	// Draw the actual mesh
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
}



void Mesh::DrawInstance(Shader& shader, Camera& camera, unsigned int instances, std::vector<glm::vec3> translations, std::vector<glm::vec3> rotations, std::vector<glm::vec3> scales, const std::vector<Light>& lights) const
{
	// Bind shader to be able to access uniforms
	shader.Activate();
	VAO.Bind();

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

		textures[i].Bind(i);
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

	for (size_t i = 0; i < instances; i++)
	{
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

