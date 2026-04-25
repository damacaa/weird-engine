#version 300 es
precision highp float;
precision highp int;

#include "../common/utils.glsl"

// #define SHADOWS_ENABLED
// #define LONG_SHADOWS  // Uses FAR as max distance with no fade; comment out for zoom-aware short shadows
// #define ANTIALIASING

// #define DEBUG_SHOW_DISTANCE
// #define DEBUG_SHOW_COLORS

// Constants
const int MAX_STEPS = 128;
const float EPSILON = 0.05;
const float NEAR = 0.1;
const float FAR = 1.4;
const float NORMAL_EPSILON = 0.001;

const float SHADOW_VALUE = 0.85;
const float SHADOW_WORLD_DISTANCE = 0.5; // Max shadow cast distance in world-space units

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

uniform sampler2D t_colorTexture;
uniform sampler2D t_distanceSampledTexture; // Used for ineer lighting
uniform sampler2D t_backgroundTexture;
uniform sampler2D t_distanceCorrectedTexture; // Used for shadows and AO

const vec2 u_directionalLightDirection = vec2(0.7071, 0.7071);
uniform float u_ambienOcclusionRadius;
uniform float u_ambienOcclusionStrength;
uniform float u_overscan;
uniform vec3 u_shadowTint;

// For cast shadows and ambient occlusion, we need a distance function that has been corrected to fix smooth union artifacts
// Real distance in screen UV space
float mapOutside(vec2 p)
{
	// Remap screen UV to overscan texture UV
	vec2 overscanUV = 0.5 + (p - 0.5) / (1.0 + u_overscan);
	return texture(t_distanceCorrectedTexture, overscanUV).x;
}

vec2 softShadow(vec2 ro, vec2 rd, float initialDistance, float far, float k)
{
	if (initialDistance >= far)
		return vec2(far, 1.0); // Already beyond max distance, fully lit

	float res = 1.0;
	float t = initialDistance;
	float closestT = far; // t at the point of closest approach to any occluder

	for (int i = 0; i < MAX_STEPS; i++)
	{
		vec2 p = ro + rd * t;

		// Check if ray has left the overscan texture bounds
		float overscanMargin = 0.5 * u_overscan;
		vec2 dist = abs(p - 0.5);
		if (max(dist.x, dist.y) > 0.5 + overscanMargin)
		{
			break;
		}

		// Sample distance in screen UV space, which is the same space we're raymarching through, so no correction needed
		float h = mapOutside(ro + rd * t);

		// Track where the shadow is strongest (closest approach to an occluder)
		float newRes = k * h / t;
		if (newRes < res)
		{
			res = newRes;
			closestT = t;
		}

		if (h <= 0.0 || res < EPSILON)
			return vec2(t, 0.0); // Fully in shadow – keep t so fade works
		if (t > far)
			break; // Missed everything

		t += h;
	}

	// Return closest-approach distance so callers can fade by shadow-caster distance
	return vec2(closestT, clamp(res, 0.0, 1.0));
}

float renderShadows(vec2 uv, vec2 rd)
{
	float mapDistance = mapOutside(uv);

#ifdef LONG_SHADOWS

	float softShadowK = 16.0;
	float shadowFar = FAR;

#else

	float softShadowK = 2.0;
	float zoom = -u_camMatrix[3].z;
	// Scale max shadow distance by zoom so shadows have a consistent world-space extent
	float shadowFar = min(FAR, SHADOW_WORLD_DISTANCE / zoom);

#endif

	vec2 raymarchInfo = softShadow(uv, rd, mapDistance, shadowFar, softShadowK);
	float d = raymarchInfo.x;
	float shadowFactor = raymarchInfo.y;

#ifndef LONG_SHADOWS
	// Smooth fade-out as the shadow approaches the max distance
	float fadeFactor = 1.0 - smoothstep(shadowFar * 0.2, shadowFar, d);
	shadowFactor = mix(1.0, shadowFactor, fadeFactor);
#endif

	float shadowValue = mix(SHADOW_VALUE, 1.0, shadowFactor);

	return shadowValue;
}


