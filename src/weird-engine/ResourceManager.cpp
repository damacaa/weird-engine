#include "weird-engine/ResourceManager.h"

namespace WeirdEngine
{
	ResourceManager::ResourceManager()
	{
		//Texture defaultDiffuse = Texture(glm::vec4(255, 0, 255, 255), DIFFUSE, 0);
		//m_textureMap[MISSING_TEXTURE] = defaultDiffuse;
	}



	MeshID ResourceManager::getMeshId(const char* file, const Entity entity, bool instancing)
	{
		// If file exists
		if (m_meshPathMap.find(file) != m_meshPathMap.end()) {
			auto id = m_meshPathMap[file];

			// And id is loaded
			if (m_meshIdMap.find(id) != m_meshIdMap.end()) {
				// Return existing id
				m_resourceReferenceCount[id]++;
				m_resourcesUsedByEntity[id].insert(entity);
				return id;
			}
		}

		// New mesh

		// Make a JSON object
		std::string text = get_file_contents(file);
		m_json = json::parse(text);

		// Get the binary data
		m_data = getData(file);

		// All the meshes and transformations
		std::vector<Mesh*> meshes;
		std::vector<glm::vec3> translationsMeshes;
		std::vector<glm::quat> rotationsMeshes;
		std::vector<glm::vec3> scalesMeshes;
		std::vector<glm::mat4> matricesMeshes;

		// Prevents textures from being loaded twice
		std::vector<std::string> loadedTexName;
		std::vector<Texture> loadedTex;


		// Traverse all nodes
		traverseNode(file, 0, meshes);

		auto* mesh = meshes[0];
		MeshID id = m_meshCount++;

		m_meshPathMap[file] = id;
		m_meshIdMap[id] = mesh;

		m_resourceReferenceCount[id] = 1;
		m_resourcesUsedByEntity[id].insert(entity);

		m_json = NULL;
		m_data.clear();

		return id;
	}

	Mesh& ResourceManager::getMesh(const MeshID id)
	{
		return *m_meshIdMap[id];
	}

	void ResourceManager::freeResources(const Entity entity)
	{
		std::unordered_set<MeshID>& resources = m_resourcesUsedByEntity[entity];
		for (auto& resourceId : resources) {
			if (--m_resourceReferenceCount[resourceId] == 0) {
				// Remove resource from memory

				m_meshIdMap[resourceId]->Delete();
				delete m_meshIdMap[resourceId];
				m_meshIdMap.erase(resourceId);

			}
		}


		resources.clear();
		m_resourcesUsedByEntity.erase(entity);

	}

	void ResourceManager::loadMesh(const char* file, unsigned int indMesh, std::vector<Mesh*>& meshes)
	{
		// Get all accessor indices
		unsigned int posAccInd = m_json["meshes"][indMesh]["primitives"][0]["attributes"]["POSITION"];
		unsigned int normalAccInd = m_json["meshes"][indMesh]["primitives"][0]["attributes"]["NORMAL"];
		unsigned int texAccInd = m_json["meshes"][indMesh]["primitives"][0]["attributes"]["TEXCOORD_0"];
		unsigned int indAccInd = m_json["meshes"][indMesh]["primitives"][0]["indices"];

		// Use accessor indices to get all vertices components
		std::vector<float> posVec = getFloats(m_json["accessors"][posAccInd]);
		std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
		std::vector<float> normalVec = getFloats(m_json["accessors"][normalAccInd]);
		std::vector<glm::vec3> normals = groupFloatsVec3(normalVec);
		std::vector<float> texVec = getFloats(m_json["accessors"][texAccInd]);
		std::vector<glm::vec2> texUVs = groupFloatsVec2(texVec);

		// Combine all the vertex components and also get the indices and textures
		std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
		std::vector<GLuint> indices = getIndices(m_json["accessors"][indAccInd]);


		std::vector<Texture> textures = getTextures(file);

		// Combine the vertices, indices, and textures into a mesh
		meshes.push_back(new Mesh(m_loadedMeshesCount++, vertices, indices, textures));
	}

