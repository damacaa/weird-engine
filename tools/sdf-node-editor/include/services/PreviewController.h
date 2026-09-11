#pragma once

#include <array>
#include <vector>

#include <weird-engine.h>

namespace WeirdEngine::Editor
{
	enum class PreviewMode
	{
		WorldShape,
		UIShape
	};

	class PreviewController
	{
	public:
		static constexpr float kPreviewSelectorWidth = 118.0f;
		static constexpr float kPreviewSelectorRightMargin = 8.0f;
		static constexpr float kPreviewSelectorTopMargin = 5.0f;

		PreviewController() = default;

		void initMaterials(ServiceProvider& services);
		void initCamera(Registry& registry, ServiceProvider& services);

		void syncPreviewShape(Registry& registry, ServiceProvider& services, const Expr& expr,
							  const std::array<float, 8>& params);
		void syncParameters(ServiceProvider& services, const std::array<float, 8>& params);

		void update(Registry& registry, ServiceProvider& services);
		void clearSpawnedDots(Registry& registry);
		void destroyEntities(Registry& registry);

		PreviewMode getPreviewMode() const;
		void setPreviewMode(Registry& registry, ServiceProvider& services, PreviewMode mode, const Expr& expr,
							const std::array<float, 8>& params);

		float getCameraZoom() const;
		void setCameraZoom(float zoom);

		void updateCameraPosition(ServiceProvider& services, int winW, int winH, glm::vec2 targetCenter);

	private:
		PreviewMode m_previewMode = PreviewMode::WorldShape;
		Entity m_previewEntity = INVALID_ENTITY;
		Entity m_worldEntity = INVALID_ENTITY;

		float m_cameraZoom = 35.0f;
		glm::vec2 m_lastPreviewTargetCenter = {0.0f, 0.0f};

		unsigned int m_dotMaterialId = 0;
		std::vector<Entity> m_spawnedDots;
	};
} // namespace WeirdEngine::Editor
