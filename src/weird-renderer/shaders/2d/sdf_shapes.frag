#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

// SDF Operations
float fOpUnionSoft(float a, float b, float r)
{
	if (r <= 1e-6)
		return min(a, b);
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
	if (r <= 1e-6)
		return vec2(min(a, b), 0.0);
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
uniform int u_shapeCount;
uniform highp sampler2D t_shapeBuffer;

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

	float w = p0.z;
	float h = p0.w;
	vec2 v0 = vec2(-w * 0.5, -h / 3.0);
	vec2 v1 = vec2(w * 0.5, -h / 3.0);
	vec2 v2 = vec2(0.0, 2.0 * h / 3.0);

	vec2 e0 = v1 - v0;
	vec2 e1 = v2 - v1;
	vec2 e2 = v0 - v2;

	vec2 d0 = q - v0;
	vec2 d1 = q - v1;
	vec2 d2 = q - v2;

	vec2 pq0 = d0 - e0 * clamp(dot(d0, e0) / max(dot(e0, e0), 1e-12), 0.0, 1.0);
	vec2 pq1 = d1 - e1 * clamp(dot(d1, e1) / max(dot(e1, e1), 1e-12), 0.0, 1.0);
	vec2 pq2 = d2 - e2 * clamp(dot(d2, e2) / max(dot(e2, e2), 1e-12), 0.0, 1.0);

	float s = sign(e0.x * e2.y - e0.y * e2.x);
	float dx = min(min(dot(pq0, pq0), dot(pq1, pq1)), dot(pq2, pq2));
	float dy = min(min(s * (d0.x * e0.y - d0.y * e0.x), s * (d1.x * e1.y - d1.y * e1.x)),
				   s * (d2.x * e2.y - d2.y * e2.x));

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
	float sk = p1.x;
	vec2 e = vec2(wi, sk);

	q *= (q.x >= 0.0 ? 1.0 : -1.0);

	vec2 w = q - e;
	float wy = w.y - clamp(w.y, -he, he);
	vec2 wClamped = vec2(w.x, wy);

	float dx0 = dot(wClamped, wClamped);
	float dy0 = -w.x;

	float s = q.y * e.x - q.x * e.y;
	q *= (s >= 0.0 ? 1.0 : -1.0);

	vec2 v = q - vec2(0.0, he);
	float h = clamp(dot(v, e) * p1.w, -1.0, 1.0);
	v -= e * h;

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

float evaluateShape(int shapeId, vec2 p, vec4 parameters0, vec4 parameters1)
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

void applyShapeAddition(float dist, int material, inout float currentMinDist, inout float currentBlend,
						inout int groupColor)
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

void applyShapeSmoothAddition(float dist, int material, float smoothFactor, inout float currentMinDist,
							  inout float currentBlend, inout int groupColor)
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

void applyShapeCombination(int combination, float dist, int material, float smoothFactor, inout float targetDist,
						   inout float targetBlend, inout int targetColor)
{
	if (combination == COMB_ADDITION)
	{
		applyShapeAddition(dist, material, targetDist, targetBlend, targetColor);
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
		applyShapeSmoothAddition(dist, material, smoothFactor, targetDist, targetBlend, targetColor);
	}
	else if (combination == COMB_SMOOTH_SUBTRACTION)
	{
		applyShapeSmoothSubtraction(dist, smoothFactor, targetDist);
	}
}

void flushShapeGroup(float groupDist, float groupBlend, int groupColor, inout float minDist, inout float globalBlend,
					 inout int finalMaterialId)
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

vec2 getShapeDistanceMaterial(vec2 p)
{
	float minDist = 50000.0;
	int finalMaterialId = 0;
	float globalBlend = 0.0;

	int currentGroup = -1;
	float groupDist = 10000.0;
	float groupBlend = 0.0;
	int currentGroupColor = -1;

	for (int i = 0; i < u_shapeCount; ++i)
	{
		vec4 meta, p0, p1;
		fetchShapeInstance(i, meta, p0, p1);

		uint packed0 = uint(meta.x + 0.5);
		uint packed1 = uint(meta.y + 0.5);

		int shapeId = int(packed0 & 0xFFFFu);
		int combination = int((packed0 >> 16u) & 0x7u);
		uint flags = packed0 >> 19u;

		int group = int(packed1 & 0xFFFFu);
		int material = int((packed1 >> 16u) & 0x3Fu);
		float smoothFactor = meta.z;
		float onionThickness = meta.w;

		if (group != currentGroup)
		{
			if (currentGroup != -1 && currentGroup != 65535)
			{
				flushShapeGroup(groupDist, groupBlend, currentGroupColor, minDist, globalBlend, finalMaterialId);
			}
			currentGroup = group;
			groupDist = 10000.0;
			groupBlend = 0.0;
		}

		float dist = evaluateShape(shapeId, p, p0, p1);
		if ((flags & FLAG_ONION) != 0u)
		{
			dist = abs(dist) - onionThickness;
		}

		if ((flags & FLAG_GLOBAL_GROUP) != 0u)
		{
			applyShapeCombination(combination, dist, material, smoothFactor, minDist, globalBlend, finalMaterialId);
		}
		else
		{
			applyShapeCombination(combination, dist, material, smoothFactor, groupDist, groupBlend, currentGroupColor);
		}
	}

	if (currentGroup != -1 && currentGroup != 65535)
	{
		flushShapeGroup(groupDist, groupBlend, currentGroupColor, minDist, globalBlend, finalMaterialId);
	}

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
