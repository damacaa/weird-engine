#version 300 es
precision highp float;
precision highp int;

#include "../common/utils.glsl"

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
uniform vec3 u_camPositionChange;

struct Material2D
{
	vec4 color;
	vec4 secondaryColor;
	int pattern;
	float patternScale;
	float emission;
	float edgeThickness;
	vec4 edgeColor;
	float refraction;
};

uniform Material2D u_materials[16];

vec4 evaluatePattern(Material2D mat, vec2 worldPos, float dist)
{
	vec4 baseColor = mat.color;

	if (mat.pattern == 1) // Checkers
	{
		vec2 p = floor(worldPos * mat.patternScale);
		float check = mod(p.x + p.y, 2.0);
		baseColor = mix(mat.color, mat.secondaryColor, check);
	}
	else if (mat.pattern == 2) // Perlin / procedural noise
	{
		vec2 p = worldPos * mat.patternScale * 0.2;
		vec2 i = floor(p);
		vec2 f = fract(p);
		f = f * f * (3.0 - 2.0 * f);
		float n00 = hash(i);
		float n10 = hash(i + vec2(1.0, 0.0));
		float n01 = hash(i + vec2(0.0, 1.0));
		float n11 = hash(i + vec2(1.0, 1.0));
		float n = mix(mix(n00, n10, f.x), mix(n01, n11, f.x), f.y);
		baseColor = mix(mat.color, mat.secondaryColor, n);
	}
	else if (mat.pattern == 3) // Waves
	{
		float w = sin(worldPos.y * mat.patternScale + sin(worldPos.x * mat.patternScale * 0.5) + u_time * 2.0) * 0.5 + 0.5;
		baseColor = mix(mat.color, mat.secondaryColor, w);
	}
	else if (mat.pattern == 4) // Gradient
	{
		float g = clamp(0.5 + (worldPos.y * mat.patternScale * 0.1), 0.0, 1.0);
		baseColor = mix(mat.secondaryColor, mat.color, g);
	}

	if (mat.edgeThickness > 0.0 && dist <= 0.0)
	{
		float edgeFactor = 1.0 - smoothstep(0.0, mat.edgeThickness, -dist);
		baseColor = mix(baseColor, mat.edgeColor, edgeFactor);
	}

	return baseColor;
}

void main()
{
	vec2 screenUV = v_texCoord;
	vec4 color = texture(t_materialDataTexture, screenUV);
	float distance = color.x;
	int materialId = int(color.y);
	float mask = color.z;

	float aspectRatio = u_resolution.x / u_resolution.y;
	float zoom = -u_camMatrix[3].z;
	vec2 uv = (2.0 * screenUV) - 1.0;
	uv.x *= aspectRatio;
	vec2 worldPos = (zoom * uv) - u_camMatrix[3].xy;

	// New material color
	vec4 c = vec4(1.0, 1.0, 1.0, 0.0);
	if (materialId < 16)
	{
		c = evaluatePattern(u_materials[materialId], worldPos, distance);
	}
	c = vec4(toLinear(c.rgb), c.a);

#ifdef MATERIAL_BLENDING
	// Get current material color
	float oldZoom = -u_oldCamMatrix[3].z;
	float zoomRatio = zoom / oldZoom;
	vec2 prevTexCoord;
	prevTexCoord.x = zoomRatio * (screenUV.x - 0.5) + 0.5 + 0.5 * u_camPositionChange.x / (oldZoom * aspectRatio);
	prevTexCoord.y = zoomRatio * (screenUV.y - 0.5) + 0.5 + 0.5 * u_camPositionChange.y / oldZoom;

	vec4 currentColor = texture(t_currentColorTexture, prevTexCoord);
	currentColor = clamp(currentColor, 0.0, 1.0);

	// Blend new color with current color
	float zoomFactor = (zoom - 10.0) * 0.02;
	zoomFactor = smoothstep(0.0, 1.0, zoomFactor);

	vec4 diff = clamp((c - currentColor), -1.0, 1.0);
	vec4 blendedColor = currentColor + (min(u_deltaTime * u_materialBlendSpeed, 1.0) * diff);

	// Use the mask to force the centers of the dots to get the instantaneous color every frame
	c = mix(blendedColor, c, clamp(mask, 0.0, 1.0));
#endif

	FragColor = c;
}
