#pragma once
#include "weird-renderer/resources/Mesh.h"

namespace WeirdEngine
{
	constexpr size_t MAX_PATH_LENGTH = 4096;
	using WeirdRenderer::MeshID;

	struct MeshRenderer
	{
		MeshID mesh = 0;
		int materialIndex = 0;
	};
} // namespace WeirdEngine
