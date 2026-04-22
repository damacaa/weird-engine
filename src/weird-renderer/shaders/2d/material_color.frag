#version 300 es
precision highp float;
precision highp int;

#include "../common/utils.glsl"

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
uniform float u_deltaTime;
uniform float u_materialBlendSpeed;

uniform mat4 u_oldCamMatrix;
uniform sampler2D t_materialDataTexture;
uniform sampler2D t_currentColorTexture;
uniform vec4 u_staticColors[16];
uniform vec3 u_camPositionChange;

vec3 randomColor(int index)
{
	float seed = float(index) * 43758.5453;
	float r = fract(sin(seed) * 43758.5453);
	float g = fract(sin(seed + 1.0) * 43758.5453);
	float b = fract(sin(seed + 2.0) * 43758.5453);
	return vec3(r, g, b);
}

void main()
{
	vec2 screenUV = v_texCoord;
	vec4 color = texture(t_materialDataTexture, screenUV);
	float distance = color.x;
	int materialId = int(color.y);
	float mask = color.z;

	// New material color
	vec4 c = materialId < 16 ? u_staticColors[materialId] : vec4(1.0, 1.0, 1.0, 0.0);
	c = vec4(toLinear(c.rgb), c.a);

	// Get current material color
	// Compensate for both camera translation and zoom change to find the same world point in the previous frame.
	// prevTexCoord = (zoom/oldZoom) * (texCoord - 0.5) + 0.5 + 0.5 * camPositionChange / (oldZoom * [aspectRatio, 1])
	float aspectRatio = u_resolution.x / u_resolution.y;
	float zoom = -u_camMatrix[3].z;
	float oldZoom = -u_oldCamMatrix[3].z;
	float zoomRatio = zoom / oldZoom;
	vec2 prevTexCoord;
	prevTexCoord.x = zoomRatio * (screenUV.x - 0.5) + 0.5 + 0.5 * u_camPositionChange.x / (oldZoom * aspectRatio);
	prevTexCoord.y = zoomRatio * (screenUV.y - 0.5) + 0.5 + 0.5 * u_camPositionChange.y / oldZoom;

	vec4 currentColor = texture(t_currentColorTexture, prevTexCoord);
	currentColor = clamp(currentColor, 0.0, 1.0);

	// Blend new color with current color
	// Calculate zoom factor to reduce blending with distance

	float zoomFactor = (zoom - 10.0) * 0.02;
	zoomFactor = smoothstep(0.0, 1.0, zoomFactor);

	// TODO: uniform to control speed?
	// c = mix(c, currentColor, 0.999 * (1.0 - zoomFactor) * mask);
	// c = mix(c, currentColor, pow(0.99, 60.0 * u_deltaTime) * (1.0 - zoomFactor) * mask);
	vec4 diff = clamp((c - currentColor), -1.0, 1.0);
	c = currentColor + (min(u_deltaTime * u_materialBlendSpeed, 1.0) * diff);

	FragColor = c;
}