// Ligthing inside shapes uses the original distance field, without correction (world coordinates)
// Only really used for normals, but it also gives a more consistent light falloff near edges that isn't affected by zoom level
float mapInside(vec2 p)
{
	return texture(t_distanceSampledTexture, p).x;
}

float calculateLight(vec2 uv, vec2 rd, vec2 normal, float shadows, float innerDistance, float edgeThickness)
{
	float lightNormalDot = -(dot(-rd, normal));

	// Fade shadow value for interior pixels from SHADOW_VALUE at depth to the computed custom shadow
	float innerShapeFade = clamp(innerDistance / edgeThickness, 0.0, 1.0);
	float innerShadowValue = mix(shadows, SHADOW_VALUE, innerShapeFade);

	// Extra light on the lit side, scaled by the angle to the light and shadow visibility
	float extraLight = max(0.0, 0.5 * lightNormalDot);
	float lightVisibility = smoothstep(SHADOW_VALUE, 1.0, innerShadowValue);
	extraLight *= lightVisibility;

	// Define the edge glow/falloff range in the distance field coordinate system.
	float borderMask = 1.0 - smoothstep(0.0, edgeThickness, innerDistance);

	// Apply border mask
	float lightOnBorderOnly = extraLight * borderMask;
	float light = 1.0 + lightOnBorderOnly;
	
	return clamp(light, 0.0, 10.0);
}


