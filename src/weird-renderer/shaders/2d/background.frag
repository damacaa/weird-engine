#version 300 es
precision highp float;
precision highp int;

// Constants

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform mat4 u_camMatrix;
uniform vec2 u_resolution;
uniform float u_time;

// Background Parameter Uniforms
uniform vec4 u_bgPrimaryColor;
uniform vec4 u_bgSecondaryColor;
uniform float u_bgScale;
uniform float u_bgIntensity;

uniform sampler2D t_prevBackground;
uniform bool u_enableBlend;

// INJECTION POINT
#include "background_injection"

#ifndef HAS_CUSTOM_BACKGROUND
vec3 getBackground(vec2 uv, vec2 worldPos)
{
	return u_bgPrimaryColor.rgb;
}
#endif

void main()
{
	vec2 uv = (2.0 * v_texCoord) - 1.0;
	uv.x *= (u_resolution.x / u_resolution.y);

	float zoom = -2.0 * u_camMatrix[3].z;
	vec2 pos = (zoom * uv) - u_camMatrix[3].xy;

	vec3 background = getBackground(uv, pos);

	if (u_enableBlend)
	{
		vec3 prevBackground = texture(t_prevBackground, v_texCoord).rgb;

		vec3 diff = background - prevBackground;
		vec3 blendStep = diff * 0.05;

		// Ensure we don't get stuck due to 8-bit color quantization
		blendStep += sign(diff) * (1.5 / 255.0);
		blendStep = clamp(blendStep, -abs(diff), abs(diff));

		background = prevBackground + blendStep;
	}

	FragColor = vec4(background, 1.0);
}