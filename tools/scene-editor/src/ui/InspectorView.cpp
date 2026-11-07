#include "ui/InspectorView.h"
#include "services/ExternalToolsManager.h"
#include "services/SceneOperations.h"
#include "ui/MaterialEditorView.h"

#include "weird-engine.h"
#include "weird-engine/components/FlyMovement.h"
#include "weird-engine/components/FlyMovement2D.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-renderer/components/Button.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/Light2DComponent.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/Shape.h"
#include "weird-renderer/components/TextRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace WeirdEngine::Editor
{
	void InspectorView::renderInspector(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr)
	{
		const float winW = ImGui::GetIO().DisplaySize.x;
		const float winH = ImGui::GetIO().DisplaySize.y;
		const float panelH = winH - ctx.menuBarHeight; // Full height to window bottom!

		ImGui::SetNextWindowPos(ImVec2(winW - EditorContext::RIGHT_PANEL_WIDTH, ctx.menuBarHeight), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(EditorContext::RIGHT_PANEL_WIDTH, panelH), ImGuiCond_Always);

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
									   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
									   ImGuiWindowFlags_NoCollapse;

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		if (ImGui::Begin("##InspectorPanel", nullptr, flags))
		{
			ImGui::TextDisabled("INSPECTOR");
			ImGui::Separator();

			if (ctx.selectedGroupId != 0)
			{
				renderGroupInspector(ctx, sceneOps);
			}
			else if (ctx.selectedEntities.size() + ctx.selectedClusterIds.size() > 1)
			{
				renderMultiSelectionInspector(ctx, sceneOps);
			}
			else if (ctx.selectedClusterId != 0)
			{
				renderClusterInspector(ctx, sceneOps, toolsMgr);
			}
			else if (ctx.selectedEntity != INVALID_ENTITY)
			{
				renderEntityInspector(ctx, sceneOps, toolsMgr);
			}
			else
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entity or cluster selected.");
				ImGui::Spacing();
				ImGui::TextWrapped("Click in the viewport or hierarchy to select an object.");
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	void InspectorView::renderGroupInspector(EditorContext& ctx, SceneOperations& sceneOps)
	{
		auto* g = ctx.findGroup(ctx.selectedGroupId);
		if (!g)
		{
			ctx.selectedGroupId = 0;
			return;
		}

		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Group (Dummy Container)");
		ImGui::TextDisabled("ID: %u | %zu Entities, %zu Clusters", g->id, g->entities.size(), g->clusterIds.size());
		ImGui::Separator();

		char nameBuf[128];
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", g->name.c_str());
		if (ImGui::InputText("Group Name", nameBuf, sizeof(nameBuf)))
		{
			g->name = nameBuf;
			ctx.isSceneDirty = true;
		}

		ImGui::Spacing();
		if (ImGui::Button("Ungroup (Ctrl+Shift+G)", ImVec2(-1.0f, 26.0f)))
		{
			sceneOps.ungroupSelected();
			return;
		}

		if (sceneOps.canGroupSelected())
		{
			if (ImGui::Button("Add Selected Items to This Group", ImVec2(-1.0f, 24.0f)))
			{
				for (Entity e : ctx.selectedEntities)
					sceneOps.addEntityToGroup(g->id, e);
				for (uint32_t cId : ctx.selectedClusterIds)
					sceneOps.addClusterToGroup(g->id, cId);
			}
		}

		ImGui::Separator();
		ImGui::Text("Contents (%zu items):", g->clusterIds.size() + g->entities.size());

		for (size_t i = 0; i < g->clusterIds.size();)
		{
			uint32_t cId = g->clusterIds[i];
			auto* c = ctx.services->clusters().getById(cId);
			std::string label = c ? ("[Cluster] " + c->name) : ("Cluster #" + std::to_string(cId));
			ImGui::BulletText("%s", label.c_str());
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
			ImGui::PushID(static_cast<int>(1000 + cId));
			if (ImGui::SmallButton("Remove"))
			{
				g->clusterIds.erase(g->clusterIds.begin() + i);
				ctx.isSceneDirty = true;
			}
			else
			{
				++i;
			}
			ImGui::PopID();
		}

		for (size_t i = 0; i < g->entities.size();)
		{
			Entity e = g->entities[i];
			std::string label = sceneOps.formatEntityLabel(e);
			ImGui::BulletText("%s", label.c_str());
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
			ImGui::PushID(static_cast<int>(2000 + e));
			if (ImGui::SmallButton("Remove"))
			{
				g->entities.erase(g->entities.begin() + i);
				ctx.isSceneDirty = true;
			}
			else
			{
				++i;
			}
			ImGui::PopID();
		}

		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Delete Group", ImVec2(-1.0f, 24.0f)))
		{
			sceneOps.ungroupSelected();
		}
	}

	void InspectorView::renderMultiSelectionInspector(EditorContext& ctx, SceneOperations& sceneOps)
	{
		ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Multi-Selection");
		ImGui::TextDisabled("%zu Entities, %zu Clusters selected", ctx.selectedEntities.size(),
							ctx.selectedClusterIds.size());
		ImGui::Separator();

		if (ImGui::Button("Group Selected (Ctrl+G)", ImVec2(-1.0f, 26.0f)))
		{
			sceneOps.groupSelected();
			return;
		}

		if (ctx.selectedEntities.size() == 2 && ctx.selectedClusterIds.empty())
		{
			if (ImGui::Button("Create Spring Between Pair", ImVec2(-1.0f, 24.0f)))
			{
				auto it = ctx.selectedEntities.begin();
				Entity entA = *it++;
				Entity entB = *it;
				Entity springEnt = ctx.registry->createEntity();
				ctx.services->names().setName(springEnt, "Spring_" + std::to_string(entA) + "_" + std::to_string(entB));
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
				return;
			}
			if (ImGui::Button("Create Distance Constraint Between Pair", ImVec2(-1.0f, 24.0f)))
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
				return;
			}
		}

		if (ImGui::Button("Delete Selected (Del)", ImVec2(-1.0f, 24.0f)))
		{
			sceneOps.deleteSelected();
			return;
		}

		if (ImGui::Button("Deselect All (Esc)", ImVec2(-1.0f, 22.0f)))
		{
			sceneOps.deselectAll();
			return;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Selected Items:");

		for (uint32_t cId : ctx.selectedClusterIds)
		{
			auto* c = ctx.services->clusters().getById(cId);
			std::string label = c ? ("[Cluster] " + c->name) : ("Cluster #" + std::to_string(cId));
			ImGui::BulletText("%s", label.c_str());
		}

		for (Entity e : ctx.selectedEntities)
		{
			std::string label = sceneOps.formatEntityLabel(e);
			ImGui::BulletText("%s", label.c_str());
		}
	}

	void InspectorView::renderClusterInspector(EditorContext& ctx, SceneOperations& sceneOps,
											   ExternalToolsManager& toolsMgr)
	{
		auto* cluster = ctx.services->clusters().getById(ctx.selectedClusterId);
		if (!cluster)
		{
			ctx.selectedClusterId = 0;
			return;
		}

		ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Cluster Instance");
		ImGui::TextDisabled("ID: %u | Type: %s", cluster->id, cluster->type.c_str());
		if (!cluster->assetPath.empty())
		{
			ImGui::TextDisabled("Asset: %s", std::filesystem::path(cluster->assetPath).filename().string().c_str());
			float avail = ImGui::GetContentRegionAvail().x;
			if (ImGui::Button("Edit in Cluster Editor", ImVec2(avail * 0.58f - 4.0f, 22.0f)))
			{
				toolsMgr.launchClusterEditor(cluster->assetPath);
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload from File", ImVec2(-1.0f, 22.0f)))
			{
				sceneOps.reloadCluster(cluster->id);
			}
		}
		ImGui::Separator();

		// Cluster Name
		std::snprintf(ctx.clusterNameBuf, sizeof(ctx.clusterNameBuf), "%s", cluster->name.c_str());
		if (ImGui::InputText("Name", ctx.clusterNameBuf, sizeof(ctx.clusterNameBuf)))
		{
			cluster->name = ctx.clusterNameBuf;
			ctx.isSceneDirty = true;
		}

		// Reset Cluster Buttons
		ImGui::Spacing();
		if (ImGui::Button("Reset to Initial Position (R)", ImVec2(-1.0f, 26.0f)))
		{
			sceneOps.resetClusterToInitialPosition(*cluster);
		}
		if (ImGui::Button("Reset Shape at Current Centroid", ImVec2(-1.0f, 24.0f)))
		{
			vec2 centroid = sceneOps.computeClusterCentroid(*cluster);
			sceneOps.resetClusterShapeAtPosition(*cluster, centroid);
		}

		// Cluster Tags
		ImGui::Spacing();
		ImGui::Text("Tags:");
		for (size_t i = 0; i < cluster->tags.size();)
		{
			ImGui::PushID(static_cast<int>(i));
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[%s]", cluster->tags[i].c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("x"))
			{
				cluster->tags.erase(cluster->tags.begin() + i);
				ctx.isSceneDirty = true;
			}
			else
			{
				ImGui::SameLine();
				++i;
			}
			ImGui::PopID();
		}

		ImGui::NewLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputTextWithHint("##ClusterAddTag", "New tag...", ctx.newTagBuf, sizeof(ctx.newTagBuf));
		ImGui::SameLine();
		if (ImGui::Button("Add Tag") && ctx.newTagBuf[0] != '\0')
		{
			cluster->tags.push_back(ctx.newTagBuf);
			ctx.newTagBuf[0] = '\0';
			ctx.isSceneDirty = true;
		}

		ImGui::Separator();

		// Transform (Centroid position)
		float pos[2] = {cluster->position.x, cluster->position.y};
		if (ImGui::DragFloat2("Position", pos, 0.1f))
		{
			sceneOps.setSelectedPosition(vec2(pos[0], pos[1]));
			ctx.isSceneDirty = true;
		}

		// Cluster Materials
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Collect unique materials used across member entities
			std::map<uint16_t, int> usedMaterials;
			for (Entity e : cluster->entities)
			{
				if (ctx.registry->hasComponent<Shape>(e))
				{
					usedMaterials[ctx.registry->getComponent<Shape>(e).material]++;
				}
				if (ctx.registry->hasComponent<Dot>(e))
				{
					usedMaterials[static_cast<uint16_t>(ctx.registry->getComponent<Dot>(e).materialId)]++;
				}
				if (ctx.registry->hasComponent<TextRenderer>(e))
				{
					usedMaterials[ctx.registry->getComponent<TextRenderer>(e).material]++;
				}
			}

			// 1. Quick Reassign All palette
			ImGui::Text("Quick Assign All Shapes:");
			const float swatchSize = 22.0f;
			const float spacing = 3.0f;
			for (int i = 0; i < 16; ++i)
			{
				if (i > 0 && i % 8 != 0)
				{
					ImGui::SameLine(0.0f, spacing);
				}
				ImGui::PushID(i);
				const auto& mat = ctx.services->materials2D().get(static_cast<uint16_t>(i));
				if (ImGui::ColorButton(
						"##ClusterAssignAllSwatch", ImVec4(mat.color.r, mat.color.g, mat.color.b, mat.color.a),
						ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(swatchSize, swatchSize)))
				{
					MaterialEditorView::setClusterAllMaterials(*cluster, static_cast<uint16_t>(i), ctx);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Assign ALL entities in cluster to [%d] %s", i,
									  mat.name.empty() ? ("Material " + std::to_string(i)).c_str() : mat.name.c_str());
				}
				ImGui::PopID();
			}

			ImGui::Spacing();

			// 2. Used Materials in cluster & individual remapping
			if (usedMaterials.empty())
			{
				ImGui::TextDisabled("No shapes with materials in cluster.");
			}
			else
			{
				ImGui::Text("Used Materials (%zu):", usedMaterials.size());
				for (const auto& [matId, count] : usedMaterials)
				{
					ImGui::PushID(static_cast<int>(matId));
					const auto& srcMat = ctx.services->materials2D().get(matId);
					ImGui::ColorButton(
						"##src_swatch", ImVec4(srcMat.color.r, srcMat.color.g, srcMat.color.b, srcMat.color.a),
						ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(18.0f, 18.0f));
					ImGui::SameLine();
					std::string srcLabel =
						MaterialEditorView::getMaterialName(matId, ctx) + " (" + std::to_string(count) + ")";
					ImGui::TextUnformatted(srcLabel.c_str());

					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.0f);
					std::string comboPreview = MaterialEditorView::getMaterialName(matId, ctx);
					if (ImGui::BeginCombo("##remap_combo", comboPreview.c_str()))
					{
						for (int t = 0; t < 16; ++t)
						{
							ImGui::PushID(t);
							const auto& tMat = ctx.services->materials2D().get(static_cast<uint16_t>(t));
							ImGui::ColorButton(
								"##combo_swatch", ImVec4(tMat.color.r, tMat.color.g, tMat.color.b, tMat.color.a),
								ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14.0f, 14.0f));
							ImGui::SameLine();
							std::string tName = MaterialEditorView::getMaterialName(static_cast<uint16_t>(t), ctx);
							bool isSelected = (matId == static_cast<uint16_t>(t));
							if (ImGui::Selectable(tName.c_str(), isSelected))
							{
								if (static_cast<uint16_t>(t) != matId)
								{
									MaterialEditorView::reassignClusterMaterial(*cluster, matId,
																				static_cast<uint16_t>(t), ctx);
								}
							}
							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
							ImGui::PopID();
						}
						ImGui::EndCombo();
					}
					ImGui::PopID();
				}
			}

			ImGui::Spacing();
			if (ImGui::Button("Customize Scene Materials...", ImVec2(-1.0f, 22.0f)))
			{
				ctx.selectMaterialsTab = true;
			}
		}

		// Member Entities & Local Names (clickable to inspect individual entities)
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Member Entities", ImGuiTreeNodeFlags_DefaultOpen))
		{
			std::unordered_map<Entity, std::string> entityToLocalName;
			for (const auto& [localName, entity] : cluster->localNames)
			{
				entityToLocalName[entity] = localName;
			}

			for (Entity e : cluster->entities)
			{
				std::string entLabel;
				auto itName = entityToLocalName.find(e);
				if (itName != entityToLocalName.end())
				{
					entLabel = itName->second + " (#" + std::to_string(static_cast<uint32_t>(e)) + ")";
				}
				else
				{
					entLabel = "Entity #" + std::to_string(static_cast<uint32_t>(e));
				}

				if (ctx.registry->hasComponent<RigidBody2D>(e))
				{
					auto& rb = ctx.registry->getComponent<RigidBody2D>(e);
					if (rb.type == BodyType::Fixed || rb.isFixed)
					{
						entLabel += " [Fixed]";
					}
					else
					{
						entLabel += " [m=" + std::to_string(rb.mass).substr(0, 4) + "]";
					}
				}

				if (ImGui::Selectable((entLabel + "##c_insp_" + std::to_string(static_cast<uint32_t>(e))).c_str(),
									  false))
				{
					ctx.selectedEntity = e;
					ctx.selectedClusterId = 0;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Click to inspect and edit components for this entity.");
				}
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Destroy Cluster", ImVec2(-1.0f, 26.0f)))
		{
			sceneOps.deleteSelected();
		}
	}

	void InspectorView::renderEntityInspector(EditorContext& ctx, SceneOperations& sceneOps,
											  ExternalToolsManager& toolsMgr)
	{
		Entity e = ctx.selectedEntity;
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Entity #%u", e.id);

		auto* parentCluster = ctx.services->clusters().getClusterForEntity(e);
		if (parentCluster)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Belongs to Cluster: %s", parentCluster->name.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Select Cluster##from_ent"))
			{
				ctx.selectedClusterId = parentCluster->id;
				ctx.selectedEntity = INVALID_ENTITY;
				return;
			}

			for (const auto& [localName, ent] : parentCluster->localNames)
			{
				if (ent == e)
				{
					ImGui::TextDisabled("Local Name: '%s'", localName.c_str());
					break;
				}
			}
		}
		ImGui::Separator();

		// 1. Identity: Unique Name
		std::string currentName = ctx.services->names().getName(e);
		std::snprintf(ctx.entityNameBuf, sizeof(ctx.entityNameBuf), "%s", currentName.c_str());
		if (ImGui::InputText("Unique Name", ctx.entityNameBuf, sizeof(ctx.entityNameBuf)))
		{
			if (ctx.entityNameBuf[0] == '\0')
			{
				ctx.services->names().removeName(e);
			}
			else
			{
				ctx.services->names().setName(e, ctx.entityNameBuf);
			}
			ctx.isSceneDirty = true;
		}

		// 2. Identity: Tags
		ImGui::Spacing();
		ImGui::Text("Tags:");
		std::vector<std::string> tags = ctx.services->tags().getTags(e);
		for (const auto& tag : tags)
		{
			ImGui::PushID(tag.c_str());
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[%s]", tag.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("x"))
			{
				ctx.services->tags().removeTag(e, tag);
				ctx.isSceneDirty = true;
			}
			else
			{
				ImGui::SameLine();
			}
			ImGui::PopID();
		}

		ImGui::NewLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::InputTextWithHint("##EntityAddTag", "New tag...", ctx.newTagBuf, sizeof(ctx.newTagBuf));
		ImGui::SameLine();
		if (ImGui::Button("Add Tag") && ctx.newTagBuf[0] != '\0')
		{
			ctx.services->tags().addTag(e, ctx.newTagBuf);
			ctx.newTagBuf[0] = '\0';
			ctx.isSceneDirty = true;
		}

		ImGui::Separator();

		// 3. Transform
		if (ctx.registry->hasComponent<Transform>(e))
		{
			if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& t = ctx.registry->getComponent<Transform>(e);
				float p[3] = {t.position.x, t.position.y, t.position.z};
				if (ImGui::DragFloat3("Position", p, 0.1f))
				{
					vec2 delta(p[0] - t.position.x, p[1] - t.position.y);
					t.position = vec3(p[0], p[1], p[2]);
					ctx.registry->setComponentDirty(t);
					if (ctx.registry->hasComponent<Shape>(e))
					{
						auto& s = ctx.registry->getComponent<Shape>(e);
						sceneOps.offsetShapeParameters(s, delta);
						ctx.registry->setComponentDirty(s);
					}
					if (ctx.registry->hasComponent<RigidBody2D>(e))
					{
						ctx.simulation2D->setPosition(ctx.registry->getComponent<RigidBody2D>(e).simulationId,
													  vec2(p[0], p[1]));
					}
					ctx.isSceneDirty = true;
				}

				float r[3] = {t.rotation.x, t.rotation.y, t.rotation.z};
				if (ImGui::DragFloat3("Rotation", r, 0.05f))
				{
					t.rotation = vec3(r[0], r[1], r[2]);
					ctx.registry->setComponentDirty(t);
					ctx.isSceneDirty = true;
				}

				float s[3] = {t.scale.x, t.scale.y, t.scale.z};
				if (ImGui::DragFloat3("Scale", s, 0.05f))
				{
					t.scale = vec3(s[0], s[1], s[2]);
					ctx.registry->setComponentDirty(t);
					ctx.isSceneDirty = true;
				}

				if (e != ctx.services->render().getCameraEntity())
				{
					if (ImGui::Button("Remove Transform", ImVec2(-1.0f, 22.0f)))
					{
						ctx.registry->getComponentManager<Transform>()->removeData(e);
						ctx.registry->freeRemovedComponents();
						ctx.isSceneDirty = true;
					}
				}
			}
		}

		// 4. Shape Component
		if (ctx.registry->hasComponent<Shape>(e))
		{
			if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& s = ctx.registry->getComponent<Shape>(e);

				// Shape ID
				int shapeId = static_cast<int>(s.distanceFieldId);
				if (ImGui::InputInt("DistanceField ID", &shapeId))
				{
					s.distanceFieldId = static_cast<uint16_t>(shapeId);
					ctx.registry->setComponentDirty(s);
					ctx.worldRenderContext->shapesNeedUpdate = true;
					ctx.isSceneDirty = true;
				}

				// CSG Combination
				const char* combos[] = {"Addition", "Subtraction", "Intersection", "SmoothAddition",
										"SmoothSubtraction"};
				int combIdx = static_cast<int>(s.combination);
				if (ImGui::Combo("CSG Mode", &combIdx, combos, IM_ARRAYSIZE(combos)))
				{
					s.combination = static_cast<CombinationType>(combIdx);
					ctx.registry->setComponentDirty(s);
					ctx.worldRenderContext->shapesNeedUpdate = true;
					ctx.isSceneDirty = true;
				}

				if (combIdx >= 3)
				{
					if (ImGui::SliderFloat("Smooth Factor", &s.smoothFactor, 0.01f, 10.0f))
					{
						s.smoothFactor = std::max(0.01f, s.smoothFactor);
						ctx.registry->setComponentDirty(s);
						ctx.worldRenderContext->shapesNeedUpdate = true;
						ctx.isSceneDirty = true;
					}
				}

				// Material ID
				if (MaterialEditorView::renderMaterialSelector("Material Slot", s.material, ctx))
				{
					ctx.registry->setComponentDirty(s);
					ctx.worldRenderContext->shapesNeedUpdate = true;
					ctx.isSceneDirty = true;
				}

				// Collision
				if (ImGui::Checkbox("Collisions Enabled", &s.hasCollisions))
				{
					ctx.registry->setComponentDirty(s);
					ctx.isSceneDirty = true;
				}

				// Parameters 0..7
				ImGui::Text("Parameters:");
				const auto* customData = ctx.services->shapes().getCustomShapeData(s.distanceFieldId);
				if (customData)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("Reset Defaults"))
					{
						std::copy(customData->defaultParameters.begin(), customData->defaultParameters.end(),
								  s.parameters);
						ctx.registry->setComponentDirty(s);
						ctx.worldRenderContext->shapesNeedUpdate = true;
						ctx.isSceneDirty = true;
					}
					if (!customData->filePath.empty())
					{
						float avail = ImGui::GetContentRegionAvail().x;
						if (ImGui::Button("Edit in Shape Editor", ImVec2(avail * 0.65f - 4.0f, 22.0f)))
						{
							toolsMgr.launchShapeEditor(customData->filePath);
						}
						ImGui::SameLine();
						if (ImGui::Button("Reload Shape", ImVec2(-1.0f, 22.0f)))
						{
							sceneOps.reloadCustomShape(customData->filePath);
						}
					}
				}

				for (int p = 0; p < 8; ++p)
				{
					std::string paramLabel = "Param " + std::to_string(p);
					if (s.distanceFieldId == DefaultShapes::Circle)
					{
						if (p == DefaultShapes::Circle::PosX)
							paramLabel = "Pos X";
						else if (p == DefaultShapes::Circle::PosY)
							paramLabel = "Pos Y";
						else if (p == DefaultShapes::Circle::Radius)
							paramLabel = "Radius";
					}
					else if (s.distanceFieldId == DefaultShapes::Box)
					{
						if (p == DefaultShapes::Box::PosX)
							paramLabel = "Pos X";
						else if (p == DefaultShapes::Box::PosY)
							paramLabel = "Pos Y";
						else if (p == DefaultShapes::Box::SizeX)
							paramLabel = "Size X";
						else if (p == DefaultShapes::Box::SizeY)
							paramLabel = "Size Y";
					}
					else if (s.distanceFieldId == DefaultShapes::RoundBox)
					{
						if (p == DefaultShapes::RoundBox::PosX)
							paramLabel = "Pos X";
						else if (p == DefaultShapes::RoundBox::PosY)
							paramLabel = "Pos Y";
						else if (p == DefaultShapes::RoundBox::SizeX)
							paramLabel = "Size X";
						else if (p == DefaultShapes::RoundBox::SizeY)
							paramLabel = "Size Y";
						else if (p == DefaultShapes::RoundBox::Radius)
							paramLabel = "Radius";
					}
					else if (s.distanceFieldId == DefaultShapes::Triangle)
					{
						if (p == DefaultShapes::Triangle::PosX)
							paramLabel = "Pos X";
						else if (p == DefaultShapes::Triangle::PosY)
							paramLabel = "Pos Y";
						else if (p == DefaultShapes::Triangle::Width)
							paramLabel = "Width";
						else if (p == DefaultShapes::Triangle::Height)
							paramLabel = "Height";
					}
					else if (s.distanceFieldId == DefaultShapes::Line)
					{
						if (p == DefaultShapes::Line::StartX)
							paramLabel = "Start X";
						else if (p == DefaultShapes::Line::StartY)
							paramLabel = "Start Y";
						else if (p == DefaultShapes::Line::EndX)
							paramLabel = "End X";
						else if (p == DefaultShapes::Line::EndY)
							paramLabel = "End Y";
						else if (p == DefaultShapes::Line::Width)
							paramLabel = "Width";
					}
					else if (s.distanceFieldId == DefaultShapes::Ramp)
					{
						if (p == DefaultShapes::Ramp::PosX)
							paramLabel = "Pos X";
						else if (p == DefaultShapes::Ramp::PosY)
							paramLabel = "Pos Y";
						else if (p == DefaultShapes::Ramp::Width)
							paramLabel = "Width";
						else if (p == DefaultShapes::Ramp::Height)
							paramLabel = "Height";
						else if (p == DefaultShapes::Ramp::Skew)
							paramLabel = "Skew";
					}
					else if (s.distanceFieldId == DefaultShapes::Star)
					{
						if (p == 0)
							paramLabel = "Pos X";
						else if (p == 1)
							paramLabel = "Pos Y";
						else if (p == 2)
							paramLabel = "Radius";
						else if (p == 3)
							paramLabel = "Displace";
						else if (p == 4)
							paramLabel = "Points";
						else if (p == 5)
							paramLabel = "Speed";
					}
					else if (customData && !customData->parameterNames[p].empty())
					{
						paramLabel = customData->parameterNames[p];
					}

					float oldVal = s.parameters[p];
					if (ImGui::DragFloat(paramLabel.c_str(), &s.parameters[p], 0.05f))
					{
						float delta = s.parameters[p] - oldVal;
						if (ctx.registry->hasComponent<Transform>(e))
						{
							auto& t = ctx.registry->getComponent<Transform>(e);
							if (p == 0 && s.distanceFieldId != DefaultShapes::Line)
							{
								t.position.x += delta;
								ctx.registry->setComponentDirty(t);
							}
							else if (p == 1 && s.distanceFieldId != DefaultShapes::Line)
							{
								t.position.y += delta;
								ctx.registry->setComponentDirty(t);
							}
						}
						ctx.registry->setComponentDirty(s);
						ctx.isSceneDirty = true;
					}
				}

				if (ImGui::Button("Remove Shape", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<Shape>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.worldRenderContext->shapesNeedUpdate = true;
					ctx.isSceneDirty = true;
				}
			}
		}

		// 5. RigidBody2D Component
		if (ctx.registry->hasComponent<RigidBody2D>(e))
		{
			if (ImGui::CollapsingHeader("RigidBody2D", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& rb = ctx.registry->getComponent<RigidBody2D>(e);

				const char* bodyTypes[] = {"Dynamic", "Kinematic", "Fixed"};
				int btype = static_cast<int>(rb.type);
				if (ImGui::Combo("Body Type", &btype, bodyTypes, IM_ARRAYSIZE(bodyTypes)))
				{
					rb.type = static_cast<BodyType>(btype);
					rb.isFixed = (rb.type == BodyType::Fixed);
					ctx.registry->setComponentDirty(rb);
					ctx.simulation2D->setBodyType(rb.simulationId, rb.type);
					ctx.isSceneDirty = true;
				}

				if (ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 1000.0f))
				{
					ctx.registry->setComponentDirty(rb);
					ctx.simulation2D->setMass(rb.simulationId, rb.mass);
					ctx.isSceneDirty = true;
				}

				// Quick fix/unfix toggle
				if (rb.type != BodyType::Fixed)
				{
					if (ImGui::Button("Fix in Place (Fixed)", ImVec2(-1.0f, 22.0f)))
					{
						rb.type = BodyType::Fixed;
						rb.isFixed = true;
						ctx.registry->setComponentDirty(rb);
						ctx.simulation2D->setBodyType(rb.simulationId, BodyType::Fixed);
						ctx.isSceneDirty = true;
					}
				}
				else
				{
					if (ImGui::Button("Unfix (Dynamic)", ImVec2(-1.0f, 22.0f)))
					{
						rb.type = BodyType::Dynamic;
						rb.isFixed = false;
						ctx.registry->setComponentDirty(rb);
						ctx.simulation2D->setBodyType(rb.simulationId, BodyType::Dynamic);
						ctx.isSceneDirty = true;
					}
				}

				if (ImGui::Checkbox("Collision", &rb.enableCollision))
				{
					ctx.registry->setComponentDirty(rb);
					ctx.simulation2D->setCollisionEnabled(rb.simulationId, rb.enableCollision);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Checkbox("Shape Collision", &rb.enableShapeCollision))
				{
					ctx.registry->setComponentDirty(rb);
					ctx.simulation2D->setShapeCollisionEnabled(rb.simulationId, rb.enableShapeCollision);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove RigidBody", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<RigidBody2D>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 6. Dot Component
		if (ctx.registry->hasComponent<Dot>(e))
		{
			if (ImGui::CollapsingHeader("Dot", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& dot = ctx.registry->getComponent<Dot>(e);
				if (ImGui::Checkbox("Static Dot", &dot.isStatic))
				{
					ctx.registry->setComponentDirty(dot);
					ctx.isSceneDirty = true;
				}

				if (MaterialEditorView::renderMaterialSelector("Dot Material", dot.materialId, ctx))
				{
					ctx.registry->setComponentDirty(dot);
					ctx.worldRenderContext->shapesNeedUpdate = true;
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove Dot", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<Dot>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 7. Spring Component
		if (ctx.registry->hasComponent<Spring>(e))
		{
			if (ImGui::CollapsingHeader("Spring", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& spring = ctx.registry->getComponent<Spring>(e);

				int entA = (spring.entityA == INVALID_ENTITY) ? -1 : static_cast<int>(spring.entityA);
				int entB = (spring.entityB == INVALID_ENTITY) ? -1 : static_cast<int>(spring.entityB);

				std::string nameA = (spring.entityA != INVALID_ENTITY && ctx.registry->isEntityAlive(spring.entityA))
										? sceneOps.formatEntityLabel(spring.entityA)
										: "None";
				std::string nameB = (spring.entityB != INVALID_ENTITY && ctx.registry->isEntityAlive(spring.entityB))
										? sceneOps.formatEntityLabel(spring.entityB)
										: "None";

				ImGui::Text("Connected Entities:");
				if (ImGui::InputInt("Entity A", &entA))
				{
					spring.entityA = (entA < 0) ? INVALID_ENTITY : static_cast<Entity>(entA);
					ctx.registry->setComponentDirty(spring);
					sceneOps.syncConstraintClusterMembership(e, spring.entityA, spring.entityB);
					ctx.isSceneDirty = true;
				}
				ImGui::TextDisabled("A: %s", nameA.c_str());

				if (ImGui::InputInt("Entity B", &entB))
				{
					spring.entityB = (entB < 0) ? INVALID_ENTITY : static_cast<Entity>(entB);
					ctx.registry->setComponentDirty(spring);
					sceneOps.syncConstraintClusterMembership(e, spring.entityA, spring.entityB);
					ctx.isSceneDirty = true;
				}
				ImGui::TextDisabled("B: %s", nameB.c_str());

				if (spring.entityA != INVALID_ENTITY && spring.entityB != INVALID_ENTITY &&
					ctx.registry->isEntityAlive(spring.entityA) && ctx.registry->isEntityAlive(spring.entityB))
				{
					vec2 posA = sceneOps.getEntityPosition(spring.entityA);
					vec2 posB = sceneOps.getEntityPosition(spring.entityB);
					float curDist = glm::distance(posA, posB);
					ImGui::Text("Current Distance: %.2f", curDist);
					ImGui::SameLine();
					if (ImGui::SmallButton("Set Rest to Current"))
					{
						spring.restDistance = curDist;
						ctx.registry->setComponentDirty(spring);
						ctx.isSceneDirty = true;
					}
				}

				if (ImGui::DragFloat("Stiffness", &spring.stiffness, 0.05f, 0.0f, 1000.0f))
				{
					ctx.registry->setComponentDirty(spring);
					ctx.isSceneDirty = true;
				}

				if (ImGui::DragFloat("Rest Distance", &spring.restDistance, 0.05f, 0.0f, 1000.0f))
				{
					ctx.registry->setComponentDirty(spring);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove Spring", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<Spring>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 8. DistanceConstraint Component
		if (ctx.registry->hasComponent<DistanceConstraint>(e))
		{
			if (ImGui::CollapsingHeader("Distance Constraint", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& dc = ctx.registry->getComponent<DistanceConstraint>(e);

				int entA = (dc.entityA == INVALID_ENTITY) ? -1 : static_cast<int>(dc.entityA);
				int entB = (dc.entityB == INVALID_ENTITY) ? -1 : static_cast<int>(dc.entityB);

				std::string nameA = (dc.entityA != INVALID_ENTITY && ctx.registry->isEntityAlive(dc.entityA))
										? sceneOps.formatEntityLabel(dc.entityA)
										: "None";
				std::string nameB = (dc.entityB != INVALID_ENTITY && ctx.registry->isEntityAlive(dc.entityB))
										? sceneOps.formatEntityLabel(dc.entityB)
										: "None";

				ImGui::Text("Connected Entities:");
				if (ImGui::InputInt("Entity A", &entA))
				{
					dc.entityA = (entA < 0) ? INVALID_ENTITY : static_cast<Entity>(entA);
					ctx.registry->setComponentDirty(dc);
					sceneOps.syncConstraintClusterMembership(e, dc.entityA, dc.entityB);
					ctx.isSceneDirty = true;
				}
				ImGui::TextDisabled("A: %s", nameA.c_str());

				if (ImGui::InputInt("Entity B", &entB))
				{
					dc.entityB = (entB < 0) ? INVALID_ENTITY : static_cast<Entity>(entB);
					ctx.registry->setComponentDirty(dc);
					sceneOps.syncConstraintClusterMembership(e, dc.entityA, dc.entityB);
					ctx.isSceneDirty = true;
				}
				ImGui::TextDisabled("B: %s", nameB.c_str());

				if (dc.entityA != INVALID_ENTITY && dc.entityB != INVALID_ENTITY &&
					ctx.registry->isEntityAlive(dc.entityA) && ctx.registry->isEntityAlive(dc.entityB))
				{
					vec2 posA = sceneOps.getEntityPosition(dc.entityA);
					vec2 posB = sceneOps.getEntityPosition(dc.entityB);
					float curDist = glm::distance(posA, posB);
					ImGui::Text("Current Distance: %.2f", curDist);
					ImGui::SameLine();
					if (ImGui::SmallButton("Set Distance to Current"))
					{
						dc.distance = curDist;
						ctx.registry->setComponentDirty(dc);
						ctx.isSceneDirty = true;
					}
				}

				if (ImGui::DragFloat("Target Distance", &dc.distance, 0.05f, 0.0f, 1000.0f))
				{
					dc.distance = std::max(0.0f, dc.distance);
					ctx.registry->setComponentDirty(dc);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove Distance Constraint", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<DistanceConstraint>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 9. Light2D Component
		if (ctx.registry->hasComponent<Light2DComponent>(e))
		{
			if (ImGui::CollapsingHeader("Light 2D", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& light = ctx.registry->getComponent<Light2DComponent>(e);

				const char* lightTypes[] = {"Directional", "Point", "Cone"};
				int typeIdx = static_cast<int>(light.type);
				if (ImGui::Combo("Light Type", &typeIdx, lightTypes, IM_ARRAYSIZE(lightTypes)))
				{
					light.type = static_cast<Light2DType>(typeIdx);
					ctx.registry->setComponentDirty(light);
					ctx.isSceneDirty = true;
				}

				float col[4] = {light.color.r, light.color.g, light.color.b, light.color.a};
				if (ImGui::ColorEdit4("Color & Intensity", col))
				{
					light.color = glm::vec4(col[0], col[1], col[2], col[3]);
					ctx.registry->setComponentDirty(light);
					ctx.isSceneDirty = true;
				}

				if (light.type == Light2DType::Directional || light.type == Light2DType::Cone)
				{
					float dir[2] = {light.direction.x, light.direction.y};
					if (ImGui::DragFloat2("Direction", dir, 0.05f))
					{
						if (glm::length(glm::vec2(dir[0], dir[1])) > 0.001f)
							light.direction = glm::normalize(glm::vec2(dir[0], dir[1]));
						ctx.registry->setComponentDirty(light);
						ctx.isSceneDirty = true;
					}
				}

				if (light.type == Light2DType::Point || light.type == Light2DType::Cone)
				{
					if (ImGui::DragFloat("Radius", &light.radius, 0.5f, 0.1f, 1000.0f))
					{
						ctx.registry->setComponentDirty(light);
						ctx.isSceneDirty = true;
					}
				}

				if (light.type == Light2DType::Cone)
				{
					if (ImGui::SliderFloat("Cone Angle", &light.coneAngle, 1.0f, 180.0f))
					{
						ctx.registry->setComponentDirty(light);
						ctx.isSceneDirty = true;
					}
					if (ImGui::SliderFloat("Cone Penumbra", &light.conePenumbra, 0.0f, 90.0f))
					{
						ctx.registry->setComponentDirty(light);
						ctx.isSceneDirty = true;
					}
				}

				if (ImGui::Checkbox("Cast Shadows", &light.castShadows))
				{
					ctx.registry->setComponentDirty(light);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove Light 2D", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<Light2DComponent>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 10. Camera Component
		if (ctx.registry->hasComponent<ECS::Camera>(e))
		{
			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& cam = ctx.registry->getComponent<ECS::Camera>(e);

				if (ImGui::DragFloat("Near Plane", &cam.nearPlane, 0.05f, 0.001f, 100.0f))
				{
					ctx.registry->setComponentDirty(cam);
					ctx.isSceneDirty = true;
				}
				if (ImGui::DragFloat("Far Plane", &cam.farPlane, 1.0f, 1.0f, 10000.0f))
				{
					ctx.registry->setComponentDirty(cam);
					ctx.isSceneDirty = true;
				}

				if (e != ctx.services->render().getCameraEntity())
				{
					if (ImGui::Button("Remove Camera", ImVec2(-1.0f, 22.0f)))
					{
						ctx.registry->getComponentManager<ECS::Camera>()->removeData(e);
						ctx.registry->freeRemovedComponents();
						ctx.isSceneDirty = true;
					}
				}
				else
				{
					ImGui::TextDisabled("(Active Main Viewport Camera)");
				}
			}
		}

		// 11. TextRenderer Component
		if (ctx.registry->hasComponent<TextRenderer>(e))
		{
			if (ImGui::CollapsingHeader("Text Renderer", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& tr = ctx.registry->getComponent<TextRenderer>(e);

				char textBuf[33];
				std::snprintf(textBuf, sizeof(textBuf), "%s", tr.text.c_str());
				if (ImGui::InputText("Text", textBuf, sizeof(textBuf)))
				{
					tr.text = textBuf;
					ctx.registry->setComponentDirty(tr);
					ctx.isSceneDirty = true;
				}

				if (MaterialEditorView::renderMaterialSelector("Text Material", tr.material, ctx))
				{
					ctx.registry->setComponentDirty(tr);
					ctx.isSceneDirty = true;
				}

				const char* hAligns[] = {"Left", "Center", "Right"};
				int hIdx = static_cast<int>(tr.horizontalAlignment);
				if (ImGui::Combo("Horizontal Align", &hIdx, hAligns, IM_ARRAYSIZE(hAligns)))
				{
					tr.horizontalAlignment = static_cast<TextRenderer::HorizontalAlignment>(hIdx);
					ctx.registry->setComponentDirty(tr);
					ctx.isSceneDirty = true;
				}

				const char* vAligns[] = {"Top", "Center", "Bottom"};
				int vIdx = static_cast<int>(tr.verticalAlignment);
				if (ImGui::Combo("Vertical Align", &vIdx, vAligns, IM_ARRAYSIZE(vAligns)))
				{
					tr.verticalAlignment = static_cast<TextRenderer::VerticalAlignment>(vIdx);
					ctx.registry->setComponentDirty(tr);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove Text Renderer", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<TextRenderer>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		if (ctx.registry->hasComponent<UITextRenderer>(e))
		{
			if (ImGui::CollapsingHeader("UI Text Renderer", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& tr = ctx.registry->getComponent<UITextRenderer>(e);

				char textBuf[33];
				std::snprintf(textBuf, sizeof(textBuf), "%s", tr.text.c_str());
				if (ImGui::InputText("Text##UI", textBuf, sizeof(textBuf)))
				{
					tr.text = textBuf;
					ctx.registry->setComponentDirty(tr);
					ctx.isSceneDirty = true;
				}

				if (MaterialEditorView::renderMaterialSelector("Text Material##UI", tr.material, ctx))
				{
					ctx.registry->setComponentDirty(tr);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove UI Text Renderer", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<UITextRenderer>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 12. FlyMovement Components
		if (ctx.registry->hasComponent<ECS::FlyMovement2D>(e))
		{
			if (ImGui::CollapsingHeader("Fly Movement 2D", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& fly = ctx.registry->getComponent<ECS::FlyMovement2D>(e);
				if (ImGui::DragFloat("Speed", &fly.speed, 0.1f, 0.0f, 100.0f))
				{
					ctx.registry->setComponentDirty(fly);
					ctx.isSceneDirty = true;
				}
				if (ImGui::DragFloat("Scroll Speed", &fly.scrollSpeed, 0.1f, 0.0f, 100.0f))
				{
					ctx.registry->setComponentDirty(fly);
					ctx.isSceneDirty = true;
				}
				if (ImGui::Checkbox("Smooth", &fly.isSmooth))
				{
					ctx.registry->setComponentDirty(fly);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove Fly Movement 2D", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<ECS::FlyMovement2D>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		if (ctx.registry->hasComponent<ECS::FlyMovement>(e))
		{
			if (ImGui::CollapsingHeader("Fly Movement (3D)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& fly = ctx.registry->getComponent<ECS::FlyMovement>(e);
				if (ImGui::DragFloat("Speed", &fly.speed, 0.1f, 0.0f, 100.0f))
				{
					ctx.registry->setComponentDirty(fly);
					ctx.isSceneDirty = true;
				}
				if (ImGui::DragFloat("Sensitivity", &fly.sensitivity, 0.01f, 0.0f, 10.0f))
				{
					ctx.registry->setComponentDirty(fly);
					ctx.isSceneDirty = true;
				}

				if (ImGui::Button("Remove Fly Movement", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<ECS::FlyMovement>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 13. Button Components
		if (ctx.registry->hasComponent<ShapeButton>(e))
		{
			if (ImGui::CollapsingHeader("Shape Button", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& btn = ctx.registry->getComponent<ShapeButton>(e);
				if (ImGui::DragFloat("Modifier Amount", &btn.modifierAmount, 0.05f))
				{
					ctx.registry->setComponentDirty(btn);
					ctx.isSceneDirty = true;
				}
				if (ImGui::DragFloat("Click Padding", &btn.clickPadding, 0.5f, 0.0f, 100.0f))
				{
					ctx.registry->setComponentDirty(btn);
					ctx.isSceneDirty = true;
				}
				ImGui::Text("Hovered: %s", btn.hovered ? "True" : "False");

				if (ImGui::Button("Remove Shape Button", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<ShapeButton>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		if (ctx.registry->hasComponent<ShapeToggle>(e))
		{
			if (ImGui::CollapsingHeader("Shape Toggle", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& tog = ctx.registry->getComponent<ShapeToggle>(e);
				if (ImGui::Checkbox("Active", &tog.active))
				{
					ctx.registry->setComponentDirty(tog);
					ctx.isSceneDirty = true;
				}
				if (ImGui::DragFloat("Modifier Amount", &tog.modifierAmount, 0.05f))
				{
					ctx.registry->setComponentDirty(tog);
					ctx.isSceneDirty = true;
				}
				if (ImGui::DragFloat("Click Padding", &tog.clickPadding, 0.5f, 0.0f, 100.0f))
				{
					ctx.registry->setComponentDirty(tog);
					ctx.isSceneDirty = true;
				}
				ImGui::Text("Hovered: %s", tog.hovered ? "True" : "False");

				if (ImGui::Button("Remove Shape Toggle", ImVec2(-1.0f, 22.0f)))
				{
					ctx.registry->getComponentManager<ShapeToggle>()->removeData(e);
					ctx.registry->freeRemovedComponents();
					ctx.isSceneDirty = true;
				}
			}
		}

		// 14. Attached Scripts and Custom Components
		auto allCompTypes = ctx.registry->getComponentTypes(e);
		for (size_t compID : allCompTypes)
		{
			std::string compName = ctx.registry->getComponentName(compID);

			if (compName == "Transform" || compName == "Shape" || compName == "RigidBody2D" || compName == "Dot" ||
				compName == "UIDot" || compName == "UIShape" || compName == "Spring" ||
				compName == "DistanceConstraint" || compName == "Light2DComponent" || compName == "TextRenderer" ||
				compName == "UITextRenderer" || compName == "Camera" || compName == "ShapeButton" ||
				compName == "ShapeToggle" || compName == "FlyMovement" || compName == "FlyMovement2D")
			{
				continue;
			}

			std::string headerTitle = (compName == "Unknown")
										  ? ("Missing Script (Type ID: " + std::to_string(compID) + ")")
										  : (compName + " (Script)");

			ImGui::PushID(static_cast<int>(compID));
			if (ImGui::CollapsingHeader(headerTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f),
								   compName == "Unknown" ? "Missing script or unregistered component type."
														 : "Custom component / script attached.");
				ImGui::BulletText("Component Name: %s", compName.c_str());
				ImGui::BulletText("Type ID: %zu", compID);
			}
			ImGui::PopID();
		}

		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Delete Entity", ImVec2(-1.0f, 26.0f)))
		{
			sceneOps.deleteSelected();
		}
	}
} // namespace WeirdEngine::Editor
