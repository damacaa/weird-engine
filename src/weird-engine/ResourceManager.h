#pragma once
#include "../weird-renderer/Mesh.h"
#include "ecs/Entity.h"
#include <json/json.h>
#include <set>
#include <unordered_set>

using json = nlohmann::json;

using MeshID = std::uint32_t;

class ResourceManager {
private:

	const std::string MISSING_TEXTURE = "missing";
	const std::string DIFFUSE = "diffuse";
	const std::string SPECULAR = "specular";

	const std::list<std::string> m_diffuseNames{
		"diffuse",
		"Diffuse",
		"Albedo",
		"albedo"
	};

	MeshID m_meshCount = 0;

	std::map<std::string, MeshID> m_meshPathMap;
	std::map<MeshID, Mesh*> m_meshIdMap;

	std::map<Entity, std::unordered_set<MeshID>> m_resourcesUsedByEntity;
	std::map<MeshID, unsigned int> m_resourceReferenceCount;

	unsigned int m_loadedMeshesCount = 0;
	std::map<std::string, Texture> m_textureMap;
	unsigned int m_textureCount = 0;

	std::vector<unsigned char> m_data;
	json m_json;


	// Loads a single mesh by its index
	void loadMesh(const char* file, unsigned int indMesh, std::vector<Mesh*>& meshes);

	// Traverses a node recursively, so it essentially traverses all connected nodes
	void traverseNode(const char* file, unsigned int nextNode, std::vector<Mesh*>& meshes, glm::mat4 matrix = glm::mat4(1.0f));

	// Gets the binary data from a file
	std::vector<unsigned char> getData(const char* file);
	// Interprets the binary data into floats, indices, and textures
	std::vector<float> getFloats(json accessor);
	std::vector<GLuint> getIndices(json accessor);
	std::vector<Texture> getTextures(const char* file);
	void addDefaultTextures(std::vector<Texture>& textures);


	// Assembles all the floats into vertices
	std::vector<Vertex> assembleVertices(
		std::vector<glm::vec3> positions,
		std::vector<glm::vec3> normals,
		std::vector<glm::vec2> texUVs
	);

	// Helps with the assembly from above by grouping floats
	std::vector<glm::vec2> groupFloatsVec2(std::vector<float> floatVec);
	std::vector<glm::vec3> groupFloatsVec3(std::vector<float> floatVec);
	std::vector<glm::vec4> groupFloatsVec4(std::vector<float> floatVec);

	Texture& getTexture(std::string path, std::string textureType, GLuint slot);

public:

	ResourceManager();

	MeshID getMeshId(const char* path, const Entity entity, bool instancing = false);
	Mesh& getMesh(const MeshID id);
	void freeResources(const Entity entity);
};

