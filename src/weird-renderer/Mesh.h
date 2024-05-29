#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include<string>
#include<json/json.h>

#include"VAO.h"
#include"EBO.h"
#include"Camera.h"
#include"Texture.h"
#include "Light.h"

class Mesh
{
public:
	std::vector <Vertex> vertices;
	std::vector <GLuint> indices;
	std::vector <Texture> textures;
	// Store quadVAO in public so it can be used in the Draw function
	VAO VAO;
	GLuint UBO;

	// Holds number of m_instances (if 1 the mesh will be rendered normally)
	unsigned int m_instances;

	Mesh() :m_instances(1) {};
	// Initializes the mesh
	Mesh(std::vector <Vertex>& vertices, std::vector <GLuint>& indices, std::vector <Texture>& textures, unsigned int instances);

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
	void DrawInstance
	(
		Shader& shader,
		Camera& camera,
		unsigned int instances,
		std::vector<glm::vec3> translations,
		std::vector<glm::vec3> rotations,
		std::vector<glm::vec3> scales,
		const std::vector<Light>& lights
	) const;

private:

};
#endif