#pragma once

#include "core/EditorContext.h"
#include <SDL3/SDL_dialog.h>
#include <string>

namespace WeirdEngine::Editor
{
	class SceneOperations
	{
	public:
		explicit SceneOperations(EditorContext& context)
			: m_ctx(context)
		{
		}

		// Scene file I/O
		void newScene();
		void openLoadSceneDialog();
		void openSaveSceneDialog();
		void saveScenePrompt();
		void loadScene(const std::string& path);
		void saveScene(const std::string& path);
		void pollFileActions();

		// Spawning
		Entity spawnDefaultShape(uint16_t shapeType, vec2 pos);
		Entity spawnCustomShape(const std::string& assetPath, vec2 pos);
		void spawnCluster(const std::string& assetPath, vec2 pos);
		vec2 getCameraCenterWorld() const;

		// Entity / Shape manipulation
		void offsetShapeParameters(Shape& s, vec2 delta);
		vec2 getShapePosition(const Shape& s, Entity e) const;
		vec2 getEntityPosition(Entity e) const;
		void setEntityPosition(Entity e, vec2 newPos);

		// Cluster manipulation
		std::filesystem::path resolveClusterPath(const std::string& assetPath) const;
		vec2 computeClusterCentroid(const ClusterInstance& cluster) const;
		ClusterRestData& getOrCreateClusterRestData(const ClusterInstance& cluster);
		void resetClusterShapeAtPosition(ClusterInstance& cluster, vec2 targetPos);
		void resetClusterToInitialPosition(ClusterInstance& cluster);
		void setClusterPosition(uint32_t cId, vec2 newPos);
		bool reloadCluster(uint32_t clusterId);
		int reloadClustersFromAsset(const std::string& assetPath);
		int reloadAllClusters();
		bool reloadCustomShape(const std::string& assetPath);
		void notifyShapeReloaded(ShapeId id);

		// Grouping
		EditorGroup* findGroup(uint32_t gId);
		EditorGroup& createGroup(const std::string& name = "New Group");
		void groupSelected();
		void ungroupSelected();
		bool canGroupSelected() const;
		bool isEntityInAnyGroup(Entity e) const;
		bool isClusterInAnyGroup(uint32_t cId) const;
		void addEntityToGroup(uint32_t gId, Entity e);
		void addClusterToGroup(uint32_t gId, uint32_t cId);
		void removeFromAllGroups(Entity e);
		void removeClusterFromAllGroups(uint32_t cId);
		void cleanGroups();

		// Constraints
		void syncConstraintClusterMembership(Entity constraintEnt, Entity entA, Entity entB);
		void syncAllConstraintsClusterMembership();

		// Selection & editing commands
		void deleteSelected();
		void duplicateSelected();
		vec2 getSelectedPosition() const;
		void setSelectedPosition(vec2 newPos);

		// Picking & selection helpers
		void pickAtWorld(vec2 worldPos);
		void selectEntity(Entity e, bool multiSelect = false);
		void selectCluster(uint32_t cId, bool multiSelect = false);
		void selectGroup(uint32_t gId);
		void selectItem(const HierarchyItem& item, bool multiSelect);
		void handleHierarchyItemClick(const HierarchyItem& item);
		void applyRangeSelection(const HierarchyItem& fromItem, const HierarchyItem& toItem,
								 bool keepExistingSelection);
		void deselectAll();

		// Simulation
		void toggleSimulation();

		// Formatting
		std::string formatEntityLabel(Entity e, const ClusterInstance* parentCluster = nullptr) const;

	private:
		static void SDLCALL onOpenSceneCallback(void* userdata, const char* const* filelist, int filter);
		static void SDLCALL onSaveSceneCallback(void* userdata, const char* const* filelist, int filter);

		EditorContext& m_ctx;
	};
} // namespace WeirdEngine::Editor
