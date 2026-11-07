#include "ui/MaterialEditorView.h"

#include "weird-engine.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/Shape.h"
#include "weird-renderer/components/TextRenderer.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace WeirdEngine::Editor
{
	std::string MaterialEditorView::getMaterialName(uint16_t id, const EditorContext& ctx)
	{
		if (id >= 16)
			id = 15;
		const auto& mat = ctx.services->materials2D().get(id);
		if (!mat.name.empty())
			return "[" + std::to_string(id) + "] " + mat.name;
		return "Material " + std::to_string(id);
	}

	bool MaterialEditorView::renderMaterialSelector(const char* label, uint16_t& currentMat, const EditorContext& ctx)
	{
		bool changed = false;
		uint16_t safeId = currentMat < 16 ? currentMat : 15;
		const auto& curMat = ctx.services->materials2D().get(safeId);

		ImGui::PushID(label);
		ImGui::ColorButton("##cur_swatch", ImVec4(curMat.color.r, curMat.color.g, curMat.color.b, curMat.color.a),
						   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(18.0f, 18.0f));
		ImGui::SameLine();

		std::string comboPreview = getMaterialName(safeId, ctx);
		if (ImGui::BeginCombo(label, comboPreview.c_str()))
		{
			for (int i = 0; i < 16; ++i)
			{
				ImGui::PushID(i);
				const auto& m = ctx.services->materials2D().get(static_cast<uint16_t>(i));
				ImGui::ColorButton("##combo_swatch", ImVec4(m.color.r, m.color.g, m.color.b, m.color.a),
								   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
								   ImVec2(14.0f, 14.0f));
				ImGui::SameLine();
				bool isSel = (safeId == static_cast<uint16_t>(i));
				if (ImGui::Selectable(getMaterialName(static_cast<uint16_t>(i), ctx).c_str(), isSel))
				{
					currentMat = static_cast<uint16_t>(i);
					changed = true;
				}
				if (isSel)
				{
					ImGui::SetItemDefaultFocus();
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		return changed;
	}

	bool MaterialEditorView::renderMaterialSelector(const char* label, unsigned int& currentMat,
													const EditorContext& ctx)
	{
		uint16_t m = static_cast<uint16_t>(currentMat);
		if (renderMaterialSelector(label, m, ctx))
		{
			currentMat = static_cast<unsigned int>(m);
			return true;
		}
		return false;
	}

	void MaterialEditorView::reassignClusterMaterial(ClusterInstance& cluster, uint16_t sourceMat, uint16_t targetMat,
													 EditorContext& ctx)
	{
		bool changed = false;
		for (Entity e : cluster.entities)
		{
			if (ctx.registry->hasComponent<Shape>(e))
			{
				auto& s = ctx.registry->getComponent<Shape>(e);
				if (s.material == sourceMat)
				{
					s.material = targetMat;
					ctx.registry->setComponentDirty(s);
					changed = true;
				}
			}
			if (ctx.registry->hasComponent<Dot>(e))
			{
				auto& dot = ctx.registry->getComponent<Dot>(e);
				if (dot.materialId == sourceMat)
				{
					dot.materialId = static_cast<unsigned int>(targetMat);
					ctx.registry->setComponentDirty(dot);
					changed = true;
				}
			}
			if (ctx.registry->hasComponent<TextRenderer>(e))
			{
				auto& tr = ctx.registry->getComponent<TextRenderer>(e);
				if (tr.material == sourceMat)
				{
					tr.material = targetMat;
					ctx.registry->setComponentDirty(tr);
					changed = true;
				}
			}
		}
		if (changed)
		{
			ctx.isSceneDirty = true;
			ctx.worldRenderContext->shapesNeedUpdate = true;
		}
	}

	void MaterialEditorView::setClusterAllMaterials(ClusterInstance& cluster, uint16_t targetMat, EditorContext& ctx)
	{
		bool changed = false;
		for (Entity e : cluster.entities)
		{
			if (ctx.registry->hasComponent<Shape>(e))
			{
				auto& s = ctx.registry->getComponent<Shape>(e);
				if (s.material != targetMat)
				{
					s.material = targetMat;
					ctx.registry->setComponentDirty(s);
					changed = true;
				}
			}
			if (ctx.registry->hasComponent<Dot>(e))
			{
				auto& dot = ctx.registry->getComponent<Dot>(e);
				if (dot.materialId != targetMat)
				{
					dot.materialId = static_cast<unsigned int>(targetMat);
					ctx.registry->setComponentDirty(dot);
					changed = true;
				}
			}
			if (ctx.registry->hasComponent<TextRenderer>(e))
			{
				auto& tr = ctx.registry->getComponent<TextRenderer>(e);
				if (tr.material != targetMat)
				{
					tr.material = targetMat;
					ctx.registry->setComponentDirty(tr);
					changed = true;
				}
			}
		}
		if (changed)
		{
			ctx.isSceneDirty = true;
			ctx.worldRenderContext->shapesNeedUpdate = true;
		}
	}

	void MaterialEditorView::renderMaterialsTab(EditorContext& ctx)
	{
		ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Scene Materials (16 Slots)");
		ImGui::TextDisabled("Customize 2D materials used in the scene.");
		ImGui::Separator();

		// Swatches grid (2 rows of 8)
		const float swatchSize = 28.0f;
		const float spacing = 4.0f;
		for (int i = 0; i < 16; ++i)
		{
			if (i > 0 && i % 8 != 0)
			{
				ImGui::SameLine(0.0f, spacing);
			}

			ImGui::PushID(i);
			const bool selected = (ctx.selectedMaterialSlot == i);
			const auto& mat = ctx.services->materials2D().get(static_cast<uint16_t>(i));

			if (selected)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			}

			if (ImGui::ColorButton("##mat_palette_swatch", ImVec4(mat.color.r, mat.color.g, mat.color.b, mat.color.a),
								   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
								   ImVec2(swatchSize, swatchSize)))
			{
				ctx.selectedMaterialSlot = i;
			}

			if (selected)
			{
				ImGui::PopStyleColor();
				ImGui::PopStyleVar();
			}

			if (ImGui::IsItemHovered())
			{
				if (!mat.name.empty())
				{
					ImGui::SetTooltip("[%d] %s%s", i, mat.name.c_str(), selected ? " (selected)" : "");
				}
				else
				{
					ImGui::SetTooltip("Material %d%s", i, selected ? " (selected)" : "");
				}
			}

			ImGui::PopID();
		}

		ImGui::Spacing();
		ImGui::Separator();

		// Selected Material Properties
		if (ctx.selectedMaterialSlot < 0 || ctx.selectedMaterialSlot >= 16)
		{
			ctx.selectedMaterialSlot = 0;
		}

		auto& mat = ctx.services->materials2D().get(static_cast<uint16_t>(ctx.selectedMaterialSlot));

		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Slot [%d] Properties", ctx.selectedMaterialSlot);

		// Name
		char nameBuf[64];
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", mat.name.c_str());
		if (ImGui::InputText("Name##SlotName", nameBuf, sizeof(nameBuf)))
		{
			mat.name = nameBuf;
			ctx.isSceneDirty = true;
		}

		// Color
		if (ImGui::ColorEdit4("Color##SlotColor", &mat.color[0], ImGuiColorEditFlags_AlphaBar))
		{
			ctx.isSceneDirty = true;
			ctx.worldRenderContext->shapesNeedUpdate = true;
		}

		// Copy / Paste color
		if (ImGui::SmallButton("Copy Color"))
		{
			ctx.copiedMaterialColor = mat.color;
			ctx.hasCopiedMaterialColor = true;
		}
		if (ctx.hasCopiedMaterialColor)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Paste Color"))
			{
				mat.color = ctx.copiedMaterialColor;
				ctx.isSceneDirty = true;
				ctx.worldRenderContext->shapesNeedUpdate = true;
			}
		}

		// Pattern
		const char* patternNames[] = {"None", "Checkers", "Perlin Noise", "Waves", "Gradient"};
		int currentPattern = static_cast<int>(mat.pattern);
		if (currentPattern < 0 || currentPattern > 4)
		{
			currentPattern = 0;
		}

		if (ImGui::Combo("Pattern##SlotPattern", &currentPattern, patternNames, 5))
		{
			mat.pattern = static_cast<Pattern2D>(currentPattern);
			ctx.isSceneDirty = true;
			ctx.worldRenderContext->shapesNeedUpdate = true;
		}

		if (mat.pattern != Pattern2D::None)
		{
			if (ImGui::ColorEdit4("Secondary Color##SlotSecColor", &mat.secondaryColor[0],
								  ImGuiColorEditFlags_AlphaBar))
			{
				ctx.isSceneDirty = true;
				ctx.worldRenderContext->shapesNeedUpdate = true;
			}
			if (ImGui::DragFloat("Pattern Scale##SlotScale", &mat.patternScale, 0.05f, 0.01f, 100.0f))
			{
				ctx.isSceneDirty = true;
				ctx.worldRenderContext->shapesNeedUpdate = true;
			}
		}

		ImGui::Spacing();
		if (ImGui::Button("Reset Slot to Default", ImVec2(-1.0f, 22.0f)))
		{
			if (static_cast<size_t>(ctx.selectedMaterialSlot) < ColorPalette::Default.size())
			{
				mat.color = vec4(ColorPalette::Default[ctx.selectedMaterialSlot], 1.0f);
			}
			else
			{
				mat.color = vec4(1.0f);
			}
			mat.secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
			mat.pattern = Pattern2D::None;
			mat.patternScale = 1.0f;
			ctx.isSceneDirty = true;
			ctx.worldRenderContext->shapesNeedUpdate = true;
		}

		ImGui::Spacing();
		ImGui::Separator();

		// All slots summary list
		ImGui::Text("All Slots:");
		ImGui::BeginChild("##MaterialSlotsList", ImVec2(0.0f, 0.0f), true);
		for (int i = 0; i < 16; ++i)
		{
			ImGui::PushID(i);
			const auto& slotMat = ctx.services->materials2D().get(static_cast<uint16_t>(i));
			bool isSelected = (ctx.selectedMaterialSlot == i);

			ImGui::ColorButton("##row_swatch",
							   ImVec4(slotMat.color.r, slotMat.color.g, slotMat.color.b, slotMat.color.a),
							   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16.0f, 16.0f));
			ImGui::SameLine();

			std::string rowLabel = "[" + std::to_string(i) + "] ";
			if (!slotMat.name.empty())
			{
				rowLabel += slotMat.name;
			}
			else
			{
				rowLabel += "(unnamed)";
			}
			if (slotMat.pattern != Pattern2D::None)
			{
				rowLabel += " [Pattern]";
			}

			if (ImGui::Selectable(rowLabel.c_str(), isSelected))
			{
				ctx.selectedMaterialSlot = i;
			}

			ImGui::PopID();
		}
		ImGui::EndChild();
	}
} // namespace WeirdEngine::Editor
