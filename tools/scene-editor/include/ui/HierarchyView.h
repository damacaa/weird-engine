#pragma once

#include "core/EditorContext.h"
#include <string>

namespace WeirdEngine
{
	struct ClusterInstance;
}

namespace WeirdEngine::Editor
{
	class SceneOperations;
	class ExternalToolsManager;

	class HierarchyView
	{
	public:
		void renderHierarchyTab(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);

	private:
		void renderHierarchyEntityItem(Entity e, const ClusterInstance* parentCluster, const std::string& searchStr,
									   EditorContext& ctx, SceneOperations& sceneOps);
		void renderHierarchyClusterItem(const ClusterInstance& cluster, size_t clusterIdx, const std::string& searchStr,
										EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);
		void renderHierarchyGroupItem(EditorGroup& group, const std::string& searchStr, EditorContext& ctx,
									  SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);
	};
} // namespace WeirdEngine::Editor
