#include "services/PreviewController.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "weird-renderer/core/Display.h"
#include <imgui.h>

namespace WeirdEngine::Editor
{
	void PreviewController::initMaterials(ServiceProvider& services)
	{
		// Set neutral dark gray background for the 2D scene
		auto& bg = services.render().getBackground();
		bg.type = BackgroundType::Solid;
		bg.primaryColor = glm::vec4(0.08f, 0.08f, 0.08f, 1.0f);

		// Define material 0 with default light blue accent color (UI Shape / Audio preview)
		auto& mat0 = services.materials2D().get(0);
		mat0.name = "sdf_preview";
		mat0.color = glm::vec4(0.15f, 0.85f, 0.95f, 1.0f);

		// Define material 1 with distinct warm amber accent color (World CustomShape preview)
		auto& mat1 = services.materials2D().getOrCreate("world_preview");
		mat1.color = glm::vec4(1.0f, 0.58f, 0.16f, 1.0f);

		// Define material 2 for spawned physics dots (bright warm yellow/white)
		auto& matDot = services.materials2D().getOrCreate("preview_dot");
		matDot.color = glm::vec4(1.0f, 0.92f, 0.35f, 1.0f);
		m_dotMaterialId = matDot.id;
	}

	void PreviewController::initCamera(Registry& registry, ServiceProvider& services)
	{
		Entity mainCam = services.tags().getEntityByTag("mainCamera");
		if (mainCam < MAX_ENTITIES && registry.hasComponent<Transform>(mainCam))
		{
			auto& t = registry.getComponent<Transform>(mainCam);
			t.position.z = m_cameraZoom;
			if (registry.hasComponent<FlyMovement2D>(mainCam))
			{
				auto& fly = registry.getComponent<FlyMovement2D>(mainCam);
				fly.targetPosition = t.position;
			}
		}
	}

	void PreviewController::updateCameraPosition(ServiceProvider& services, int winW, int winH, glm::vec2 targetCenter)
	{
		float uvCenterX = ((2.0f * targetCenter.x / static_cast<float>(winW)) - 1.0f) *
						  (static_cast<float>(winW) / static_cast<float>(winH));
		float uvCenterY = (2.0f * targetCenter.y / static_cast<float>(winH)) - 1.0f;

		Entity mainCam = services.tags().getEntityByTag("mainCamera");
		if (mainCam < MAX_ENTITIES && services.registry().hasComponent<Transform>(mainCam))
		{
			auto& t = services.registry().getComponent<Transform>(mainCam);
			float zoom = m_cameraZoom > 0.0f ? m_cameraZoom : 35.0f;
			glm::vec3 desiredPos(-zoom * uvCenterX, -zoom * uvCenterY, zoom);

			if (m_previewMode == PreviewMode::UIShape)
			{
				desiredPos.x += 100000.0f;
				desiredPos.y += 100000.0f;
			}

			if (glm::distance(t.position, desiredPos) > 0.001f)
			{
				t.position = desiredPos;
				services.registry().setComponentDirty(t);
			}
			if (services.registry().hasComponent<FlyMovement2D>(mainCam))
			{
				auto& fly = services.registry().getComponent<FlyMovement2D>(mainCam);
				fly.targetPosition = desiredPos;
			}
		}
	}

