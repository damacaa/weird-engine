#pragma once
#include "../ECS.h"
#include"../../../weird-renderer/Mesh.h"

constexpr size_t MAX_PATH_LENGTH = 4096;

struct MeshRenderer : public Component {
private:

public:

	MeshRenderer() {};

	MeshRenderer(Mesh mesh) : mesh(mesh) {


	};

	Mesh mesh;
	glm::vec3 translation;
	glm::vec3 scale;
	glm::vec3 rotation;

};

class RenderSystem : public System {

public:
	RenderSystem() {

		entities = std::vector<Entity>();

	}


	void render( ECS& ecs, Shader& shader, Camera& camera, const std::vector<Light>& lights) {

		auto array = manager->getComponentArray<MeshRenderer>();

		const int size = array->size;

		for (size_t i = 0; i < size; i++)
		{
			
			MeshRenderer& mr = array->componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			mr.mesh.Draw(shader, camera, glm::vec3(t.x, t.y, t.z), glm::vec3(0), glm::vec3(1), lights);
		}


		/*for (auto entity : entities) {

			auto& t = ecs.getComponent<Transform>(entity);
			auto& mr = ecs.getComponent<MeshRenderer>(entity);

			mr.mesh.Draw(shader, camera, glm::vec3(t.x,t.y,t.z), glm::vec3(0), glm::vec3(1), lights);
		}*/
	}

};



/*class SmallString {
	static constexpr size_t short_size = 15;
	union {
		char short_str[short_size + 1];
		char* long_str;
	};
	size_t length;
	bool is_short;

public:
	SmallString(const char* str) : length(std::strlen(str)), is_short(length <= short_size) {
		if (is_short) {
			std::strncpy(short_str, str, short_size);
			short_str[short_size] = '\0';
		} else {
			long_str = new char[length + 1];
			std::strcpy(long_str, str);
		}
	}

	~SmallString() {
		if (!is_short) {
			delete[] long_str;
		}
	}

	const char* c_str() const {
		return is_short ? short_str : long_str;
	}
};*/