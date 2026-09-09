#version 300 es
precision highp float;
precision highp int;

#include "../common/utils.glsl"

// Constants
const int MAX_STEPS = 128;
const float EPSILON = 0.05;
const float NEAR = 0.1;
const float FAR = 1.4;
const float NORMAL_EPSILON = 0.001;

const float SHADOW_VALUE = 0.85;
const float SHADOW_WORLD_DISTANCE = 0.5; // Max shadow cast distance in world-space units

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
uniform sampler2D t_distanceSampledTexture; // Used for inner lighting
uniform sampler2D t_backgroundTexture;
uniform sampler2D t_distanceCorrectedTexture; // Used for shadows and AO

uniform float u_ambienOcclusionRadius;
uniform float u_ambienOcclusionStrength;
uniform float u_overscan;
uniform vec3 u_shadowTint;
uniform float u_refractionIntensity;

struct Light2D
{
	int type;           // 0=Directional, 1=Point, 2=Cone
	vec2 position;      // World position
	vec2 direction;     // Normalized direction vector (Directional & Cone)
	vec4 color;         // RGB = color, A = intensity
	float radius;       // Attenuation radius (Point & Cone)
	float coneAngle;    // Cone half-angle in radians
	float conePenumbra; // Cone penumbra angle in radians
	int castShadows;    // 1 = true, 0 = false
};

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

uniform int u_numLights;
uniform Light2D u_lights[8];
uniform Material2D u_materials[16];

// For cast shadows and ambient occlusion, we need a distance function that has been corrected to fix smooth union
// artifacts Real distance in screen UV space
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

		float h = mapOutside(ro + rd * t);

		float newRes = k * h / t;
		if (newRes < res)
		{
			res = newRes;
			closestT = t;
		}

		if (h <= 0.0 || res < EPSILON)
			return vec2(t, 0.0); // Fully in shadow
		if (t > far)
			break; // Missed everything

		t += h;
	}

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
	float shadowFar = min(FAR, SHADOW_WORLD_DISTANCE / zoom);
#endif

	vec2 raymarchInfo = softShadow(uv, rd, mapDistance, shadowFar, softShadowK);
	float d = raymarchInfo.x;
	float shadowFactor = raymarchInfo.y;

#ifndef LONG_SHADOWS
	float fadeFactor = 1.0 - smoothstep(shadowFar * 0.2, shadowFar, d);
	shadowFactor = mix(1.0, shadowFactor, fadeFactor);
#endif

	return mix(SHADOW_VALUE, 1.0, shadowFactor);
}

float mapInside(vec2 p)
{
	return texture(t_distanceSampledTexture, p).x;
}

float calculateLightForRay(vec2 uv, vec2 rd, vec2 normal, float shadows, float innerDistance, float edgeThickness)
{
	float lightNormalDot = -(dot(-rd, normal));

	float innerShapeFade = clamp(innerDistance / max(edgeThickness, 0.0001), 0.0, 1.0);
	float innerShadowValue = mix(shadows, SHADOW_VALUE, innerShapeFade);

	float extraLight = max(0.0, 0.5 * lightNormalDot);
	float lightVisibility = smoothstep(SHADOW_VALUE, 1.0, innerShadowValue);
	extraLight *= lightVisibility;

	float borderMask = 1.0 - smoothstep(0.0, max(edgeThickness, 0.0001), innerDistance);
	float lightOnBorderOnly = extraLight * borderMask;
	float light = 1.0 + lightOnBorderOnly;

	return clamp(light, 0.0, 10.0);
}

