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

		MeshRenderer() {};

		MeshRenderer(MeshID mesh)
			: mesh(mesh) {};
	};
} // namespace WeirdEngine