	void ResourceManager::traverseNode(const char* file, unsigned int nextNode, std::vector<Mesh*>& meshes, glm::mat4 matrix)
	{
		// Current node
		json node = m_json["nodes"][nextNode];

		// Get translation if it exists
		glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
		if (node.find("translation") != node.end())
		{
			float transValues[3];
			for (unsigned int i = 0; i < node["translation"].size(); i++)
				transValues[i] = (node["translation"][i]);
			translation = glm::make_vec3(transValues);
		}
		// Get quaternion if it exists
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		if (node.find("rotation") != node.end())
		{
			float rotValues[4] =
			{
				node["rotation"][3],
				node["rotation"][0],
				node["rotation"][1],
				node["rotation"][2]
			};
			rotation = glm::make_quat(rotValues);
		}
		// Get scale if it exists
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
		if (node.find("scale") != node.end())
		{
			float scaleValues[3];
			for (unsigned int i = 0; i < node["scale"].size(); i++)
				scaleValues[i] = (node["scale"][i]);
			scale = glm::make_vec3(scaleValues);
		}
		// Get matrix if it exists
		glm::mat4 matNode = glm::mat4(1.0f);
		if (node.find("matrix") != node.end())
		{
			float matValues[16];
			for (unsigned int i = 0; i < node["matrix"].size(); i++)
				matValues[i] = (node["matrix"][i]);
			matNode = glm::make_mat4(matValues);
		}

		// Initialize matrices
		glm::mat4 trans = glm::mat4(1.0f);
		glm::mat4 rot = glm::mat4(1.0f);
		glm::mat4 sca = glm::mat4(1.0f);

		// Use translation, rotation, and scale to change the initialized matrices
		trans = glm::translate(trans, translation);
		rot = glm::mat4_cast(rotation);
		sca = glm::scale(sca, scale);

		// Multiply all matrices together
		glm::mat4 matNextNode = matrix * matNode * trans * rot * sca;

		// Check if the node contains a mesh and if it does load it
		if (node.find("mesh") != node.end())
		{
			// TODO: restor this
			// translationsMeshes.push_back(translation);
			// rotationsMeshes.push_back(rotation);
			// scalesMeshes.push_back(scale);
			// matricesMeshes.push_back(matNextNode);

			loadMesh(file, node["mesh"], meshes);
		}

		// TODO: retore this
		// Check if the node has children, and if it does, apply this function to them with the matNextNode
		/*if (node.find("children") != node.end())
		{
			for (unsigned int i = 0; i < node["children"].size(); i++)
				traverseNode(file, node["children"][i], matNextNode);
		}*/

	}

	std::vector<unsigned char> ResourceManager::getData(const char* file)
	{
		// Create a place to store the raw text, and get the uri of the .bin file
		std::string bytesText;
		std::string uri = m_json["buffers"][0]["uri"];

		// Store raw text data into bytesText
		std::string fileStr = std::string(file);
		std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);
		bytesText = get_file_contents((fileDirectory + uri).c_str());