	void PreviewController::syncPreviewShape(Registry& registry, ServiceProvider& services, const Expr& expr,
											 const std::array<float, 8>& params)
	{
		int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
		int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;
		float menuH = 24.0f;
		float contentH = static_cast<float>(winH) - menuH;
		float inspectorWidth = 360.0f;
		float previewHeight = std::min(360.0f, std::max(240.0f, contentH * 0.40f));
		glm::vec2 targetCenter(static_cast<float>(winW) - inspectorWidth * 0.5f, previewHeight * 0.5f);
		m_lastPreviewTargetCenter = targetCenter;

		updateCameraPosition(services, winW, winH, targetCenter);

		// 1. World Shape (CustomShape component on separate entity with Material 1)
		std::shared_ptr<IMathExpression> worldExpr = expr.node;

		if (m_worldEntity != INVALID_ENTITY && registry.hasComponent<CustomShape>(m_worldEntity))
		{
			registry.destroyEntity(m_worldEntity);
			m_worldEntity = INVALID_ENTITY;
		}

		ShapeId worldShapeId = services.shapes().registerSDF(worldExpr ? worldExpr : Expr(0.0f).node);
		auto& mat1 = services.materials2D().getOrCreate("world_preview");
		ShapeConfig worldConfig;
		worldConfig.shapeId = worldShapeId;
		worldConfig.material = mat1;
		worldConfig.combination = CombinationType::Addition;
		worldConfig.hasCollision = true;
		worldConfig.group = 0;
		std::copy_n(params.data(), 8, worldConfig.variables.data);

		m_worldEntity = services.shapes().addShape(worldConfig);
		if (m_worldEntity != INVALID_ENTITY && registry.hasComponent<CustomShape>(m_worldEntity))
		{
			auto& cs = registry.getComponent<CustomShape>(m_worldEntity);
			cs.smoothFactor = 0.0f;
			cs.material = mat1.id;
			std::copy_n(params.data(), 8, cs.parameters);
			registry.setComponentDirty(cs);
		}

		services.render().forceShaderRefresh();
		services.audio().resampleShape();
	}

	void PreviewController::syncParameters(ServiceProvider& services, const std::array<float, 8>& params)
	{
		// 1. Update World CustomShape parameters
		if (m_worldEntity != INVALID_ENTITY && services.registry().hasComponent<CustomShape>(m_worldEntity))
		{
			auto& cs = services.registry().getComponent<CustomShape>(m_worldEntity);
			std::copy_n(params.data(), 8, cs.parameters);
			services.registry().setComponentDirty(cs);
		}

		// 2. Update UI Shape parameters
		if (m_previewEntity != INVALID_ENTITY && services.registry().hasComponent<UIShape>(m_previewEntity))
		{
			auto& uiShape = services.registry().getComponent<UIShape>(m_previewEntity);
			std::copy_n(params.data(), 8, uiShape.parameters);
			services.registry().setComponentDirty(uiShape);
		}

		services.audio().resampleShape();
	}

