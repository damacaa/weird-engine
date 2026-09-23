#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

// SDF Operations
float fOpUnionSoft(float a, float b, float r)
{
	float h = max(r - abs(a - b), 0.0);
	return min(a, b) - h * h * 0.25 / r;
}

float fOpUnionSoft(float a, float b, float r, float invR)
{
	float h = max(r - abs(a - b), 0.0);
	return min(a, b) - h * h * 0.25 * invR;
}

vec2 fOpUnionSoft_blend(float a, float b, float r)
{
	float h = max(r - abs(a - b), 0.0);
	float blend = h / r;
	return vec2(min(a, b) - h * h * 0.25 / r, blend);
}

vec2 fOpUnionSoft_blend(float a, float b, float r, float invR)
{
	float h = max(r - abs(a - b), 0.0);
	float blend = h * invR;
	return vec2(min(a, b) - h * h * 0.25 * invR, blend);
}

float fOpSubSoft(float a, float b, float r)
{
	return -fOpUnionSoft(b, -a, r);
}

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform float u_time;
uniform float u_audioVolume;
uniform int u_loadedObjects;
uniform int u_shapeCount;
uniform highp sampler2D t_shapeBuffer;

uniform vec2 u_resolution;
uniform float u_overscan;
uniform mat4 u_camMatrix;

// Shape variables
#define var8 u_time
#define var9 p.x
#define var10 p.y
#define var11 u_audioVolume

#define var0 parameters0.x
#define var1 parameters0.y
#define var2 parameters0.z
#define var3 parameters0.w
#define var4 parameters1.x
#define var5 parameters1.y
#define var6 parameters1.z
#define var7 parameters1.w

// Slot 0: Injected dynamically by SDFShaderGenerationSystem.
// Contains deduplicated shape helpers (e.g. sdTriangle_impl) and evaluate_sdf_<id>() functions.
#include "helper_functions"

void fetchShapeParams(int idx, out vec4 p0, out vec4 p1)
{
	p0 = texelFetch(t_shapeBuffer, ivec2(idx % 16384, idx / 16384), 0);
	p1 = texelFetch(t_shapeBuffer, ivec2((idx + 1) % 16384, (idx + 1) / 16384), 0);
}

void applyShapeAddition(float dist, int material, inout float currentMinDist, inout float currentBlend, inout int groupColor)
{
	if (dist <= currentMinDist)
	{
		currentBlend = 0.0;
		groupColor = material;
	}
	currentMinDist = min(currentMinDist, dist);
}

void applyShapeSubtraction(float dist, inout float currentMinDist)
{
	currentMinDist = max(currentMinDist, -dist);
}

void applyShapeIntersection(float dist, inout float currentMinDist)
{
	currentMinDist = max(currentMinDist, dist);
}

void applyShapeSmoothAddition(float dist, int material, float smoothFactor, inout float currentMinDist, inout float currentBlend, inout int groupColor)
{
	vec2 res = fOpUnionSoft_blend(currentMinDist, dist, smoothFactor);
	if (res.y > 0.0)
	{
		currentBlend = max(currentBlend, res.y);
	}
	else if (dist < currentMinDist)
	{
		currentBlend = 0.0;
	}
	if (dist <= res.x)
	{
		groupColor = material;
	}
	currentMinDist = res.x;
}

void applyShapeSmoothSubtraction(float dist, float smoothFactor, inout float currentMinDist)
{
	currentMinDist = fOpSubSoft(currentMinDist, dist, smoothFactor);
}

void flushShapeGroup(float groupDist, float groupBlend, int groupColor, inout float minDist, inout float globalBlend, inout int finalMaterialId)
{
	if (groupDist <= max(minDist, 0.0))
	{
		finalMaterialId = groupColor;
	}
	if (groupDist <= minDist)
	{
		globalBlend = groupBlend;
	}
	if (minDist > groupDist)
	{
		minDist = groupDist;
	}
}

float modifyDistanceBasedOnMaterial(float dist, int materialId, int objectId)
{
	return dist;
}

vec2 getShapeDistanceMaterial(vec2 p)
{
	float minDist = 100000.0;
	int finalMaterialId = 0;
	float globalBlend = 0.0;

	// Slot 1: Injected dynamically by SDFShaderGenerationSystem.
	// Contains the unrolled per-instance shape evaluation loop, CSG combinations, and group flushes.
#include "shapes"

	return vec2(minDist, max(float(finalMaterialId), 0.0));
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

	float aspectRatio = u_resolution.x / u_resolution.y;
	uv.x *= aspectRatio;

	float zoom = -u_camMatrix[3].z;
	vec2 pos = (zoom * uv) - u_camMatrix[3].xy;

	vec2 res = getShapeDistanceMaterial(pos);

	FragColor = vec4(res.x, res.y, 0.0, 1.0);
}
