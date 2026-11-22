#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp usampler2D;

// SDF Operations
float fOpUnionSoft(float a, float b, float r)
{
	if (r <= 1e-6)
		return min(a, b);
	float h = max(r - abs(a - b), 0.0);
	return min(a, b) - h * h * 0.25 / r;
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
uniform int u_shapeCount;
uniform highp sampler2D t_shapeBuffer;
uniform highp usampler2D t_shapeTileHeader;
uniform highp usampler2D t_shapeTileIndices;
uniform int u_shapeTileCols;
uniform int u_shapeTileRows;
uniform vec2 u_shapeFadeRange;
uniform float u_shapeFadeReach;

uniform vec2 u_resolution;
uniform float u_overscan;
uniform mat4 u_camMatrix;

// Shape variables (used by custom AST-generated evaluate_sdf_<id> functions)
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

// Canonical built-in GPU primitive IDs (must match GpuShape2D::BuiltinType)
const int GPU_SHAPE_CIRCLE = 0;
const int GPU_SHAPE_BOX = 1;
const int GPU_SHAPE_TRIANGLE = 2;
const int GPU_SHAPE_LINE = 3;
const int GPU_SHAPE_RAMP = 4;
const int GPU_SHAPE_SINE_WAVE = 5;
const int GPU_SHAPE_STAR = 6;

const uint FLAG_ONION = 1u;
const uint FLAG_GLOBAL_GROUP = 2u;

const int COMB_ADDITION = 0;
const int COMB_SUBTRACTION = 1;
const int COMB_INTERSECTION = 2;
const int COMB_SMOOTH_ADDITION = 3;
const int COMB_SMOOTH_SUBTRACTION = 4;

// Built-in collapsed 2D SDF evaluators (rotation and constant subexpressions precomputed on CPU)
float evalBuiltinCircle(vec2 p, vec4 p0)
{
	return length(p - p0.xy) - p0.z;
}

float evalBuiltinBox(vec2 p, vec4 p0, vec4 p1)
{
	vec2 q = p - p0.xy;
	q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
	vec2 d = abs(q) - p0.zw;
	return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - p1.z;
}

float evalBuiltinTriangle(vec2 p, vec4 p0, vec4 p1)
{
	vec2 q = p - p0.xy;
	q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
	q.y -= p1.w;

	float hw = p0.z * 0.5;
	float yBot = -p0.w * (1.0 / 3.0);
	float yTop = p0.w * (2.0 / 3.0);

	vec2 v0 = vec2(-hw, yBot);
	vec2 v1 = vec2(hw, yBot);
	vec2 v2 = vec2(0.0, yTop);

	// Evaluate edges sequentially to keep peak live registers low on mobile TBDR GPUs.
	vec2 e = v1 - v0;
	vec2 e2 = v0 - v2;
	float s = sign(e.x * e2.y - e.y * e2.x);

	vec2 d = q - v0;
	vec2 pq = d - e * clamp(dot(d, e) / max(dot(e, e), 1e-12), 0.0, 1.0);
	float dx = dot(pq, pq);
	float dy = s * (d.x * e.y - d.y * e.x);

	e = v2 - v1;
	d = q - v1;
	pq = d - e * clamp(dot(d, e) / max(dot(e, e), 1e-12), 0.0, 1.0);
	dx = min(dx, dot(pq, pq));
	dy = min(dy, s * (d.x * e.y - d.y * e.x));

	d = q - v2;
	pq = d - e2 * clamp(dot(d, e2) / max(dot(e2, e2), 1e-12), 0.0, 1.0);
	dx = min(dx, dot(pq, pq));
	dy = min(dy, s * (d.x * e2.y - d.y * e2.x));

	return -sqrt(dx) * sign(dy) - p1.z;
}

float evalBuiltinLine(vec2 p, vec4 p0, vec4 p1)
{
	vec2 pa = p - p0.xy;
	vec2 ba = p0.zw;
	float h = clamp(dot(pa, ba) * p1.x, 0.0, 1.0);
	return length(pa - ba * h) - p1.y;
}

float evalBuiltinRamp(vec2 p, vec4 p0, vec4 p1)
{
	vec2 q = p - p0.xy;
	q = vec2(p1.y * q.x - p1.z * q.y, p1.z * q.x + p1.y * q.y);

	float wi = p0.z;
	float he = p0.w;
	vec2 e = vec2(wi, p1.x);

	q *= (q.x >= 0.0 ? 1.0 : -1.0);

	vec2 w = q - e;
	float dy0 = -w.x;
	w.y -= clamp(w.y, -he, he);
	float dx0 = dot(w, w);

	float s = q.y * e.x - q.x * e.y;
	q *= (s >= 0.0 ? 1.0 : -1.0);

	vec2 v = q - vec2(0.0, he);
	v -= e * clamp(dot(v, e) * p1.w, -1.0, 1.0);

	float dx1 = dot(v, v);
	float dy1 = wi * he - abs(s);

	return sqrt(min(dx0, dx1)) * sign(-min(dy0, dy1));
}

float evalBuiltinSineWave(vec2 p, vec4 p0)
{
	return (p.y - p0.w) - p0.x * sin(p0.y * p.x + p0.z * u_time);
}

float evalBuiltinStar(vec2 p, vec4 p0, vec4 p1)
{
	vec2 q = p - p0.xy;
	float dist = length(q) - p0.z;
	float angle = p1.x * atan(q.y, q.x) - p1.y * u_time;
	return dist + p0.w * sin(angle);
}

// Slot 0: Injected dynamically by SDFShaderGenerationSystem (custom evaluate_sdf_<id> functions).
#include "helper_functions"

float evaluateShape(mediump int shapeId, vec2 p, vec4 parameters0, vec4 parameters1)
{
	switch (shapeId)
	{
		case GPU_SHAPE_CIRCLE:
			return evalBuiltinCircle(p, parameters0);
		case GPU_SHAPE_BOX:
			return evalBuiltinBox(p, parameters0, parameters1);
		case GPU_SHAPE_TRIANGLE:
			return evalBuiltinTriangle(p, parameters0, parameters1);
		case GPU_SHAPE_LINE:
			return evalBuiltinLine(p, parameters0, parameters1);
		case GPU_SHAPE_RAMP:
			return evalBuiltinRamp(p, parameters0, parameters1);
		case GPU_SHAPE_SINE_WAVE:
			return evalBuiltinSineWave(p, parameters0);
		case GPU_SHAPE_STAR:
			return evalBuiltinStar(p, parameters0, parameters1);
			// Slot 1: Injected dynamically by SDFShaderGenerationSystem (custom shape dispatch cases).
#include "custom_shape_cases"
		default:
			return 10000.0;
	}
}

void fetchShapeInstance(int shapeIndex, out vec4 meta, out vec4 p0, out vec4 p1)
{
	int base = shapeIndex * 3;
	meta = texelFetch(t_shapeBuffer, ivec2(base & 16383, base >> 14), 0);
	int idx1 = base + 1;
	p0 = texelFetch(t_shapeBuffer, ivec2(idx1 & 16383, idx1 >> 14), 0);
	int idx2 = base + 2;
	p1 = texelFetch(t_shapeBuffer, ivec2(idx2 & 16383, idx2 >> 14), 0);
}

void applyShapeAddition(float dist, mediump int material, inout float currentMinDist, inout mediump int groupColor)
{
	if (dist <= currentMinDist)
	{
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

void applyShapeSmoothAddition(float dist, mediump int material, float smoothFactor, inout float currentMinDist,
							  inout mediump int groupColor)
{
	float res = fOpUnionSoft(currentMinDist, dist, smoothFactor);
	if (dist <= res)
	{
		groupColor = material;
	}
	currentMinDist = res;
}

void applyShapeSmoothSubtraction(float dist, float smoothFactor, inout float currentMinDist)
{
	currentMinDist = fOpSubSoft(currentMinDist, dist, smoothFactor);
}

void applyShapeCombination(mediump int combination, float dist, mediump int material, float smoothFactor,
						   inout float targetDist, inout mediump int targetColor)
{
	if (combination == COMB_ADDITION)
	{
		applyShapeAddition(dist, material, targetDist, targetColor);
	}
	else if (combination == COMB_SUBTRACTION)
	{
		applyShapeSubtraction(dist, targetDist);
	}
	else if (combination == COMB_INTERSECTION)
	{
		applyShapeIntersection(dist, targetDist);
	}
	else if (combination == COMB_SMOOTH_ADDITION)
	{
		applyShapeSmoothAddition(dist, material, smoothFactor, targetDist, targetColor);
	}
	else if (combination == COMB_SMOOTH_SUBTRACTION)
	{
		applyShapeSmoothSubtraction(dist, smoothFactor, targetDist);
	}
}

void flushShapeGroup(float groupDist, mediump int groupColor, inout float minDist, inout mediump int finalMaterialId)
{
	if (groupDist <= max(minDist, 0.0))
	{
		finalMaterialId = groupColor;
	}
	minDist = min(minDist, groupDist);
}

vec2 getShapeDistanceMaterial(vec2 p, uint tileStart, uint tileCount)
{
	float minDist = 50000.0;
	mediump int finalMaterialId = 0;

	int currentGroup = -1;
	float groupDist = 10000.0;
	mediump int currentGroupColor = -1;

	for (uint k = 0u; k < tileCount; ++k)
	{
		uint flatIdx = tileStart + k;
		int shapeIdx = int(texelFetch(t_shapeTileIndices, ivec2(int(flatIdx & 63u), int(flatIdx >> 6u)), 0).x);

		vec4 meta, p0, p1;
		fetchShapeInstance(shapeIdx, meta, p0, p1);

		// Evaluate shape before unpacking remaining metadata so fewer scalars stay live across the switch.
		mediump int shapeId = int(uint(meta.x + 0.5) & 0xFFFFu);
		float dist = evaluateShape(shapeId, p, p0, p1);

		uint packed0 = uint(meta.x + 0.5);
		uint packed1 = uint(meta.y + 0.5);
		mediump int combination = int((packed0 >> 16u) & 0x7u);
		mediump uint flags = packed0 >> 19u;
		int group = int(packed1 & 0xFFFFu);
		mediump int material = int((packed1 >> 16u) & 0x3Fu);

		if (group != currentGroup)
		{
			if (currentGroup != -1 && currentGroup != 65535)
			{
				flushShapeGroup(groupDist, currentGroupColor, minDist, finalMaterialId);
			}
			currentGroup = group;
			groupDist = 10000.0;
		}

		if ((flags & FLAG_ONION) != 0u)
		{
			dist = abs(dist) - meta.w;
		}

		if ((flags & FLAG_GLOBAL_GROUP) != 0u)
		{
			applyShapeCombination(combination, dist, material, meta.z, minDist, finalMaterialId);
		}
		else
		{
			applyShapeCombination(combination, dist, material, meta.z, groupDist, currentGroupColor);
		}
	}

	if (currentGroup != -1 && currentGroup != 65535)
	{
		flushShapeGroup(groupDist, currentGroupColor, minDist, finalMaterialId);
	}

	return vec2(minDist, max(float(finalMaterialId), 0.0));
}

void main()
{
	ivec2 fragPx = ivec2(gl_FragCoord.xy);
	ivec2 tileCoord = clamp(fragPx >> 4, ivec2(0), ivec2(u_shapeTileCols - 1, u_shapeTileRows - 1));
	uvec2 tileData = texelFetch(t_shapeTileHeader, tileCoord, 0).xy;
	uint tileStart = tileData.x;
	uint tileCount = tileData.y;

#ifndef DEBUG_SHOW_SHAPE_TILES
	if (tileCount == 0u)
	{
		FragColor = vec4(u_shapeFadeReach, 0.0, 0.0, 1.0);
		return;
	}
#endif

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

	vec2 res = (tileCount > 0u) ? getShapeDistanceMaterial(pos, tileStart, tileCount) : vec2(50000.0, 0.0);

	// Smoothly saturate positive distance with a C2 Hermite curve so populated tiles meet empty tiles
	// identically at tile boundaries with zero discontinuity, while interior (d < 0) remains exact.
	float fadeStart = u_shapeFadeRange.x;
	float fadeSpan = max(u_shapeFadeRange.y, 1e-4);
	float d = res.x;
	float t = clamp((d - fadeStart) / fadeSpan, 0.0, 1.0);
	float t2 = t * t;
	float effectiveDist = (d <= fadeStart) ? d : fadeStart + fadeSpan * (t - t2 * t + 0.5 * t2 * t2);
	float finalMat = res.y;

#ifdef DEBUG_SHOW_SHAPE_TILES
	ivec2 localPx = fragPx & 15;
	if (localPx.x == 0 || localPx.y == 0)
	{
		effectiveDist = min(effectiveDist, -0.01);
		finalMat = float(min(tileCount, 15u));
	}
#endif

	FragColor = vec4(effectiveDist, finalMat, 0.0, 1.0);
}
