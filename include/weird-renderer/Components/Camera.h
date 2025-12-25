#pragma once
#include "weird-engine/vec.h"
#include "weird-engine/ecs/Component.h"
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
				vec2 resolution(WeirdRenderer::Screen::width, WeirdRenderer::Screen::height);
				vec2 halfResolution(0.5f * resolution);

				vec2 screenPositionCorrected(screenPosition.x, screenPosition.y);
				vec2 position = (vec2)cameraTransform.position + ((screenPositionCorrected - halfResolution) * (cameraTransform.position.z / halfResolution.y));

				return position;
			}

			static glm::vec2 worldPosition2DToScreenPosition(Transform cameraTransform, vec2 worldPosition)
			{
				vec2 resolution(WeirdRenderer::Screen::width, WeirdRenderer::Screen::height);
				vec2 halfResolution(0.5f * resolution);

				// 1. Get the position relative to the camera center
				vec2 relativeWorldPosition = worldPosition - (vec2)cameraTransform.position;

				// 2. Calculate the inverse projection factor
				// In the first function, we multiplied by (Z / halfRes.y).
				// To reverse it, we multiply by (halfRes.y / Z).
				float inverseScale = halfResolution.y / cameraTransform.position.z;

				// 3. Apply scale and add the screen center offset
				vec2 screenPosition = (relativeWorldPosition * inverseScale) + halfResolution;

				return screenPosition;
			}

		};
	}
}