	void PreviewController::update(Registry& registry, ServiceProvider& services)
	{
		if (m_previewMode != PreviewMode::WorldShape)
			return;

		int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
		int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;
		float menuH = 24.0f;
		float contentH = static_cast<float>(winH) - menuH;
		float inspectorWidth = 360.0f;
		float canvasWidth = static_cast<float>(winW) - inspectorWidth;
		float previewHeight = std::min(360.0f, std::max(240.0f, contentH * 0.40f));
		float inspectorHeight = contentH - previewHeight;

		float gapMinX = canvasWidth;
		float gapMaxX = static_cast<float>(winW);
		float gapMinY = menuH + inspectorHeight;
		float gapMaxY = static_cast<float>(winH);
		float selectorMinX = gapMaxX - kPreviewSelectorWidth - kPreviewSelectorRightMargin;

		Entity mainCam = services.tags().getEntityByTag("mainCamera");
		if (mainCam == INVALID_ENTITY || !registry.hasComponent<Transform>(mainCam))
			return;

		auto& camTransform = registry.getComponent<Transform>(mainCam);

		// 1. Mouse wheel zoom
		float mouseScreenX = services.input().getMouseX();
		float mouseCameraY = services.input().getMouseY(); // Display::height - rawSDL_Y
		float mouseTopLeftY = static_cast<float>(winH) - mouseCameraY;

		bool isInPreviewGap = (mouseScreenX >= gapMinX && mouseScreenX <= gapMaxX && mouseTopLeftY >= gapMinY &&
							   mouseTopLeftY <= gapMaxY);
		bool isInPreviewSelector = (mouseScreenX >= selectorMinX && mouseScreenX <= gapMaxX &&
									mouseTopLeftY >= gapMinY && mouseTopLeftY <= gapMaxY);
		bool isHoveredInGap = isInPreviewGap && !isInPreviewSelector;

		if (isHoveredInGap)
		{
			float wheelDelta = 0.0f;
			if (services.input().getMouseButtonDown(Input::WheelUp))
				wheelDelta += 1.0f;
			if (services.input().getMouseButtonDown(Input::WheelDown))
				wheelDelta -= 1.0f;

			const ImGuiIO& io = ImGui::GetIO();
			if (wheelDelta == 0.0f && std::abs(io.MouseWheel) > 0.01f)
			{
				wheelDelta = io.MouseWheel;
			}

			if (wheelDelta != 0.0f)
			{
				float zoomFactor = (wheelDelta > 0.0f) ? 0.90f : 1.10f;
				m_cameraZoom = std::clamp(m_cameraZoom * zoomFactor, 5.0f, 200.0f);
				glm::vec2 targetCenter(static_cast<float>(winW) - inspectorWidth * 0.5f, previewHeight * 0.5f);
				updateCameraPosition(services, winW, winH, targetCenter);
			}
		}

		// 2. Mouse spawning: check left click
		bool isLeftDown =
			services.input().getMouseButton(Input::LeftClick) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

		if (isLeftDown && isHoveredInGap)
		{
			static float lastSpawnTime = 0.0f;
			if (services.time().time() - lastSpawnTime > 0.05f)
			{
				lastSpawnTime = services.time().time();

				glm::vec2 mousePosForCam(mouseScreenX, mouseCameraY);
				glm::vec2 worldPos = ECS::Camera::screenPositionToWorldPosition2D(camTransform, mousePosForCam);

				if (m_spawnedDots.size() < 3000)
				{
					Entity dotEntity = registry.createEntity();
					auto& t = registry.addComponent<Transform>(dotEntity);

					float offsetX = ((std::rand() % 200) - 100.0f) * 0.01f * 0.5f;
					float offsetY = ((std::rand() % 200) - 100.0f) * 0.01f * 0.5f;
					t.position = glm::vec3(worldPos.x + offsetX, worldPos.y + offsetY, 0.0f);

					auto& dot = registry.addComponent<Dot>(dotEntity);
					dot.materialId = m_dotMaterialId;

					registry.addComponent<RigidBody2D>(dotEntity);

					m_spawnedDots.push_back(dotEntity);
				}
			}
		}

		// 3. Destroy dots that leave the preview window
		float margin = 20.0f;
		for (size_t i = 0; i < m_spawnedDots.size();)
		{
			Entity e = m_spawnedDots[i];
			if (!registry.hasComponent<Transform>(e))
			{
				m_spawnedDots[i] = m_spawnedDots.back();
				m_spawnedDots.pop_back();
				continue;
			}

			auto& t = registry.getComponent<Transform>(e);
			glm::vec2 screenPos =
				ECS::Camera::worldPosition2DToScreenPosition(camTransform, glm::vec2(t.position.x, t.position.y));
			float screenTopLeftY = static_cast<float>(winH) - screenPos.y;

			bool outOfBounds = (screenPos.x < (gapMinX - margin)) || (screenPos.x > (gapMaxX + margin)) ||
							   (screenTopLeftY < (gapMinY - margin)) || (screenTopLeftY > (gapMaxY + margin));

			if (outOfBounds)
			{
				registry.destroyEntity(e);
				m_spawnedDots[i] = m_spawnedDots.back();
				m_spawnedDots.pop_back();
			}
			else
			{
				++i;
			}
		}
	}

	void PreviewController::clearSpawnedDots(Registry& registry)
	{
		for (Entity e : m_spawnedDots)
		{
			if (e != INVALID_ENTITY && registry.hasComponent<Transform>(e))
			{
				registry.destroyEntity(e);
			}
		}
		m_spawnedDots.clear();
	}

	void PreviewController::destroyEntities(Registry& registry)
	{
		clearSpawnedDots(registry);
		if (m_worldEntity != INVALID_ENTITY && registry.hasComponent<CustomShape>(m_worldEntity))
		{
			registry.destroyEntity(m_worldEntity);
			m_worldEntity = INVALID_ENTITY;
		}
	}

	PreviewMode PreviewController::getPreviewMode() const
	{
		return m_previewMode;
	}

	void PreviewController::setPreviewMode(Registry& registry, ServiceProvider& services, PreviewMode mode,
										   const Expr& expr, const std::array<float, 8>& params)
	{
		m_previewMode = mode;
		syncPreviewShape(registry, services, expr, params);
	}

	float PreviewController::getCameraZoom() const
	{
		return m_cameraZoom;
	}

	void PreviewController::setCameraZoom(float zoom)
	{
		m_cameraZoom = zoom;
	}
} // namespace WeirdEngine::Editor