void main()
{
	vec2 screenUV = v_texCoord;
	vec4 colorSample = texture(t_colorTexture, screenUV);
	vec3 color = toSRGB(colorSample.rgb); // + (0.25 * floor(colorSample.a));
	float alpha = colorSample.a;		  // + (0.25 * floor(colorSample.a));
	vec4 data = texture(t_distanceSampledTexture, screenUV);
	float distance = data.x;

	float zoom = -u_camMatrix[3].z;
	float aspectRatio = u_resolution.x / u_resolution.y;
	float overscanScale = 1.0 + u_overscan;

	// Corrected distance, used for out shadows and AO
	float correctedDistance = mapOutside(screenUV);

	// Calculate normal
	vec2 p = screenUV;
	float d1 = mapInside(p + vec2(NORMAL_EPSILON, 0.0)) - mapInside(p - vec2(NORMAL_EPSILON, 0.0));
	float d2 = mapInside(p + vec2(0.0, NORMAL_EPSILON)) - mapInside(p - vec2(0.0, NORMAL_EPSILON));
	vec2 g = vec2(d1, d2);
	float len2 = dot(g, g);
	// Prevent NaN vector if length == 0
	vec2 normal = (len2 > 1e-8) ? g * inversesqrt(len2) : vec2(0.0);

#ifdef ANTIALIASING

	// Distance change over one pixel
	float smoothing = 1.0 * fwidth(distance);

	// Convert the distance to a factor between 0.0 and 1.0
	float shapeFactor = 1.0 - smoothstep(-smoothing, smoothing, distance);

#else

	float shapeFactor = distance <= 0.0 ? 1.0 : 0.0;

#endif

#ifdef DEBUG_SHOW_COLORS
	shapeFactor = 1.0;
#endif



	// Point light
	// vec2 rd = normalize(vec2(1.0) - screenUV);

	// Directional light
	vec2 rd = u_directionalLightDirection.xy;

	// World-space base offset used for the shadow edge
	// We raymarch from a slightly shifted pixel in the opposite light direction to expose bright edge
	float baseLightShadowOffset = min(0.005 * zoom, 0.3);
	float lightShadowOffset = baseLightShadowOffset / (zoom * overscanScale);

#ifdef SHADOWS_ENABLED

	float shadows = renderShadows(screenUV + (lightShadowOffset * rd), rd);
	float t = shadows;

#else

	float shadows = 1.0;
	float t = SHADOW_VALUE; // Force ambient occlusion to be fully applied when shadows are disabled, so we can still get darkening without directional light

#endif

	// Define the distance over which the ambient occlusion fades out
	float aoBlendFactor = smoothstep(0.0, u_ambienOcclusionRadius / u_resolution.y, correctedDistance);
	float fadeToFull = smoothstep(u_ambienOcclusionStrength, 1.0, t);
	float ao = mix(aoBlendFactor, 1.0, fadeToFull);
	// Apply ao
	shadows *= ao;

	// Edge thickness should use the distance field normalization used by mapInside / mapOutside.
	// in this shader, t_distanceSampledTexture is in world-distance units, so we keep world-space edge thickness
	// with a small adjustment for aspect and overscan so it stays resolution-independent.
	float lightEdgeThickness = (baseLightShadowOffset / zoom) * (0.5 / aspectRatio) * overscanScale;

	float light = calculateLight(screenUV, rd, normal, shadows, -distance, lightEdgeThickness * 0.5);

	// Remove shadows for interior pixels, but keep the shape factor for fading out the edge
	shadows = mix(shadows, 1.0, shapeFactor);

	// Refraction
#ifdef REFRACTION

	// Sample background with refraction
	float refractionDistance = -1.0 / (1.0 - clamp(((-distance * 100.0) + 1.0), 0.0, 10.0));
	refractionDistance = max(0.0, refractionDistance - 0.1);
	vec2 backgroundOffset = 0.01 * shapeFactor * refractionDistance * normal;
	backgroundOffset.x *= u_resolution.y / u_resolution.x;

	// Calculate the base UV
	vec2 finalUV = screenUV + backgroundOffset;

	// Get the size of one texel (pixel) in UV space
	ivec2 texSize = textureSize(t_backgroundTexture, 0);
	vec2 texelSize = 1.0 / vec2(texSize);

	// Define a Rotated Grid pattern (approx 0.5 pixel radius)
	// This pattern breaks grid alignment artifacts better than a simple + shape
	vec2 uv0 = finalUV + vec2(-0.125, -0.375) * texelSize; // Top-Left
	vec2 uv1 = finalUV + vec2(0.375, -0.125) * texelSize;  // Top-Right
	vec2 uv2 = finalUV + vec2(0.125, 0.375) * texelSize;   // Bottom-Right
	vec2 uv3 = finalUV + vec2(-0.375, 0.125) * texelSize;  // Bottom-Left

	// Sample and Average
	vec3 col0 = texture(t_backgroundTexture, uv0).rgb;
	vec3 col1 = texture(t_backgroundTexture, uv1).rgb;
	vec3 col2 = texture(t_backgroundTexture, uv2).rgb;
	vec3 col3 = texture(t_backgroundTexture, uv3).rgb;

	vec3 backgroundColor = (col0 + col1 + col2 + col3) * 0.25;

	// Blend with the non-refraction-sampled background color based on shape factor to show refraction only inside shapes
	backgroundColor = mix(texture(t_backgroundTexture, screenUV).rgb, backgroundColor, shapeFactor);

#else

	vec3 backgroundColor = texture(t_backgroundTexture, screenUV).rgb;

#endif

	// Combine the material's alpha with shape factor
	float finalAlpha = clamp(alpha + 0.1, 0.0, 1.0) * shapeFactor;

#ifdef DEBUG_SHOW_NORMALS
	color = vec3(normal, 0.0);
#endif

	vec3 shadowTransmittance = mix(u_shadowTint, vec3(1.0), shadows);
	vec3 shadedBackground = backgroundColor * shadowTransmittance;

	// Apply lighting to the shapes, cast shadows on the background, and mix both based on shape factor
	color = mix(color * light, shadedBackground, 1.0 - finalAlpha);

	FragColor = vec4(color, 1.0);

// Distance debug
#ifdef DEBUG_SHOW_DISTANCE

#ifdef REFRACTION
	FragColor = vec4(vec3(length(refractionDistance * 0.1)), 1.0);
// return;
#endif

	// float debugDistance = 10.0 * distance;
	float debugDistance = 0.5 * texture(t_distanceCorrectedTexture, screenUV).x;

	float value = 0.5 * (cos(500.0 * debugDistance) + 1.0);
	// value = debugDistance * 10.;
	// value = value * value * value;
	vec3 debugColor = debugDistance > 0.0 ? mix(vec3(1), vec3(0.2), value) :				  // outside
						  (debugDistance + 1.0) * mix(vec3(1.0, 0.2, 0.2), vec3(0.1), value); // inside

	FragColor = vec4(debugColor, 1.0);

#endif
}
