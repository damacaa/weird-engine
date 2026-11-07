#pragma once

#include "core/EditorContext.h"
#include <cstdint>
#include <string>

namespace WeirdEngine
{
	struct ClusterInstance;
}

namespace WeirdEngine::Editor
{
	class MaterialEditorView
	{
	public:
		static std::string getMaterialName(uint16_t id, const EditorContext& ctx);
		static bool renderMaterialSelector(const char* label, uint16_t& currentMat, const EditorContext& ctx);
		static bool renderMaterialSelector(const char* label, unsigned int& currentMat, const EditorContext& ctx);
		static void reassignClusterMaterial(ClusterInstance& cluster, uint16_t sourceMat, uint16_t targetMat,
											EditorContext& ctx);
		static void setClusterAllMaterials(ClusterInstance& cluster, uint16_t targetMat, EditorContext& ctx);

		void renderMaterialsTab(EditorContext& ctx);
	};
} // namespace WeirdEngine::Editor
