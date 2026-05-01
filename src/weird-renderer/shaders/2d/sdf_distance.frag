#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

#include "../common/shapes.glsl"

// #define BLEND_SHAPES
// #define MOTION_BLUR
// #define DEBUG_SHOW_GRID

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D u_diffuse;
uniform sampler2D u_specular;

uniform vec4 u_lightColor;
uniform vec3 u_lightPos;
uniform vec3 u_camPos;

uniform float u_time;
uniform float u_k;
uniform sampler2D t_colorTexture;

uniform int u_loadedObjects;
uniform highp sampler2D t_shapeBuffer;
uniform highp sampler2D t_gridHeader;
uniform highp sampler2D t_gridIndices;

uniform vec2 u_gridBoundsMin;
uniform vec2 u_gridStep;
uniform int u_gridCols;
uniform int u_gridRows;

uniform vec2 u_resolution;
uniform float u_overscan;

uniform mat4 u_camMatrix;
uniform mat4 u_oldCamMatrix;
uniform vec3 u_camPositionChange;
uniform vec3 u_staticColors[16];
uniform vec3 u_directionalLightDirection;

uniform float u_deltaTime;
uniform float u_motionBlurBlendSpeed;

uniform int u_customShapeCount;

const float u_uiScale = 50.0;

// Constants
const int MAX_STEPS = 100;
const float EPSILON = 0.01;
const float NEAR = 0.1;
const float FAR = 100.0;

// Custom shape variables
#define var8 u_time
#define var9 p.x
#define var10 p.y
#define var11 u_uiScale* uv.x
#define var12 u_uiScale* uv.y

#define var0 parameters0.x
#define var1 parameters0.y
#define var2 parameters0.z
#define var3 parameters0.w
#define var4 parameters1.x
#define var5 parameters1.y
#define var6 parameters1.z
#define var7 parameters1.w

const int SEGMENTS = 9;
float sdPolygon(in vec2[SEGMENTS] v, in vec2 p)
{
	float d = dot(p - v[0], p - v[0]);
	float s = 1.0;
	for (int i = 0, j = SEGMENTS - 1; i < SEGMENTS; j = i, i++)
	{
		vec2 e = v[j] - v[i];
		vec2 w = p - v[i];
		vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
		d = min(d, dot(b, b));
		bvec3 c = bvec3(p.y >= v[i].y, p.y<v[j].y, e.x * w.y> e.y * w.x);
		if (all(c) || all(not(c)))
			s *= -1.0;
	}
	return s * sqrt(d);
}

vec3 getColor(vec2 p, vec2 uv)
{
	float minDist = 100000.0;
	float minColorDist = minDist;

	int finalMaterialId = 16;
	float mask = 0.0;

#include "custom_shapes"

	if (minDist <= 0.0)
	{
		// return vec3(minDist, finalMaterialId, 0.0);
	}

	minColorDist = minDist;

	// float shapeDist = minDist;
	// minDist = 1.0; // Disable blending between balls and shapes

	float inv_k = 1.0 / u_k;

	ivec2 cellCoord = ivec2((p - u_gridBoundsMin) / u_gridStep);
	cellCoord = clamp(cellCoord, ivec2(0), ivec2(u_gridCols - 1, u_gridRows - 1));

	vec2 cellData = texelFetch(t_gridHeader, cellCoord, 0).xy;
	int startIndex = int(cellData.x);
	int count = int(cellData.y);

	for (int i = 0; i < count; i++)
	{
		int flatIndex = startIndex + i;
		ivec2 indexCoord = ivec2(flatIndex % 1024, flatIndex / 1024);
		int objectIndex = int(texelFetch(t_gridIndices, indexCoord, 0).x);

		// Shape buffer wraps to 2D at 16384 texels wide (matches DataBuffer::uploadRawData on ES drivers)
		vec4 positionSizeMaterial = texelFetch(t_shapeBuffer, ivec2(objectIndex % 16384, objectIndex / 16384), 0);
		int materialId = int(positionSizeMaterial.w);

#ifdef UI_PIPELINE
			float objectDist = shape_circle(p - positionSizeMaterial.xy, 5.0);
#else
			float objectDist = shape_circle(p - positionSizeMaterial.xy);
#endif

			// Inside ball mask is set to 0
			mask = objectDist <= 0.0 ? -objectDist * 4.0 : mask;

#ifdef BLEND_SHAPES
		finalMaterialId = objectDist <= minColorDist ? materialId : finalMaterialId;
		minDist = fOpUnionSoft(objectDist, minDist, u_k, inv_k);
		minColorDist = min(minColorDist, objectDist);
#else
		finalMaterialId = objectDist <= minDist ? materialId : finalMaterialId;
		minDist = min(minDist, objectDist);
#endif
	}

	// minDist = min(minDist, shapeDist);

#ifdef UI_PIPELINE
	minDist = min(minDist, 10.0); // Clamp max distance in UI mode
#endif

#ifdef DEBUG_SHOW_GRID
	finalMaterialId = count / 10; // Color cells based on how many objects they contain, for debugging purposes
#endif

	return vec3(minDist, max(float(finalMaterialId), 0.0), mask);
}

