#include "scene-editor/ui/HierarchyView.h"
#include "scene-editor/services/ExternalToolsManager.h"
#include "scene-editor/services/SceneOperations.h"

#include "weird-engine.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/Spring.h"
#include "weird-renderer/components/Shape.h"

#include <algorithm>
#include <imgui.h>

namespace WeirdEngine::Tools::SceneEd
{
	namespace
	{
		void sortEntitiesForHierarchy(std::vector<Entity>& entities, const EditorContext& ctx)
		{
			const Entity camEntity = ctx.services->render().getCameraEntity();
			std::sort(entities.begin(), entities.end(),
					  [&](Entity a, Entity b)
					  {
						  if (a == camEntity)
							  return true;
						  if (b == camEntity)
							  return false;

						  const bool aShape = ctx.registry->hasComponent<Shape>(a);
						  const bool bShape = ctx.registry->hasComponent<Shape>(b);
						  if (aShape != bShape)
							  return aShape;

						  if (aShape && bShape)
						  {
							  const auto& sa = ctx.registry->getComponent<Shape>(a);
							  const auto& sb = ctx.registry->getComponent<Shape>(b);
							  if (sa.layer != sb.layer)
								  return sa.layer < sb.layer;
							  if (sa.order != sb.order)
								  return sa.order < sb.order;
						  }

						  return a < b;
					  });
		}

		std::string formatShapeHierarchyPrefix(const Shape& s)
		{
			std::string prefix =
				(s.layer == Shape::GLOBAL_LAYER) ? "[Global" : ("[L" + std::to_string(s.layer));
			switch (s.combination)
			{
				case CombinationType::Subtraction:
					prefix += " -";
					break;
				case CombinationType::Intersection:
					prefix += " &";
					break;
				case CombinationType::SmoothAddition:
					prefix += " ~+";
					break;
				case CombinationType::SmoothSubtraction:
					prefix += " ~-";
					break;
				case CombinationType::Addition:
				default:
					break;
			}
			prefix += "] ";
			return prefix;
		}
	} // namespace
	void HierarchyView::renderHierarchyTab(EditorContext& ctx, SceneOperations& sceneOps,
										   ExternalToolsManager& toolsMgr)
	{
		// 1. Search Bar
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##HierSearch", "Filter hierarchy...", ctx.hierarchySearchBuf,
								 sizeof(ctx.hierarchySearchBuf));

		// 2. Toolbar
		if (ImGui::Button("+ New Group"))
		{
			sceneOps.createGroup();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(
				"Editor-only folder to organize and move entities or clusters together (no runtime effect).");
		}
		ImGui::SameLine();
		bool canGroup = sceneOps.canGroupSelected();
		if (!canGroup)
			ImGui::BeginDisabled();
		if (ImGui::Button("Group (Ctrl+G)"))
		{
			sceneOps.groupSelected();
		}
		if (!canGroup)
			ImGui::EndDisabled();

		ImGui::SameLine();
		bool canUngroup = (ctx.selectedGroupId != 0);
		if (!canUngroup)
			ImGui::BeginDisabled();
		if (ImGui::Button("Ungroup"))
		{
			sceneOps.ungroupSelected();
		}
		if (!canUngroup)
			ImGui::EndDisabled();

