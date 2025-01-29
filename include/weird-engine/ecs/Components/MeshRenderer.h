#pragma once
#include "../Component.h"
#include"../../../weird-renderer/Mesh.h"

constexpr size_t MAX_PATH_LENGTH = 4096;
using WeirdRenderer::MeshID;

struct MeshRenderer : public Component
{
public:
	MeshID mesh;

	MeshRenderer() {};

	MeshRenderer(MeshID mesh) : mesh(mesh) {};
};