void main()
{
	vec2 screenUV = v_texCoord;
	vec4 colorSample = texture(t_colorTexture, screenUV);
	vec3 color = toSRGB(colorSample.rgb);
	float alpha = colorSample.a;
	vec4 data = texture(t_distanceSampledTexture, screenUV);
	float distance = data.x;
	int materialId = int(data.y);

	int matIdx = (materialId >= 0 && materialId < 16) ? materialId : 0;
	Material2D mat = u_materials[matIdx];

	float zoom = -u_camMatrix[3].z;
	float aspectRatio = u_resolution.x / u_resolution.y;
	float overscanScale = 1.0 + u_overscan;

	vec2 uv = (2.0 * screenUV) - 1.0;
	uv *= overscanScale;
	uv.x *= aspectRatio;
	vec2 worldPos = (zoom * uv) - u_camMatrix[3].xy;

#ifdef SHADOWS_ENABLED
	float correctedDistance = mapOutside(screenUV);
#endif

	// Calculate normal
	vec2 p = screenUV;
	float d1 = mapInside(p + vec2(NORMAL_EPSILON, 0.0)) - mapInside(p - vec2(NORMAL_EPSILON, 0.0));
	float d2 = mapInside(p + vec2(0.0, NORMAL_EPSILON)) - mapInside(p - vec2(0.0, NORMAL_EPSILON));
	vec2 g = vec2(d1, d2);
	float len2 = dot(g, g);
	vec2 normal = (len2 > 1e-8) ? g * inversesqrt(len2) : vec2(0.0);

#ifdef ANTIALIASING
	float maxSmoothing = 2.0 * overscanScale / u_resolution.y;
	float smoothing = min(1.0 * fwidth(distance), maxSmoothing);
	float shapeFactor = 1.0 - smoothstep(-smoothing, smoothing, distance);
#else
	float shapeFactor = distance <= 0.0 ? 1.0 : 0.0;
#endif

#ifdef DEBUG_SHOW_COLORS
	shapeFactor = 1.0;
#endif

	float baseLightShadowOffset = min(0.005 * zoom, 0.3);
	float lightShadowOffset = baseLightShadowOffset / (zoom * overscanScale);
	float lightEdgeThickness = (baseLightShadowOffset / zoom) * (0.5 / aspectRatio) * overscanScale;

	vec3 accumulatedShapeLight = vec3(0.0);
	float minShadowValue = 1.0;

	int activeLightCount = u_numLights;
	if (activeLightCount == 0)
	{
		// Default fallback directional light
		vec2 rd = vec2(0.7071, 0.7071);
#ifdef SHADOWS_ENABLED
		float shadows = renderShadows(screenUV + (lightShadowOffset * rd), rd);
#else
		float shadows = 1.0;
#endif
		float light = calculateLightForRay(screenUV, rd, normal, shadows, -distance, lightEdgeThickness * 0.5);
		accumulatedShapeLight += vec3(light);
		minShadowValue = min(minShadowValue, shadows);
	}
	else
	{
		for (int i = 0; i < activeLightCount && i < 8; ++i)
		{
			Light2D light = u_lights[i];
			vec3 lightColor = light.color.rgb * light.color.a;

			if (light.type == 0) // Directional
			{
				vec2 rd = normalize(light.direction);
				float shadows = 1.0;
#ifdef SHADOWS_ENABLED
				if (light.castShadows != 0)
				{
					shadows = renderShadows(screenUV + (lightShadowOffset * rd), rd);
				}
#endif
				float lightFactor = calculateLightForRay(screenUV, rd, normal, shadows, -distance, lightEdgeThickness * 0.5);
				accumulatedShapeLight += lightColor * lightFactor;
				minShadowValue = min(minShadowValue, shadows);
			}
			else if (light.type == 1) // Point
			{
				vec2 toFragWorld = worldPos - light.position;
				float distWorld = length(toFragWorld);
				if (distWorld < light.radius)
				{
					float att = clamp(1.0 - (distWorld / light.radius), 0.0, 1.0);
					att = att * att; // quadratic falloff

					vec2 rd = (distWorld > 1e-5) ? toFragWorld / distWorld : vec2(0.0, 1.0);
					float shadows = 1.0;
#ifdef SHADOWS_ENABLED
					if (light.castShadows != 0)
					{
						shadows = renderShadows(screenUV + (lightShadowOffset * rd), rd);
					}
#endif
					float lightFactor = calculateLightForRay(screenUV, rd, normal, shadows, -distance, lightEdgeThickness * 0.5);
					accumulatedShapeLight += lightColor * att * lightFactor;
					minShadowValue = min(minShadowValue, mix(1.0, shadows, att));
				}
			}
			else if (light.type == 2) // Cone / Spot
			{
				vec2 toFragWorld = worldPos - light.position;
				float distWorld = length(toFragWorld);
				if (distWorld < light.radius)
				{
					float att = clamp(1.0 - (distWorld / light.radius), 0.0, 1.0);
					att = att * att;

					vec2 toFragNorm = (distWorld > 1e-5) ? toFragWorld / distWorld : vec2(0.0, 1.0);
					vec2 coneDir = normalize(light.direction);
					float cosAngle = dot(toFragNorm, coneDir);
					float innerCos = cos(light.coneAngle - light.conePenumbra);
					float outerCos = cos(light.coneAngle);
					float coneFactor = clamp((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);
					coneFactor = smoothstep(0.0, 1.0, coneFactor);

					if (coneFactor > 0.0)
					{
						vec2 rd = toFragNorm;
						float shadows = 1.0;
#ifdef SHADOWS_ENABLED
						if (light.castShadows != 0)
						{
							shadows = renderShadows(screenUV + (lightShadowOffset * rd), rd);
						}
#endif
						float lightFactor = calculateLightForRay(screenUV, rd, normal, shadows, -distance, lightEdgeThickness * 0.5);
						accumulatedShapeLight += lightColor * att * coneFactor * lightFactor;
						minShadowValue = min(minShadowValue, mix(1.0, shadows, att * coneFactor));
					}
				}
			}
		}
	}

	float shadows = minShadowValue;
#ifndef SHADOWS_ENABLED
	float t = SHADOW_VALUE;
#else
	float t = shadows;
#endif

#ifdef SHADOWS_ENABLED
	float screenDistance = correctedDistance * overscanScale * aspectRatio;
#else
	float screenDistance = max(0.0, distance) * 2.0 * aspectRatio;
#endif
	float maxAoDistance = u_ambienOcclusionRadius * 0.001;
	float aoBlendFactor = (maxAoDistance > 1e-6) ? smoothstep(0.0, maxAoDistance, screenDistance) : 1.0;
	float fadeToFull = smoothstep(u_ambienOcclusionStrength, 1.0, t);
	float ao = mix(aoBlendFactor, 1.0, fadeToFull);
	shadows *= ao;

	shadows = mix(shadows, 1.0, shapeFactor);

	// Refraction
#ifdef REFRACTION
	float effectiveRefraction = (mat.refraction > 0.0) ? mat.refraction : u_refractionIntensity;
	float refractionDistance = -1.0 / (1.0 - clamp(((-distance * 100.0) + 1.0), 0.0, 10.0));
	refractionDistance = max(0.0, refractionDistance - 0.1);
	vec2 backgroundOffset = 0.01 * shapeFactor * refractionDistance * normal * effectiveRefraction;
	backgroundOffset.x *= u_resolution.y / u_resolution.x;

	vec2 finalUV = screenUV + backgroundOffset;
	ivec2 texSize = textureSize(t_backgroundTexture, 0);
	vec2 texelSize = 1.0 / vec2(texSize);

	vec2 uv0 = finalUV + vec2(-0.125, -0.375) * texelSize;
	vec2 uv1 = finalUV + vec2(0.375, -0.125) * texelSize;
	vec2 uv2 = finalUV + vec2(0.125, 0.375) * texelSize;
	vec2 uv3 = finalUV + vec2(-0.375, 0.125) * texelSize;

	vec3 col0 = texture(t_backgroundTexture, uv0).rgb;
	vec3 col1 = texture(t_backgroundTexture, uv1).rgb;
	vec3 col2 = texture(t_backgroundTexture, uv2).rgb;
	vec3 col3 = texture(t_backgroundTexture, uv3).rgb;

	vec3 backgroundColor = (col0 + col1 + col2 + col3) * 0.25;
	backgroundColor = mix(texture(t_backgroundTexture, screenUV).rgb, backgroundColor, shapeFactor);
#else
	vec3 backgroundColor = texture(t_backgroundTexture, screenUV).rgb;
#endif

	float finalAlpha = clamp(alpha + 0.1, 0.0, 1.0) * shapeFactor;

#ifdef DEBUG_SHOW_NORMALS
	color = vec3(normal, 0.0);
#endif

	float litFactor = clamp((shadows - SHADOW_VALUE) / (1.0 - SHADOW_VALUE), 0.0, 1.0);
	float ambientOcclusion = clamp(shadows / SHADOW_VALUE, 0.0, 1.0);

	vec3 shadowTransmittance = mix(u_shadowTint * ambientOcclusion, vec3(1.0), litFactor);
	vec3 shadedBackground = backgroundColor * shadowTransmittance;

	vec3 emissionColor = mat.emission * mat.color.rgb * shapeFactor;
	vec3 litShapeColor = (color * accumulatedShapeLight) + emissionColor;

	color = mix(litShapeColor, shadedBackground, 1.0 - finalAlpha);

	FragColor = vec4(color, 1.0);

#ifdef DEBUG_SHOW_DISTANCE
	float debugDistance = 0.5 * texture(t_distanceSampledTexture, screenUV).x;
	float value = 0.5 * (cos(500.0 * debugDistance) + 1.0);
	vec3 debugColor = debugDistance > 0.0 ? mix(vec3(1), vec3(0.2), value) :
						  (debugDistance + 1.0) * mix(vec3(1.0, 0.2, 0.2), vec3(0.9, 0.5, 0.5), value);
	FragColor = vec4(debugColor, 1.0);
#endif
}