		size_t selCount = ctx.selectedEntities.size() + ctx.selectedClusterIds.size();
		if (selCount > 1)
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "(%zu selected)", selCount);
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear"))
			{
				sceneOps.deselectAll();
			}
		}

		ImGui::Separator();

		ImGui::BeginChild("##HierarchyList", ImVec2(0.0f, 0.0f), false);

		sceneOps.cleanGroups();
		ctx.visibleHierarchyItems.clear();

		std::string searchStr(ctx.hierarchySearchBuf);
		std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

		std::unordered_set<uint32_t> groupedClusterIds;
		std::unordered_set<Entity> groupedEntities;
		for (const auto& g : ctx.groups)
		{
			for (uint32_t cId : g.clusterIds)
				groupedClusterIds.insert(cId);
			for (Entity e : g.entities)
				groupedEntities.insert(e);
		}

		// A) Groups
		for (auto& group : ctx.groups)
		{
			renderHierarchyGroupItem(group, searchStr, ctx, sceneOps, toolsMgr);
		}

		// B) Root Clusters (clusters not in any group)
		const auto& allClusters = ctx.services->clusters().getAll();
		for (size_t cIdx = 0; cIdx < allClusters.size(); ++cIdx)
		{
			const auto& cluster = allClusters[cIdx];
			if (groupedClusterIds.count(cluster.id) == 0)
			{
				renderHierarchyClusterItem(cluster, cIdx, searchStr, ctx, sceneOps, toolsMgr);
			}
		}

		// C) Root Entities (alive entities not in any cluster and not in any group)
		std::vector<Entity> rootEntities;
		for (Entity e = 0; e < ctx.registry->getEntityCount(); ++e)
		{
			if (!ctx.registry->isEntityAlive(e))
				continue;

			if (ctx.services->clusters().getClusterForEntity(e) != nullptr)
				continue;

			if (groupedEntities.count(e) > 0)
				continue;

			rootEntities.push_back(e);
		}
		sortEntitiesForHierarchy(rootEntities, ctx);

		for (Entity e : rootEntities)
		{
			renderHierarchyEntityItem(e, nullptr, searchStr, ctx, sceneOps);
		}

		// Drop zone below root items to drag grouped entities/clusters back out to Root
		if (const ImGuiPayload* activePayload = ImGui::GetDragDropPayload())
		{
			bool canDropToRoot = false;
			if (activePayload->IsDataType("HIER_ENTITY") && activePayload->DataSize == sizeof(Entity))
			{
				Entity draggedEnt = *static_cast<const Entity*>(activePayload->Data);
				canDropToRoot = (ctx.services->clusters().getClusterForEntity(draggedEnt) == nullptr &&
								 sceneOps.isEntityInAnyGroup(draggedEnt));
			}
			else if (activePayload->IsDataType("HIER_CLUSTER") && activePayload->DataSize == sizeof(uint32_t))
			{
				uint32_t draggedCluster = *static_cast<const uint32_t*>(activePayload->Data);
				canDropToRoot = sceneOps.isClusterInAnyGroup(draggedCluster);
			}

			if (canDropToRoot)
			{
				ImGui::Spacing();
				const float dropW = (std::max)(ImGui::GetContentRegionAvail().x, 40.0f);
				const float dropH = (std::max)(ImGui::GetContentRegionAvail().y, 32.0f);
				ImGui::Dummy(ImVec2(dropW, dropH));

				const ImVec2 mn = ImGui::GetItemRectMin();
				const ImVec2 mx = ImGui::GetItemRectMax();
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
				drawList->AddRectFilled(mn, mx, hovered ? IM_COL32(60, 140, 200, 45) : IM_COL32(120, 120, 130, 22), 4.0f);
				drawList->AddRect(mn, mx, hovered ? IM_COL32(80, 200, 255, 220) : IM_COL32(140, 140, 150, 120), 4.0f);
				const char* dropText = "Drop here to move to Root";
				const ImVec2 textSz = ImGui::CalcTextSize(dropText);
				drawList->AddText(ImVec2(mn.x + (dropW - textSz.x) * 0.5f, mn.y + (dropH - textSz.y) * 0.5f),
								  hovered ? IM_COL32(180, 235, 255, 255) : IM_COL32(180, 180, 190, 190), dropText);

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_ENTITY"))
					{
						Entity e = *static_cast<const Entity*>(payload->Data);
						sceneOps.removeFromAllGroups(e);
					}
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_CLUSTER"))
					{
						uint32_t cId = *static_cast<const uint32_t*>(payload->Data);
						sceneOps.removeClusterFromAllGroups(cId);
					}
					ImGui::EndDragDropTarget();
				}
			}
		}

		// Context menu on empty area
		if (ImGui::BeginPopupContextWindow("HierarchyEmptyContextMenu",
										   ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("+ New Group"))
			{
				sceneOps.createGroup();
			}
			if (ImGui::MenuItem("Deselect All", "Esc", false, ctx.hasSelection()))
			{
				sceneOps.deselectAll();
			}
			ImGui::EndPopup();
		}

		if (ctx.hasPendingShift)
		{
			sceneOps.applyRangeSelection(ctx.shiftAnchorItem, ctx.pendingShiftTarget, ctx.pendingShiftIsCtrl);
			ctx.hasPendingShift = false;
		}

		ImGui::EndChild();
	}

	void HierarchyView::renderHierarchyEntityItem(Entity e, const ClusterInstance* parentCluster,
												  const std::string& searchStr, EditorContext& ctx,
												  SceneOperations& sceneOps)
	{
		const bool hasShape = ctx.registry->hasComponent<Shape>(e);
		std::string label = sceneOps.formatEntityLabel(e, parentCluster);
		if (hasShape)
		{
			label = formatShapeHierarchyPrefix(ctx.registry->getComponent<Shape>(e)) + label;
		}

		if (!searchStr.empty())
		{
			std::string lLower = label;
			std::transform(lLower.begin(), lLower.end(), lLower.begin(), ::tolower);
			if (lLower.find(searchStr) == std::string::npos)
				return;
		}

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (ctx.isEntitySelected(e))
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::PushID(static_cast<int>(e));
		ImGui::TreeNodeEx((void*)(uintptr_t)e, flags, "%s", label.c_str());

		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();

		ctx.visibleHierarchyItems.push_back({HierarchyItemType::Entity, static_cast<uint32_t>(e)});

		if (ImGui::IsItemHovered() && ImGui::GetDragDropPayload() == nullptr)
		{
			auto compIDs = ctx.registry->getComponentTypes(e);
			if (!compIDs.empty())
			{
				ImGui::BeginTooltip();
				if (hasShape)
				{
					const auto& s = ctx.registry->getComponent<Shape>(e);
					auto layerShapes = sceneOps.getShapesInLayer(s.layer);
					auto posIt = std::find(layerShapes.begin(), layerShapes.end(), e);
					int posInLayer = (posIt != layerShapes.end()) ? static_cast<int>(posIt - layerShapes.begin()) + 1 : 0;
					if (s.layer == Shape::GLOBAL_LAYER)
					{
						ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Global Layer | Order: %d / %zu",
										   posInLayer, layerShapes.size());
					}
					else
					{
						ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Layer %u | Order: %d / %zu", s.layer,
										   posInLayer, layerShapes.size());
					}
					ImGui::Separator();
				}
				ImGui::TextDisabled("Attached Components:");
				for (size_t compID : compIDs)
				{
					std::string compName = ctx.registry->getComponentName(compID);
					if (compName == "Unknown")
					{
						ImGui::BulletText("Missing Script / Unknown (Type ID: %zu)", compID);
					}
					else
					{
						ImGui::BulletText("%s", compName.c_str());
					}
				}
				ImGui::EndTooltip();
			}
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			sceneOps.handleHierarchyItemClick({HierarchyItemType::Entity, static_cast<uint32_t>(e)});
		}

		// Drag source (root entities or any shape entity for layer/order reordering)
		if ((parentCluster == nullptr || hasShape) && ImGui::BeginDragDropSource())
		{
			Entity payload = e;
			ImGui::SetDragDropPayload("HIER_ENTITY", &payload, sizeof(Entity));
			ImGui::Text("%s", label.c_str());
			if (hasShape)
			{
				ImGui::TextDisabled("Drop above/below a shape to reorder or change layer");
			}
			ImGui::EndDragDropSource();
		}

		// Drag-and-drop target on shape entities for direct CSG order & layer reordering
		if (hasShape && ImGui::BeginDragDropTarget())
		{
			const ImGuiDragDropFlags dropFlags =
				ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_ENTITY", dropFlags))
			{
				Entity srcEntity = *static_cast<const Entity*>(payload->Data);
				if (srcEntity != e && ctx.registry->isEntityAlive(srcEntity) &&
					ctx.registry->hasComponent<Shape>(srcEntity))
				{
					const float midY = (itemMin.y + itemMax.y) * 0.5f;
					const bool insertAfter = (ImGui::GetMousePos().y >= midY);
					const float lineY = insertAfter ? itemMax.y : itemMin.y;

					ImDrawList* drawList = ImGui::GetWindowDrawList();
					drawList->AddLine(ImVec2(itemMin.x, lineY), ImVec2(itemMax.x, lineY),
									  IM_COL32(80, 200, 255, 255), 2.5f);

					if (payload->IsDelivery())
					{
						sceneOps.reorderShapeRelativeTo(srcEntity, e, insertAfter);

						// Keep editor group membership consistent when both entities are standalone
						if (parentCluster == nullptr &&
							ctx.services->clusters().getClusterForEntity(srcEntity) == nullptr)
						{
							const uint32_t dstGroupId = sceneOps.getEntityGroupId(e);
							const uint32_t srcGroupId = sceneOps.getEntityGroupId(srcEntity);
							if (dstGroupId != srcGroupId)
							{
								if (dstGroupId != 0)
								{
									sceneOps.addEntityToGroup(dstGroupId, srcEntity);
								}
								else
								{
									sceneOps.removeFromAllGroups(srcEntity);
								}
							}
						}
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Context menu
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Select"))
			{
				sceneOps.selectEntity(e, false);
			}
			if (hasShape)
			{
				const auto& s = ctx.registry->getComponent<Shape>(e);
				auto layerShapes = sceneOps.getShapesInLayer(s.layer);
				auto posIt = std::find(layerShapes.begin(), layerShapes.end(), e);
				const size_t pos = (posIt != layerShapes.end()) ? static_cast<size_t>(posIt - layerShapes.begin()) : 0;

				ImGui::Separator();
				if (ImGui::MenuItem("Move Earlier in Layer", nullptr, false, pos > 0))
				{
					sceneOps.moveShapeOrderInLayer(e, -1);
				}
				if (ImGui::MenuItem("Move Later in Layer", nullptr, false,
									!layerShapes.empty() && pos + 1 < layerShapes.size()))
				{
					sceneOps.moveShapeOrderInLayer(e, +1);
				}
				if (ImGui::BeginMenu("Set Layer"))
				{
					auto usedLayers = sceneOps.getUsedShapeLayers();
					uint16_t maxRegular = 0;
					bool hasRegular = false;
					for (uint16_t l : usedLayers)
					{
						if (l != Shape::GLOBAL_LAYER)
						{
							maxRegular = hasRegular ? (std::max)(maxRegular, l) : l;
							hasRegular = true;
							std::string layerLabel = "Layer " + std::to_string(l);
							if (ImGui::MenuItem(layerLabel.c_str(), nullptr, s.layer == l))
							{
								sceneOps.setShapeLayer(e, l);
							}
						}
					}
					const uint16_t nextLayer = hasRegular ? static_cast<uint16_t>(maxRegular + 1) : 0;
					if (nextLayer < Shape::GLOBAL_LAYER)
					{
						ImGui::Separator();
						std::string nextLabel = "+ New Layer (" + std::to_string(nextLayer) + ")";
						if (ImGui::MenuItem(nextLabel.c_str()))
						{
							sceneOps.setShapeLayer(e, nextLayer);
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Global Layer", nullptr, s.layer == Shape::GLOBAL_LAYER))
					{
						sceneOps.setShapeLayer(e, Shape::GLOBAL_LAYER);
					}
					ImGui::EndMenu();
				}
			}
			if (sceneOps.canGroupSelected())
			{
				if (ImGui::MenuItem("Group Selected (Ctrl+G)"))
				{
					sceneOps.groupSelected();
				}
			}
			if (ctx.selectedEntities.size() == 2)
			{
				ImGui::Separator();
				if (ImGui::MenuItem("Create Spring Between Selected (2)"))
				{
					auto it = ctx.selectedEntities.begin();
					Entity entA = *it++;
					Entity entB = *it;
					Entity springEnt = ctx.registry->createEntity();
					ctx.services->names().setName(springEnt,
												  "Spring_" + std::to_string(entA) + "_" + std::to_string(entB));
					auto& sp = ctx.registry->addComponent<Spring>(springEnt);
					sp.entityA = entA;
					sp.entityB = entB;
					vec2 posA = sceneOps.getEntityPosition(entA);
					vec2 posB = sceneOps.getEntityPosition(entB);
					sp.restDistance = glm::distance(posA, posB);
					sp.stiffness = 1.0f;
					ctx.registry->setComponentDirty(sp);
					sceneOps.syncConstraintClusterMembership(springEnt, entA, entB);
					sceneOps.selectEntity(springEnt, false);
					ctx.isSceneDirty = true;
				}
				if (ImGui::MenuItem("Create Distance Constraint Between Selected (2)"))
				{
					auto it = ctx.selectedEntities.begin();
					Entity entA = *it++;
					Entity entB = *it;
					Entity dcEnt = ctx.registry->createEntity();
					ctx.services->names().setName(dcEnt, "DistanceConstraint_" + std::to_string(entA) + "_" +
															 std::to_string(entB));
					auto& dc = ctx.registry->addComponent<DistanceConstraint>(dcEnt);
					dc.entityA = entA;
					dc.entityB = entB;
					vec2 posA = sceneOps.getEntityPosition(entA);
					vec2 posB = sceneOps.getEntityPosition(entB);
					dc.distance = glm::distance(posA, posB);
					ctx.registry->setComponentDirty(dc);
					sceneOps.syncConstraintClusterMembership(dcEnt, entA, entB);
					sceneOps.selectEntity(dcEnt, false);
					ctx.isSceneDirty = true;
				}
			}
			if (parentCluster == nullptr && !ctx.groups.empty())
			{
				if (ImGui::BeginMenu("Add to Group"))
				{
					for (const auto& g : ctx.groups)
					{
						if (ImGui::MenuItem(g.name.c_str()))
						{
							sceneOps.addEntityToGroup(g.id, e);
						}
					}
					ImGui::EndMenu();
				}
			}
			if (sceneOps.isEntityInAnyGroup(e))
			{
				if (ImGui::MenuItem("Remove from Group"))
				{
					sceneOps.removeFromAllGroups(e);
				}
			}
			if (e != ctx.services->render().getCameraEntity())
			{
				ImGui::Separator();
				if (ImGui::MenuItem("Delete Entity", "Del"))
				{
					sceneOps.selectEntity(e, false);
					sceneOps.deleteSelected();
				}
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	void HierarchyView::renderHierarchyClusterItem(const ClusterInstance& cluster, size_t clusterIdx,
												   const std::string& searchStr, EditorContext& ctx,
												   SceneOperations& sceneOps, ExternalToolsManager& toolsMgr)
	{
		if (!searchStr.empty())
		{
			std::string cLower = cluster.name + " " + cluster.type;
			std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
			bool anyMatch = (cLower.find(searchStr) != std::string::npos);
			if (!anyMatch)
			{
				for (Entity member : cluster.entities)
				{
					std::string mLabel = sceneOps.formatEntityLabel(member, &cluster);
					if (ctx.registry->hasComponent<Shape>(member))
					{
						mLabel = formatShapeHierarchyPrefix(ctx.registry->getComponent<Shape>(member)) + mLabel;
					}
					std::transform(mLabel.begin(), mLabel.end(), mLabel.begin(), ::tolower);
					if (mLabel.find(searchStr) != std::string::npos)
					{
						anyMatch = true;
						break;
					}
				}
			}
			if (!anyMatch)
				return;
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (ctx.isClusterSelected(cluster.id))
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		std::string clusterLabel = "[Cluster] " + cluster.name + " (" + cluster.type + ")";

		ImGui::PushID(static_cast<int>(100000 + cluster.id * 1000 + clusterIdx));
		bool open = ImGui::TreeNodeEx((void*)(uintptr_t)(100000 + cluster.id * 1000 + clusterIdx), flags, "%s",
									  clusterLabel.c_str());

		ctx.visibleHierarchyItems.push_back({HierarchyItemType::Cluster, cluster.id});

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			sceneOps.handleHierarchyItemClick({HierarchyItemType::Cluster, cluster.id});
		}

		// Drag source
		if (ImGui::BeginDragDropSource())
		{
			uint32_t payload = cluster.id;
			ImGui::SetDragDropPayload("HIER_CLUSTER", &payload, sizeof(uint32_t));
			ImGui::Text("[Cluster] %s", cluster.name.c_str());
			ImGui::EndDragDropSource();
		}

		// Context menu
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Select Cluster"))
			{
				sceneOps.selectCluster(cluster.id, false);
			}
			if (!cluster.assetPath.empty())
			{
				if (ImGui::MenuItem("Reload from File"))
				{
					sceneOps.reloadCluster(cluster.id);
				}
				if (ImGui::MenuItem("Edit in Cluster Editor"))
				{
					toolsMgr.launchClusterEditor(cluster.assetPath);
				}
			}
			if (sceneOps.canGroupSelected())
			{
				if (ImGui::MenuItem("Group Selected (Ctrl+G)"))
				{
					sceneOps.groupSelected();
				}
			}
			if (!ctx.groups.empty())
			{
				if (ImGui::BeginMenu("Add to Group"))
				{
					for (const auto& g : ctx.groups)
					{
						if (ImGui::MenuItem(g.name.c_str()))
						{
							sceneOps.addClusterToGroup(g.id, cluster.id);
						}
					}
					ImGui::EndMenu();
				}
			}
			if (sceneOps.isClusterInAnyGroup(cluster.id))
			{
				if (ImGui::MenuItem("Remove from Group"))
				{
					sceneOps.removeClusterFromAllGroups(cluster.id);
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Destroy Cluster", "Del"))
			{
				sceneOps.selectCluster(cluster.id, false);
				sceneOps.deleteSelected();
			}
			ImGui::EndPopup();
		}

		if (open)
		{
			std::vector<Entity> sortedMembers;
			sortedMembers.reserve(cluster.entities.size());
			for (Entity e : cluster.entities)
			{
				if (ctx.registry->isEntityAlive(e))
				{
					sortedMembers.push_back(e);
				}
			}
			sortEntitiesForHierarchy(sortedMembers, ctx);
			for (Entity e : sortedMembers)
			{
				renderHierarchyEntityItem(e, &cluster, searchStr, ctx, sceneOps);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	void HierarchyView::renderHierarchyGroupItem(EditorGroup& group, const std::string& searchStr, EditorContext& ctx,
												 SceneOperations& sceneOps, ExternalToolsManager& toolsMgr)
	{
		if (!searchStr.empty())
		{
			std::string gLower = group.name;
			std::transform(gLower.begin(), gLower.end(), gLower.begin(), ::tolower);
			bool match = (gLower.find(searchStr) != std::string::npos);
			if (!match)
			{
				for (uint32_t cId : group.clusterIds)
				{
					auto* c = ctx.services->clusters().getById(cId);
					if (c)
					{
						std::string cLower = c->name;
						std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
						if (cLower.find(searchStr) != std::string::npos)
						{
							match = true;
							break;
						}
					}
				}
				if (!match)
				{
					for (Entity e : group.entities)
					{
						std::string eLower = sceneOps.formatEntityLabel(e);
						if (ctx.registry->hasComponent<Shape>(e))
						{
							eLower = formatShapeHierarchyPrefix(ctx.registry->getComponent<Shape>(e)) + eLower;
						}
						std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
						if (eLower.find(searchStr) != std::string::npos)
						{
							match = true;
							break;
						}
					}
				}
			}
			if (!match)
				return;
		}

		std::string groupLabel = "[Group] " + group.name + "##g_" + std::to_string(group.id);
		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

		ImGui::PushID(static_cast<int>(500000 + group.id));
		bool open = ImGui::TreeNodeEx((void*)(uintptr_t)(500000 + group.id), flags, "%s", groupLabel.c_str());

		ctx.visibleHierarchyItems.push_back({HierarchyItemType::Group, group.id});

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			sceneOps.handleHierarchyItemClick({HierarchyItemType::Group, group.id});
		}

		// Drag-and-drop target to drop items into this group
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_ENTITY"))
			{
				Entity e = *static_cast<const Entity*>(payload->Data);
				if (ctx.services->clusters().getClusterForEntity(e) == nullptr)
				{
					sceneOps.addEntityToGroup(group.id, e);
				}
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_CLUSTER"))
			{
				uint32_t cId = *static_cast<const uint32_t*>(payload->Data);
				sceneOps.addClusterToGroup(group.id, cId);
			}
			ImGui::EndDragDropTarget();
		}

		// Context menu
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Select Group"))
			{
				sceneOps.selectGroup(group.id);
			}
			if (ctx.hasSelection())
			{
				if (ImGui::MenuItem("Add Selected Items to This Group"))
				{
					for (Entity e : ctx.selectedEntities)
						sceneOps.addEntityToGroup(group.id, e);
					for (uint32_t cId : ctx.selectedClusterIds)
						sceneOps.addClusterToGroup(group.id, cId);
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G"))
			{
				sceneOps.selectGroup(group.id);
				sceneOps.ungroupSelected();
			}
			ImGui::EndPopup();
		}

		if (open)
		{
			for (size_t cIdx = 0; cIdx < group.clusterIds.size(); ++cIdx)
			{
				uint32_t cId = group.clusterIds[cIdx];
				auto* c = ctx.services->clusters().getById(cId);
				if (c)
				{
					renderHierarchyClusterItem(*c, cIdx, searchStr, ctx, sceneOps, toolsMgr);
				}
			}

			std::vector<Entity> sortedGroupEntities;
			sortedGroupEntities.reserve(group.entities.size());
			for (Entity e : group.entities)
			{
				if (ctx.registry->isEntityAlive(e))
				{
					sortedGroupEntities.push_back(e);
				}
			}
			sortEntitiesForHierarchy(sortedGroupEntities, ctx);

			for (Entity e : sortedGroupEntities)
			{
				renderHierarchyEntityItem(e, nullptr, searchStr, ctx, sceneOps);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}
} // namespace WeirdEngine::Tools::SceneEd