		// Transform the raw text data into bytes and put them in a vector
		std::vector<unsigned char> data(bytesText.begin(), bytesText.end());
		return data;
	}

	std::vector<float> ResourceManager::getFloats(json accessor)
	{
		std::vector<float> floatVec;

		// Get properties from the accessor
		unsigned int buffViewInd = accessor.value("bufferView", 1);
		unsigned int count = accessor["count"];
		unsigned int accByteOffset = accessor.value("byteOffset", 0);
		std::string type = accessor["type"];

		// Get properties from the bufferView
		json bufferView = m_json["bufferViews"][buffViewInd];
		unsigned int byteOffset = bufferView["byteOffset"];

		// Interpret the type and store it into numPerVert
		unsigned int numPerVert;
		if (type == "SCALAR") numPerVert = 1;
		else if (type == "VEC2") numPerVert = 2;
		else if (type == "VEC3") numPerVert = 3;
		else if (type == "VEC4") numPerVert = 4;
		else throw std::invalid_argument("Type is invalid (not SCALAR, VEC2, VEC3, or VEC4)");

		// Go over all the bytes in the data at the correct place using the properties from above
		unsigned int beginningOfData = byteOffset + accByteOffset;
		unsigned int lengthOfData = count * 4 * numPerVert;
		for (unsigned int i = beginningOfData; i < beginningOfData + lengthOfData; i)
		{
			unsigned char bytes[] = { m_data[i++], m_data[i++], m_data[i++], m_data[i++] };
			float value;
			std::memcpy(&value, bytes, sizeof(float));
			floatVec.push_back(value);
		}

		return floatVec;
	}

	std::vector<GLuint> ResourceManager::getIndices(json accessor)
	{
		std::vector<GLuint> indices;

		// Get properties from the accessor
		unsigned int buffViewInd = accessor.value("bufferView", 0);
		unsigned int count = accessor["count"];
		unsigned int accByteOffset = accessor.value("byteOffset", 0);
		unsigned int componentType = accessor["componentType"];

		// Get properties from the bufferView
		json bufferView = m_json["bufferViews"][buffViewInd];
		unsigned int byteOffset = bufferView["byteOffset"];

		// Get indices with regards to their type: unsigned int, unsigned short, or short
		unsigned int beginningOfData = byteOffset + accByteOffset;
		if (componentType == 5125)
		{
			for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 4; i)
			{
				unsigned char bytes[] = { m_data[i++], m_data[i++], m_data[i++], m_data[i++] };
				unsigned int value;
				std::memcpy(&value, bytes, sizeof(unsigned int));
				indices.push_back((GLuint)value);
			}
		}
		else if (componentType == 5123)
		{
			for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
			{
				unsigned char bytes[] = { m_data[i++], m_data[i++] };
				unsigned short value;
				std::memcpy(&value, bytes, sizeof(unsigned short));
				indices.push_back((GLuint)value);
			}
		}
		else if (componentType == 5122)
		{
			for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
			{
				unsigned char bytes[] = { m_data[i++], m_data[i++] };
				short value;
				std::memcpy(&value, bytes, sizeof(short));
				indices.push_back((GLuint)value);
			}
		}

		return indices;
	}

	Texture& ResourceManager::getTexture(std::string path, std::string textureType, GLuint slot)
	{
		try
		{
			// Load texture
			Texture t = Texture(path.c_str(), textureType, slot);
			// Store it in map
			m_textureMap[path] = t;
			// Add it to mesh textures
			return m_textureMap[path];
		}
		catch (const std::exception& ex)
		{
			// Couldn't load texture, using missing texture
			return m_textureMap[MISSING_TEXTURE];
		}
	}


	std::vector<Texture> ResourceManager::getTextures(const char* file)
	{
		std::vector<Texture> textures;

		// If mesh doesn't contain any texture, use 1x1 white textures
		if (m_json["images"].size() == 0) {
			std::cout << "No textures";

			std::string key = "default" + DIFFUSE;
			if (m_textureMap.find(key) != m_textureMap.end()) {

				textures.push_back(m_textureMap[key]);
			}
			else {
				Texture defaultDiffuse = Texture(glm::vec4(255), DIFFUSE, textures.size());
				textures.push_back(defaultDiffuse);
				m_textureMap[key] = defaultDiffuse;
			}


			key = "default" + SPECULAR;
			if (m_textureMap.find(key) != m_textureMap.end()) {

				textures.push_back(m_textureMap[key]);
			}
			else {
				Texture defaultSpecular = Texture(glm::vec4(0), SPECULAR, textures.size());
				textures.push_back(defaultSpecular);
				m_textureMap[key] = defaultSpecular;
			}


			return textures;
		}


		bool hasSpecular = false;

		// Load mesh textures
		std::string fileStr = std::string(file);
		std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);
		for (unsigned int i = 0; i < m_json["images"].size(); i++)
		{
			// uri of current texture
			std::string texPath = m_json["images"][i]["uri"];

			if (m_textureMap.find(texPath) != m_textureMap.end()) {
				// Texture already loaded!
				textures.push_back(m_textureMap[texPath]);
				continue;
			}

			// If the texture already exists in map, get it from there
			// Load diffuse texture
			if (texPath.find("baseColor") != std::string::npos || texPath.find("diffuse") != std::string::npos || texPath.find("albedo") != std::string::npos)
			{
				textures.push_back(getTexture((fileDirectory + texPath).c_str(), DIFFUSE, textures.size()));
			}
			// Load defaultSpecular texture
			else if (texPath.find("roughness") != std::string::npos || texPath.find("specular") != std::string::npos)
			{
				textures.push_back(getTexture((fileDirectory + texPath).c_str(), SPECULAR, textures.size()));
				hasSpecular = true;
			}

		}


		if (!hasSpecular) {
			const std::string SPECULAR = "specular";
			std::string key = "default" + SPECULAR;
			if (m_textureMap.find(key) != m_textureMap.end()) {

				textures.push_back(m_textureMap[key]);
			}
			else {
				Texture defaultSpecular = Texture(glm::vec4(0), SPECULAR, textures.size());
				textures.push_back(defaultSpecular);
				m_textureMap[key] = defaultSpecular;
			}
		}

		return textures;
	}

	void ResourceManager::addDefaultTextures(std::vector<Texture>& textures)
	{
		Texture diffuse;

		// Generate a texture object
		GLuint texture;
		glGenTextures(1, &texture);

		// Bind the texture object
		glBindTexture(GL_TEXTURE_2D, texture);

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Define the texture image (1x1 white pixel)
		unsigned char whitePixel[3] = { 255, 255, 255 }; // RGB
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);


	}

	std::vector<Vertex> ResourceManager::assembleVertices
	(
		std::vector<glm::vec3> positions,
		std::vector<glm::vec3> normals,
		std::vector<glm::vec2> texUVs
	)
	{
		std::vector<Vertex> vertices;
		for (int i = 0; i < positions.size(); i++)
		{
			vertices.push_back
			(
				Vertex
				{
					positions[i],
					normals[i],
					glm::vec3(1.0f, 1.0f, 1.0f),
					texUVs[i]
				}
			);
		}
		return vertices;
	}

	std::vector<glm::vec2> ResourceManager::groupFloatsVec2(std::vector<float> floatVec)
	{
		std::vector<glm::vec2> vectors;
		for (int i = 0; i < floatVec.size(); i)
		{
			vectors.push_back(glm::vec2(floatVec[i++], floatVec[i++]));
		}
		return vectors;
	}
	std::vector<glm::vec3> ResourceManager::groupFloatsVec3(std::vector<float> floatVec)
	{
		std::vector<glm::vec3> vectors;
		for (int i = 0; i < floatVec.size(); i)
		{
			vectors.push_back(glm::vec3(floatVec[i++], floatVec[i++], floatVec[i++]));
		}
		return vectors;
	}
	std::vector<glm::vec4> ResourceManager::groupFloatsVec4(std::vector<float> floatVec)
	{
		std::vector<glm::vec4> vectors;
		for (int i = 0; i < floatVec.size(); i)
		{
			vectors.push_back(glm::vec4(floatVec[i++], floatVec[i++], floatVec[i++], floatVec[i++]));
		}
		return vectors;
	}


}