// Bilinear
vec2 smoothSample(sampler2D tex, vec2 uv)
{
	vec2 texelSize = 1.0 / u_resolution;

	vec2 P = uv * u_resolution - 0.5;

	// 3. Get the integer indices of the top-left texel (i, j)
	ivec2 i_j = ivec2(floor(P));

	// 4. Calculate the fractional distance (weights) from the top-left center
	// This gives the interpolation factors (fx, fy) between 0.0 and 1.0
	vec2 f = fract(P);

	// 5. Calculate the indices for the four texels (i, j), (i+1, j), (i, j+1), (i+1, j+1)
	// We add 0.5 to center the sampling point inside the texel
	vec2 uv00 = (vec2(i_j) + 0.5) * texelSize;
	vec2 uv10 = (vec2(i_j) + vec2(1.5, 0.5)) * texelSize;
	vec2 uv01 = (vec2(i_j) + vec2(0.5, 1.5)) * texelSize;
	vec2 uv11 = (vec2(i_j) + vec2(1.5, 1.5)) * texelSize;

	// 6. Sample the four texels (d00, d10, d01, d11)
	vec2 d00 = texture(tex, uv00).xy;
	float d10 = texture(tex, uv10).x;
	float d01 = texture(tex, uv01).x;
	float d11 = texture(tex, uv11).x;

	// 7. Perform the Bilinear Interpolation
	float d_top = mix(d00.x, d10, f.x);
	float d_bottom = mix(d01, d11, f.x);
	float d = mix(d_top, d_bottom, f.y);

	return vec2(d, d00.y);
}

