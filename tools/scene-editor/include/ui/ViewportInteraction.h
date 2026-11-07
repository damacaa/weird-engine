#pragma once

#include "core/EditorContext.h"

namespace WeirdEngine::Editor
{
	class SceneOperations;

	class ViewportInteraction
	{
	public:
		void handleHotkeys(EditorContext& ctx, SceneOperations& sceneOps);
		void handleCameraInput(EditorContext& ctx);
		void zoomCamera(EditorContext& ctx, float steps);
		void panCamera(EditorContext& ctx, vec2 screenDelta);
		void resetCamera(EditorContext& ctx);
		vec2 getMouseWorld(EditorContext& ctx);
		void handleViewportMouseInput(EditorContext& ctx, SceneOperations& sceneOps);

		void renderBottomToolbar(EditorContext& ctx, SceneOperations& sceneOps);
		void renderViewportContextMenu(EditorContext& ctx, SceneOperations& sceneOps);
		void renderProjectModal(EditorContext& ctx);
	};
} // namespace WeirdEngine::Editor
