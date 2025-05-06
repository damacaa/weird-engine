#pragma once
#include "../Component.h"
#include "weird-renderer/Screen.h"


namespace WeirdEngine 
{
	namespace ECS
	{
		struct Camera : public Component
		{
			WeirdRenderer::Camera camera;

			Camera() : camera(vec3(0.0f))
			{

			}

			Camera(vec3 v) : camera(v)
			{

			}

			static glm::vec2 screenPositionToWorldPosition2D(Transform cameraTransform, vec2 screenPosition)
			{
				// TODO: fix this
				vec2 resolution(WeirdRenderer::Screen::width, WeirdRenderer::Screen::height);
				vec2 halfResolution(0.5f * resolution);

				vec2 screenPositionCorrected(screenPosition.x, resolution.y - screenPosition.y);
				vec2 uv = (2.0f * screenPositionCorrected - resolution) / resolution.y;
				vec2 position = (vec2)cameraTransform.position + ((screenPositionCorrected - halfResolution) * (cameraTransform.position.z / halfResolution.y));

				return position;
			}

		};
	}
}
