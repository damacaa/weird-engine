#include "ui/EditorTheme.h"

#include "imnodes.h"

namespace WeirdEngine::Editor
{
	void EditorTheme::setupImNodesStyle()
	{
		ImNodesStyle& style = ImNodes::GetStyle();
		style.Flags |= ImNodesStyleFlags_GridLines | ImNodesStyleFlags_NodeOutline;
		style.NodeCornerRounding = 6.0f;
		style.NodeBorderThickness = 1.5f;
		style.Colors[ImNodesCol_GridBackground] = IM_COL32(20, 20, 20, 255);
		style.Colors[ImNodesCol_GridLine] = IM_COL32(36, 36, 36, 255);
		style.Colors[ImNodesCol_GridLinePrimary] = IM_COL32(50, 50, 50, 255);
		style.Colors[ImNodesCol_TitleBar] = IM_COL32(38, 38, 38, 255);
		style.Colors[ImNodesCol_TitleBarHovered] = IM_COL32(52, 52, 52, 255);
		style.Colors[ImNodesCol_TitleBarSelected] = IM_COL32(66, 66, 66, 255);
		style.Colors[ImNodesCol_NodeBackground] = IM_COL32(26, 26, 26, 255);
		style.Colors[ImNodesCol_NodeBackgroundHovered] = IM_COL32(34, 34, 34, 255);
		style.Colors[ImNodesCol_NodeBackgroundSelected] = IM_COL32(42, 42, 42, 255);
		style.Colors[ImNodesCol_Link] = IM_COL32(90, 170, 240, 210);
		style.Colors[ImNodesCol_LinkHovered] = IM_COL32(130, 205, 255, 255);
		style.Colors[ImNodesCol_LinkSelected] = IM_COL32(255, 195, 75, 255);
		style.Colors[ImNodesCol_Pin] = IM_COL32(70, 195, 215, 255);
		style.Colors[ImNodesCol_PinHovered] = IM_COL32(110, 235, 255, 255);
	}

	ImVec4 EditorTheme::getCategoryColor(NodeCategory cat)
	{
		switch (cat)
		{
			case NodeCategory::Input:
				return ImVec4(0.20f, 0.45f, 0.70f, 1.0f); // Slate Blue
			case NodeCategory::Vector:
				return ImVec4(0.18f, 0.55f, 0.55f, 1.0f); // Teal
			case NodeCategory::Transforms:
				return ImVec4(0.60f, 0.40f, 0.15f, 1.0f); // Ochre / Warm Amber
			case NodeCategory::Primitives2D:
				return ImVec4(0.25f, 0.55f, 0.25f, 1.0f); // Forest Green
			case NodeCategory::Primitives3D:
				return ImVec4(0.35f, 0.55f, 0.35f, 1.0f); // Sage Green
			case NodeCategory::CSG:
				return ImVec4(0.55f, 0.22f, 0.45f, 1.0f); // Plum
			case NodeCategory::MathUnary:
			case NodeCategory::MathBinary:
			case NodeCategory::MathTernary:
				return ImVec4(0.40f, 0.35f, 0.55f, 1.0f); // Muted Purple
			case NodeCategory::Output:
				return ImVec4(0.65f, 0.22f, 0.22f, 1.0f); // Rust Red
			default:
				return ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
		}
	}

	ImU32 EditorTheme::getCategoryColorU32(NodeCategory cat)
	{
		return ImGui::ColorConvertFloat4ToU32(getCategoryColor(cat));
	}
} // namespace WeirdEngine::Editor