void main()
{
#ifdef UI_PIPELINE
	// UI: origin at bottom-left, UV stays in [0,1] to match screen-space layout
	vec2 uv = v_texCoord;
#else
	// World: remap UV to [-1,1] so the origin is centred on screen
	vec2 uv = (2.0 * v_texCoord) - 1.0;
	uv *= (1.0 + u_overscan);
#endif

	float aspectRatio = u_resolution.x / u_resolution.y; // TODO: uniform
	uv.x *= aspectRatio;

	float zoom = -u_camMatrix[3].z;
	vec2 pos = (zoom * uv) - u_camMatrix[3].xy;

	vec3 result = getColor(pos, v_texCoord);
	float d = result.x;

#ifndef UI_PIPELINE

	float distanceBonus = (0.00002 * zoom * zoom); // Compensate for precision issues when zoomed out far away
	distanceBonus =
		min(distanceBonus,
			1.0 * zoom / u_resolution.y); // Cap distance bonus to prevent artifacts when zoomed out very far away
	d -= distanceBonus;

#endif

	float finalDistance = d / zoom;
	finalDistance *= 0.5 / aspectRatio;

	float material = result.y;
	float mask = result.z;

#ifdef MOTION_BLUR

	// For each fragment, find where the same world point was in the previous frame's texture,
	// compensating for both camera translation and zoom change.
	// Derivation: worldPos = zoom * uv + camPos, so prevUV = (worldPos - oldCamPos) / oldZoom
	// which gives: prevTexCoord = (zoom/oldZoom) * (texCoord - 0.5) + 0.5 + 0.5 * camPositionChange / (oldZoom *
	// [aspectRatio, 1])
	float oldZoom = -u_oldCamMatrix[3].z;
	float zoomRatio = zoom / oldZoom;
	vec2 prevTexCoord;
	prevTexCoord.x = zoomRatio * (v_texCoord.x - 0.5) + 0.5 + 0.5 * u_camPositionChange.x / (oldZoom * aspectRatio);
	prevTexCoord.y = zoomRatio * (v_texCoord.y - 0.5) + 0.5 + 0.5 * u_camPositionChange.y / oldZoom;

	// Sample previous texture with bilinear filtering because aliasing causes this effect to accumulate error
	vec2 previousData = smoothSample(t_colorTexture, prevTexCoord);
	float previousDistance = previousData.x;

	vec4 previousDataExact = texture(t_colorTexture, prevTexCoord);
	float previousDistanceExact = previousDataExact.x;
	float previousMaterial = previousDataExact.y;
	// Different material for object trail?
	material = finalDistance >= 0.0 && previousDistance < 0.0 ? previousMaterial : material;

	// finalDistance = finalDistance <= 0.0 ? 2.0 * finalDistance : finalDistance;

	// finalDistance = step(previousDistance, finalDistance - previousDistance);
	float distanceChange = finalDistance - previousDistanceExact;
	// distanceChange = clamp(distanceChange, -0.1, 0.05);

	// qqdistanceChange *= (distanceChange > 0.0) ? 1.0 : 1.0;
	float blendDistance = previousDistance + (distanceChange * min(1.0, u_deltaTime * u_motionBlurBlendSpeed) *
											  (distanceChange < 0.0 ? 5.0 : 1.0));

	// blendDistance = mix(previousDistance, finalDistance, 0.01) + (distanceChange * u_deltaTime * 10.0);

	blendDistance = clamp(blendDistance, -0.1, 0.1);

#ifdef UI_PIPELINE

	finalDistance = blendDistance;

#else

	finalDistance = mix(blendDistance, finalDistance, mask);

#endif

#endif

#ifdef UI_PIPELINE
// float pixelSize = 0.3 / u_resolution.x;
// finalDistance = abs(finalDistance - pixelSize) - (pixelSize);
#endif

#ifdef DEBUG_SHOW_GRID
	vec2 localP = pos - u_gridBoundsMin;
	bool inBounds = localP.x >= 0.0 && localP.y >= 0.0 &&
	                localP.x < float(u_gridCols) * u_gridStep.x &&
	                localP.y < float(u_gridRows) * u_gridStep.y;
	if (inBounds)
	{
		vec2 fracLocal = mod(localP, u_gridStep);
		vec2 distToLine = min(fracLocal, u_gridStep - fracLocal);
		float gridDistWorld = min(distToLine.x, distToLine.y);
		float gridDistNorm = (gridDistWorld / zoom) * (0.5 / aspectRatio);
		float lineHalfWidth = 1.0 / min(u_resolution.x, u_resolution.y);
		if (gridDistNorm < lineHalfWidth)
		{
			finalDistance = gridDistNorm - lineHalfWidth;
			// material = 16.0 - material; // Highlight out-of-bounds areas with a different color
		}
	}
#endif

	FragColor = vec4(finalDistance, material, mask, 0.0);

	// This has no visual effect (multiplying by zero), but it forces
	// the compiler to keep u_time because it's used to calculate FragColor.
	FragColor.a += u_time * 0.001;
}
