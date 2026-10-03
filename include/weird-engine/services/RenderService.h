#pragma once

#include <vector>

#include "weird-engine/Background.h"
#include "weird-engine/ecs/Registry.h"
#include "weird-engine/systems/SDFRenderSystem.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/scene/Light.h"

namespace WeirdEngine
{
	enum class RenderMode
	{
		RayMarching3D,
		RayMarching2D,
		RayMarchingBoth
	};

	class RenderService
	{
	public:
		RenderService(Registry& registry, Entity& cameraEntity, SDFRenderSystemContext& context2D,
					  SDFRenderSystemContext& context3D, SDFRenderSystemContext& contextUI,
					  std::vector<WeirdRenderer::Light2D>& lights2D, std::vector<WeirdRenderer::Light3D>& lights3D,
					  BackgroundParams& background, RenderMode& renderMode)
			: m_registry(registry)
			, m_cameraEntity(cameraEntity)
			, m_context2D(context2D)
			, m_context3D(context3D)
			, m_contextUI(contextUI)
			, m_lights2D(lights2D)
			, m_lights3D(lights3D)
			, m_background(background)
			, m_renderMode(renderMode)
		{
		}

		WeirdRenderer::Camera& camera()
		{
			return m_registry.getComponent<ECS::Camera>(m_cameraEntity).camera;
		}

		Entity getCameraEntity() const
		{
			return m_cameraEntity;
		}

		std::vector<WeirdRenderer::Light2D>& getLights2D()
		{
			return m_lights2D;
		}

		std::vector<WeirdRenderer::Light3D>& getLights3D()
		{
			return m_lights3D;
		}

		BackgroundParams& getBackground()
		{
			return m_background;
		}

		const BackgroundParams& getBackground() const
		{
			return m_background;
		}

		RenderMode getRenderMode() const
		{
			return m_renderMode;
		}

		SDFRenderSystemContext& getContext2D()
		{
			return m_context2D;
		}

		SDFRenderSystemContext& getContext3D()
		{
			return m_context3D;
		}

		SDFRenderSystemContext& getContextUI()
		{
			return m_contextUI;
		}

		void forceShaderRefresh2D()
		{
			m_context2D.shapesNeedUpdate = true;
		}

		void forceShaderRefresh3D()
		{
			m_context3D.shapesNeedUpdate = true;
		}

		void forceShaderRefreshUI()
		{
			m_contextUI.shapesNeedUpdate = true;
		}

		void forceShaderRefresh()
		{
			forceShaderRefresh2D();
			forceShaderRefresh3D();
			forceShaderRefreshUI();
		}

	private:
		Registry& m_registry;
		Entity& m_cameraEntity;
		SDFRenderSystemContext& m_context2D;
		SDFRenderSystemContext& m_context3D;
		SDFRenderSystemContext& m_contextUI;
		std::vector<WeirdRenderer::Light2D>& m_lights2D;
		std::vector<WeirdRenderer::Light3D>& m_lights3D;
		BackgroundParams& m_background;
		RenderMode& m_renderMode;
	};
} // namespace WeirdEngine
