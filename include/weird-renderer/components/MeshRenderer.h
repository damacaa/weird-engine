#pragma once
#include "weird-engine/ecs/Component.h"
#include "weird-renderer/resources/Mesh.h"

namespace WeirdEngine
{
	constexpr size_t MAX_PATH_LENGTH = 4096;
	using WeirdRenderer::MeshID;

	struct MeshRenderer : public Component
	{
	public:
		MeshID mesh;
		int materialIndex = 0;

		MeshRenderer() {};

		MeshRenderer(MeshID mesh)
			: mesh(mesh) {};

		MeshRenderer(MeshID mesh, int materialIndex)
			: mesh(mesh), materialIndex(materialIndex) {};
	};
} // namespace WeirdEngine
