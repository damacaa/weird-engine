#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp isampler2D;

#include "../common/shapes.glsl"

// #define PATH_TRACING
// #define FISH_EYE
// #define ANTIALIASING
// #define COMBINE_2D_AND_3D_SDFS

vec2 fOpUnionSoft2(float a, float b, float k)
{
	float h = max(k - abs(a - b), 0.0) / k;
	float m = h * h * h * 0.5;
	float s = m * k * (1.0 / 3.0);
	return (a < b) ? vec2(a - s, m) : vec2(b - s, 1.0 - m);
}

float fOpSmoochUnion(float d1, float d2, float k)
{
	float h = max(k - abs(d1 - d2), 0.0);
	return min(d1, d2) + h * h * 0.35 / k;
}

// Shapes
float fSphere(vec3 p, float r)
{
	return length(p) - r;
}

float vmax(vec3 v)
{
	return max(max(v.x, v.y), v.z);
}

float fBox(vec3 p, vec3 b)
{
	vec3 d = abs(p) - b;
	return length(max(d, vec3(0.0))) + vmax(min(d, vec3(0.0)));
}

float fPlane(vec3 p, vec3 n, float distanceFromOrigin)
{
	return dot(p, n) + distanceFromOrigin;
}

float fCylinder(vec3 p, float r, float height)
{
	float d = length(p.xz) - r;
	d = max(d, abs(p.y) - height);
	return d;
}

// Outputs colors in RGBA
layout(location = 0) out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_previousColor;
uniform sampler2D t_depthTexture;       // GBuffer depth (mesh geometry)
uniform highp sampler2D t_shapeBuffer;
// Deferred GBuffer
uniform sampler2D t_gbufferAlbedo;      // RGB = mesh diffuse, A = specular
uniform sampler2D t_gbufferWorldPos;    // RGB = world-space position, A = 1 if valid
uniform sampler2D t_gbufferNormal;      // RGB = world-space normal
uniform isampler2D t_gbufferMaterial;   // R = material ID
uniform sampler2D t_gbufferBackDepth;   // Back-face depth

uniform int u_loadedObjects;
uniform int u_customShapeCount;
uniform int u_frameCounter;
uniform int u_rayBounces;

uniform mat4 u_camMatrix;
uniform mat4 u_viewProjection;
uniform float u_fov;
uniform vec2 u_resolution;
struct MaterialData
{
	vec4 color;
	float metallic;
	float roughness;
	int pattern;
	float patternScale;
	vec4 secondaryColor;
};
uniform MaterialData u_materials[16];

struct Light
{
	vec3 position;
	vec3 direction;
	vec4 color;
	int type;
};
#define MAX_LIGHTS 8
uniform int u_numLights;
uniform Light u_lights[MAX_LIGHTS];

uniform float u_time;

const int MAX_STEPS = 256;
const float RAYMARCH_EPSILON = 0.001;
const float NORMAL_EPSILON = 0.0001;
uniform float u_near;
uniform float u_far;

const float OVERSHOOT = 1.0;
const float DOT_BLEND_K = 0.2;

// === Render Style Parameters (Post-Processing) =================
// contrast: artificially boosts contrast and crushes colors for that raw 90s CG render look
uniform float u_contrast;
// ===============================================================

// Custom shape variables. In 3D these are evaluated on the XZ plane,
// producing vertically extruded SDFs from the existing 2D math expressions.
#define var8 u_time
#define var9 p.x
#define var10 p.y
#define var11 p.z

#define var0 parameters0.x
#define var1 parameters0.y
#define var2 parameters0.z
#define var3 parameters0.w
#define var4 parameters1.x
#define var5 parameters1.y
#define var6 parameters1.z
#define var7 parameters1.w

// Hash
float hash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Interpolation
float fade(float t)
{
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Gradient noise
float grad(vec2 p, vec2 ip)
{
	vec2 g = vec2(hash(ip), hash(ip + 1.0));
	g = normalize(g * 2.0 - 1.0);
	return dot(p - ip, g);
}

// Perlin Noise 2D
float perlin(vec2 p)
{
	vec2 ip = floor(p);
	vec2 fp = fract(p);

	float a = grad(p, ip);
	float b = grad(p, ip + vec2(1.0, 0.0));
	float c = grad(p, ip + vec2(0.0, 1.0));
	float d = grad(p, ip + vec2(1.0, 1.0));

	vec2 f = vec2(fade(fp.x), fade(fp.y));

	float ab = mix(a, b, f.x);
	float cd = mix(c, d, f.x);
	return mix(ab, cd, f.y);
}

// Hash 3D
vec3 hash3D(vec3 p)
{
	p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
			 dot(p, vec3(269.5, 183.3, 246.1)),
			 dot(p, vec3(113.5, 271.9, 124.6)));
	return normalize(fract(sin(p) * 43758.5453123) * 2.0 - 1.0);
}

// Gradient noise 3D
float grad3D(vec3 p, vec3 ip)
{
	return dot(p - ip, hash3D(ip));
}

// Perlin Noise 3D
float perlin3D(vec3 p)
{
	vec3 ip = floor(p);
	vec3 fp = fract(p);

	float a000 = grad3D(p, ip + vec3(0.0, 0.0, 0.0));
	float a100 = grad3D(p, ip + vec3(1.0, 0.0, 0.0));
	float a010 = grad3D(p, ip + vec3(0.0, 1.0, 0.0));
	float a110 = grad3D(p, ip + vec3(1.0, 1.0, 0.0));
	float a001 = grad3D(p, ip + vec3(0.0, 0.0, 1.0));
	float a101 = grad3D(p, ip + vec3(1.0, 0.0, 1.0));
	float a011 = grad3D(p, ip + vec3(0.0, 1.0, 1.0));
	float a111 = grad3D(p, ip + vec3(1.0, 1.0, 1.0));

	vec3 f = vec3(fade(fp.x), fade(fp.y), fade(fp.z));

	float mix00 = mix(a000, a100, f.x);
	float mix01 = mix(a010, a110, f.x);
	float mix10 = mix(a001, a101, f.x);
	float mix11 = mix(a011, a111, f.x);

	float mix0 = mix(mix00, mix01, f.y);
	float mix1 = mix(mix10, mix11, f.y);

	return mix(mix0, mix1, f.z);
}

// ==========================================
// STYLIZED DITHERING TRANSPARENCY
// ==========================================
const int bayer4x4[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);

float GetBayerThreshold(vec2 coord)
{
	int x = int(coord.x) % 4;
	int y = int(coord.y) % 4;
	float bayerValue = float(bayer4x4[y * 4 + x]) + 0.5;
	return bayerValue / 16.0;
}

// Generates a consistent 2D offset based on an object ID to prevent Dither Correlation
vec2 HashID(float id)
{
	return vec2(fract(sin(id * 12.9898) * 43758.5453) * 100.0, fract(sin(id * 78.233) * 43758.5453) * 100.0);
}

// The reusable Transparency Dither Function
float modifyDistanceBasedOnMaterial(float dist, int materialId, int objectId)
{
	float alpha = materialId < 16 ? u_materials[materialId].color.a : 1.0;

	if (alpha < 1.0)
	{
		vec2 offset = HashID(float(objectId)); // +1 to avoid zero ID correlation

#ifdef PATH_TRACING
		// Step through all 16 states of the 4x4 Bayer matrix using the frame counter.
		// Since we have Temporal Accumulation (TAA), this perfectly averages out the dither noise
		// into smooth glass-like transparency over 16 frames!
		offset += vec2(float(u_frameCounter % 4), float((u_frameCounter / 4) % 4));
#endif

		// Note: Change 1.0 to 2.0 or 4.0 here if you want chunkier retro dither pixels
		float bayer = GetBayerThreshold((gl_FragCoord.xy + offset) / 1.0);

		if (alpha <= bayer)
		{
			return u_far + 100.0; // Skip surface by pushing distance past the FAR plane
		}
	}
	return dist;
}
// ==========================================

// Projects a world-space point into GBuffer UV coordinates.
// Returns the UV in [0,1] range.  Sets `valid` to false if the point
// projects outside the screen or behind the camera.
vec2 worldToGBufferUV(vec3 worldPos, out bool valid)
{
    vec4 clipPos = u_viewProjection * vec4(worldPos, 1.0);

    // Behind the camera
    if (clipPos.w <= 0.0) {
        valid = false;
        return vec2(0.0);
    }

    vec3 ndc = clipPos.xyz / clipPos.w;  // [-1, 1]

    // Out-of-screen check with a small margin to avoid edge artifacts
    const float MARGIN = 0.02;
    if (abs(ndc.x) > 1.0 - MARGIN || abs(ndc.y) > 1.0 - MARGIN || ndc.z > 1.0 || ndc.z < -1.0) {
        valid = false;
        return vec2(0.0);
    }

    valid = true;
    return ndc.xy * 0.5 + 0.5;  // [0, 1] UV
}

// Forward declaration
float linearizeGBufferDepth(float d);

// Approximate signed distance to the rasterized mesh surface.
// Returns a large positive value if no mesh is visible at this projection.
float meshSDF(vec3 p)
{
    bool valid;
    vec2 uv = worldToGBufferUV(p, valid);

    if (!valid)
        return u_far;  // No mesh data here — don't occlude

    float frontDepthRaw = texture(t_depthTexture, uv).r;
    if (frontDepthRaw > 1.0 - 0.0001)
        return u_far;  // Sky pixel — no mesh

    float backDepthRaw  = texture(t_gbufferBackDepth, uv).r;

    float frontDepthLinear = linearizeGBufferDepth(frontDepthRaw);
    float backDepthLinear  = linearizeGBufferDepth(backDepthRaw);

    vec4 clipPos = u_viewProjection * vec4(p, 1.0);
    float sampleDepthRaw = clipPos.z / clipPos.w * 0.5 + 0.5;
    float sampleDepthLinear = linearizeGBufferDepth(sampleDepthRaw);

    // We are inside the mesh if our depth is between front and back faces
    bool insideSlab = (sampleDepthLinear > frontDepthLinear) && (sampleDepthLinear < backDepthLinear);

    // Shrink the mesh slightly to prevent self-shadowing acne
    const float SHADOW_BIAS = 0.05;

    if (insideSlab)
    {
        // Distance from sample to front face
        vec3 meshWorldPos = texture(t_gbufferWorldPos, uv).rgb;
        float distToFront = length(p - meshWorldPos);

        // Acne zone for front face
        if (distToFront < SHADOW_BIAS)
        {
            return SHADOW_BIAS - distToFront;
        }

        // Acne zone for back face (using Z depth difference as approximation)
        float zDistToBack = backDepthLinear - sampleDepthLinear;
        if (zDistToBack < SHADOW_BIAS)
        {
            return SHADOW_BIAS - zDistToBack;
        }

        // Deep inside the mesh: return 0.0 to immediately register a hit!
        // DO NOT return a negative value, otherwise the raymarcher will step backwards and rattle.
        return 0.0;
    }
    else
    {
        if (sampleDepthLinear <= frontDepthLinear)
        {
            // In front of the mesh. Return Euclidean distance + bias so it overshoots
            // slightly into the mesh, landing exactly at the 0.0 boundary.
            vec3 meshWorldPos = texture(t_gbufferWorldPos, uv).rgb;
            float distToFront = length(p - meshWorldPos);
            return min(distToFront + SHADOW_BIAS, 2.0);
        }
        else
        {
            // Behind the back face. Return safe distance + bias.
            float zDistToBack = sampleDepthLinear - backDepthLinear;
            return min(zDistToBack * 0.5 + SHADOW_BIAS, 2.0);
        }
    }
}

vec3 sceneSdf(vec3 p)
{
	float minDist = u_far;
	int finalMaterialId = 16;
	float globalBlend = 0.0;

	// Runtime-generated custom-shape SDF code is injected at this include point.
	// SDFShaderGenerationSystem walks the ECS custom shape components when they are marked dirty,
	// sorts them by group, fetches each shape's math-expression tree from the SDF registry, and
	// turns that tree into GLSL by calling IMathExpression::print(). The generated block also emits
	// parameter loads from t_shapeBuffer, expands any array literals the expression needs, applies
	// the shape's boolean/smooth combination operator, and updates minDist/finalMaterialId for the
	// current group. Shader::setFragmentIncludeCode(1, replacement) then replaces this include with
	// the final GLSL string, so every custom shape becomes part of sceneSdf() without a static branch
	// per shape type in this source file.
	// Custom shapes
#include "custom_shapes"

#ifdef COMBINE_2D_AND_3D_SDFS
	{
		float boxDist = fBox(p - vec3(0.0, 0.0, 100.5), vec3(1000.0, 1000.0, 100.0));
		minDist = -fOpUnionSoft(boxDist, -minDist, 0.5);
		// minDist = max(minDist, -boxDist);
	}

	return vec3(minDist, max(float(finalMaterialId), 0.0), globalBlend);
#endif

	// These are just spheres
	for (int i = 0; i < u_loadedObjects - (2 * u_customShapeCount); i++)
	{
		vec4 positionSizeMaterial = texelFetch(t_shapeBuffer, ivec2(i, 0), 0);
		int materialId = int(positionSizeMaterial.w);
		float objectDist = fSphere(p - positionSizeMaterial.xyz, 0.5);

		// Apply the dither function!
		objectDist = modifyDistanceBasedOnMaterial(objectDist, materialId, i);

		finalMaterialId = objectDist <= minDist ? materialId : finalMaterialId;
		
		vec2 res = fOpUnionSoft_blend(minDist, objectDist, DOT_BLEND_K);
		if (res.y > 0.0) {
			globalBlend = max(globalBlend, res.y);
		} else if (objectDist < minDist) {
			globalBlend = 0.0;
		}
		minDist = res.x;
	}

#ifdef MESH_SHADOW_SDF
	{
		float mDist = meshSDF(p);
		if (mDist < minDist) {
			minDist = mDist;
			finalMaterialId = 15;  // Use a designated "mesh shadow" material slot
		}
	}
#endif

	return vec3(minDist, max(float(finalMaterialId), 0.0), globalBlend);
}

vec3 getMaterial(vec3 p, int id)
{
	vec3 color = id < 16 ? u_materials[id].color.xyz : (0.83 * vec3(1.0));
	vec3 secondaryColor = id < 16 ? u_materials[id].secondaryColor.xyz : vec3(0.0);
	int pattern = id < 16 ? u_materials[id].pattern : 0;
	float patternScale = id < 16 ? u_materials[id].patternScale : 1.0;

	if(pattern > 0)
	{
		float patternShape = 0.0;
		vec3 sp = p * patternScale;

		if (pattern == 1)
		{
			patternShape = mod(floor(sp.x) + floor(sp.y) + floor(sp.z), 2.0);
		}
		else if(pattern == 2) // Perlin Noise
		{
			patternShape = perlin3D(sp.xyz * 10.0);
		}
		else if(pattern == 3) // Waves
		{
			patternShape = (sin(10.0  * perlin3D(sp.xyz * 10.0)) + 1.0) * 0.5;
			patternShape = smoothstep(0.3, 0.7, patternShape);

		}

		return mix(color, secondaryColor, patternShape);
	}

	

	return color;
}

vec3 getColor(vec3 p)
{
	vec3 scene = sceneSdf(p);
	return getMaterial(p, int(scene.y));
}

float rayMarch(vec3 ro, vec3 rd)
{
	float hit;

	float traveled = u_near;

	for (int i = 0; i < MAX_STEPS; i++)
	{
		vec3 p = ro + (traveled * rd);

		hit = sceneSdf(p).x;

		if (abs(hit) < RAYMARCH_EPSILON || traveled > u_far)
			break;

		traveled += hit;
	}

	// traveled += sceneSdf(ro + rd * traveled).x; // Final SDF evaluation to reduce overshoot

	return traveled;
}

vec3 getNormal(vec3 p)
{
	vec2 e = vec2(NORMAL_EPSILON, 0.0);
	vec3 n = vec3(sceneSdf(p).x) - vec3(sceneSdf(p - e.xyy).x, sceneSdf(p - e.yxy).x, sceneSdf(p - e.yyx).x);
	return normalize(n);
}

vec3 getPointLight(vec3 p, vec3 rd, vec3 color)
{

	vec3 lightPos = u_lights[0].position;

	vec3 L = normalize(lightPos - p);
	vec3 N = getNormal(p);
	vec3 V = -rd;
	vec3 R = reflect(-L, N);

	vec3 specColor = vec3(0.5);
	vec3 specular = specColor * pow(clamp(dot(R, V), 0.0, 1.0), 100.0);

	vec3 diffuse = color * clamp(dot(L, N), 0.0, 1.0);

	vec3 ambient = color * 0.05;
	vec3 fresnel = 0.1 * color * pow(1.0 + dot(rd, N), 3.0);

	float d = rayMarch(p - rd * 0.02, normalize(lightPos));

	return (d > length(lightPos - p)) ? diffuse + ambient + specular + fresnel : ambient + fresnel;
}

// Random functions for path tracing
// Based on PCG integer hash
uint pcg(inout uint state)
{
	state = state * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float randomFloat(inout float seed)
{
	uint state = floatBitsToUint(seed);
	float result = float(pcg(state)) * (1.0 / 4294967296.0); // 2^-32
	seed = uintBitsToFloat(state);
	return result;
}

vec2 hash2(inout float seed)
{
	return vec2(randomFloat(seed), randomFloat(seed));
}

vec3 hash3(inout float seed)
{
	return vec3(randomFloat(seed), randomFloat(seed), randomFloat(seed));
}

vec3 cosineSampleHemisphere(vec3 N, inout float seed)
{
	vec2 xi = hash2(seed);
	float phi = 2.0 * 3.14159265359 * xi.x;
	float cosTheta = sqrt(xi.y);
	float sinTheta = sqrt(1.0 - xi.y);

	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	return tangent * H.x + bitangent * H.y + N * H.z;
}

vec3 getSkyColor(vec3 dir)
{
	// Atmospheric fade from horizon to zenith based on the direction's Y coordinate
	float t = max(dir.y, 0.0);
	vec3 zenithColor = vec3(0.05, 0.25, 0.7);
	vec3 horizonColor = vec3(0.5, 0.6, 0.7);
	vec3 groundColor = vec3(0.1, 0.1, 0.12);

	vec3 sky = mix(horizonColor, zenithColor, pow(t, 0.4));
	if (dir.y < 0.0)
	{
		// Fake ground bounce color below horizon
		sky = mix(horizonColor, groundColor, pow(-dir.y, 0.5));
	}

	// Procedural pseudo-Sun using the first directional light
	if (u_numLights > 0 && u_lights[0].type == 0)
	{
		float sun = max(dot(dir, normalize(u_lights[0].direction)), 0.0);
		sky += vec3(1.0, 0.85, 0.6) * pow(sun, 4000.0) * 2.0; // Sun core (much smaller, tight exponent)
		sky += vec3(1.0, 0.6, 0.2) * pow(sun, 200.0) * 0.4;	  // Sun outer glow (restrained exponent)

		sky *= u_lights[0].color.w > 0.0 ? 1.0 : 0.0; // Modulate by sun color and intensity
	}

	return sky;
}

// ─────────────────────────────────────────────────────────────────────────────
// Core path-tracing kernel shared by SDF and mesh surfaces.
//
// firstN  – surface normal at the primary hit point.
//   • SDF  surfaces: pass getNormal(p) before calling.
//   • Mesh surfaces: pass the GBuffer normal directly.
// All secondary-bounce normals are always derived from the SDF analytically.
// ─────────────────────────────────────────────────────────────────────────────
vec3 pathTrace(vec3 p, vec3 rd, vec3 initialColor, int materialId, vec3 firstN, inout float seed)
{
	vec3 throughput = vec3(1.0);

	// Energy Conservation: Base color > 1.0 acts as light emission
	vec3 finalColor = max(initialColor - vec3(1.0), vec3(0.0));
	vec3 currentAlbedo = min(initialColor, vec3(1.0)); // Albedo reflects 100% light max

	vec3 currentRd = rd;

	// === Render Style Parameters ===================================
	// roughness and f0 are driven by the material palette; other params remain constant
	int currentMatId = clamp(materialId, 0, 15);
	float roughness = u_materials[currentMatId].roughness;
	float f0 = u_materials[currentMatId].metallic;

	// fresnelPower: exponent for viewing angle reflection falloff
	float fresnelPower = 5.0; // Retro: 2.0,       Realistic: 5.0

	// specExponent: tightness of the direct specular highlight
	float specExponent = 2000.0; // Retro: 400.0,     Realistic: 100.0

	// specMultiplier: intensity/scaling of the direct specular lobe
	// Retro just blows it out for that classic '90s CG highlight.
	vec3 specMultiplier = vec3(5.0f);
	// ===============================================================

	
	// Perform bounces


	for (int bounce = 0; bounce <= u_rayBounces; bounce++)
	{
		float currentF0 = f0;
		float currentRoughness = u_rayBounces == 1 ? max(roughness, 0.2) : min(roughness + (float(bounce) * 0.1 / float(u_rayBounces)), 1.0); // Add roughness with each bounce to prevent infinite mirror-like reflections
		fresnelPower = mix(50.0, 1.0, currentF0);

		// On the first bounce use the injected normal (supports both SDF and mesh
		// primary hits). Subsequent bounces always use the SDF analytical gradient.
		vec3 N = (bounce == 0) ? firstN : getNormal(p);

		float viewAngle = clamp(dot(-currentRd, N), 0.0, 1.0);
		float fresnel = currentF0 + ((1.0 - currentF0) * pow(1.0 - viewAngle, fresnelPower));

		// HACK: Reduce high-frequency noise at extreme grazing angles (like distant horizon)
		// 1. Cap the maximum Fresnel reflection to avoid perfect mirror-like sampling
		fresnel = min(fresnel, max(currentF0, 0.85));
		// 2. Artificially boost roughness to blur the heavily scattered rays at grazing angles
		float grazingFactor = pow(1.0 - viewAngle, 5.0);
		currentRoughness = mix(currentRoughness, 1.0, grazingFactor * 0.5);

		if (bounce == u_rayBounces && bounce > 0)
		{
			fresnel = 0.0;
		}

		// Choose ray type probability based on Fresnel
		float randVal = hash2(seed).x;
		bool isSpecular = randVal < fresnel;

		// 1. Accumulate Direct Lighting from all active lights
		vec3 directLighting = vec3(0.0);

		for (int i = 0; i < u_numLights; i++)
		{
			Light light = u_lights[i];

			vec3 L;
			float lightDist;
			float attenuation = 1.0;

			if (light.type == 0)
			{
				// Directional
				L = normalize(light.direction + (vec3(hash2(seed), hash2(seed).x) * 2.0 - 1.0) * 0.05);
				lightDist = u_far * 0.5;
			}
			else if (light.type == 1)
			{
				// Point
				vec3 lightVec = light.position - p;
				lightDist = length(lightVec);
				vec3 jitter = (vec3(hash2(seed), hash2(seed).x) * 2.0 - 1.0) * 0.25;
				L = normalize(lightVec + jitter);

				float a = 3.0;
				float b = 0.7;
				attenuation = 10.0 / (a * lightDist * lightDist + b * lightDist + 1.0);
			}
			else
			{
				// Cone
				vec3 lightVec = light.position - p;
				lightDist = length(lightVec);
				vec3 jitter = (vec3(hash2(seed), hash2(seed).x) * 2.0 - 1.0) * 0.15;
				L = normalize(lightVec + jitter);

				float a = 3.0;
				float b = 0.7;
				attenuation = 10.0 / (a * lightDist * lightDist + b * lightDist + 1.0);

				// Cone falloff (spot effect)
				vec3 surfaceToLight = normalize(light.position - p);
				float spotEffect = dot(surfaceToLight, normalize(light.direction));

				float innerCone = 0.95;
				float outerCone = 0.80;

				attenuation *= smoothstep(outerCone, innerCone, spotEffect);
			}

			// Direct shadow ray for this specific light
			float dLight = rayMarch(p + N * 0.01, L);

			// Check if we hit nothing before reaching the light (or if the ray just ran out of steps grazing the
			// surface)
			bool hitObstacle = (dLight < lightDist) && (sceneSdf(p + N * 0.01 + dLight * L).x < RAYMARCH_EPSILON * 5.0);
			if (!hitObstacle)
			{
				vec3 diffuse = currentAlbedo * max(dot(N, L), 0.0) * (1.0 - fresnel);

				vec3 H = normalize(L - currentRd);
				float specPow = pow(max(dot(N, H), 0.0), specExponent);
				vec3 specular = specMultiplier * specPow *
								fresnel; // Note: for realistic, multiply this by 'fresnel' for energy conservation

				directLighting += (diffuse + specular) * attenuation * light.color.xyz * light.color.w;
			}
		}

		// Add all direct light contributions for this bounce
		finalColor += throughput * directLighting;

		if (bounce == u_rayBounces) 
		{

			// If this final hit is in a shadow, it will still evaluate to black.
			// Add a little bit of sky ambient so deep shadows always have some color.
			if (bounce > 0) 
			{
				finalColor += throughput * currentAlbedo * getSkyColor(N) * 0.15;
			}
			// ==========================================
			break;
		}

		// 2. Indirect bounce (Calculated once per bounce path)
		vec3 diffuseDir = cosineSampleHemisphere(N, seed);

		vec3 reflectDir = reflect(currentRd, N);
		vec3 jitter = normalize(vec3(hash2(seed), hash2(seed).x) * 2.0 - 1.0);
		vec3 specularDir = normalize(reflectDir + jitter * currentRoughness);
		if (dot(specularDir, N) < 0.0)
			specularDir = reflectDir; // prevent pointing inside

		vec3 bounceDir = isSpecular ? specularDir : diffuseDir;
		currentRd = bounceDir;

		float d = rayMarch(p + N * 0.01, bounceDir);

		// modulate throughput based on branch
		if (isSpecular)
			throughput *= mix(vec3(1.0), currentAlbedo, currentF0); // metallic tint
		else
			throughput *= currentAlbedo;

		bool bounceHitObstacle = (d < u_far) && (sceneSdf(p + N * 0.01 + d * bounceDir).x < RAYMARCH_EPSILON * 5.0);

		if (!bounceHitObstacle)
		{
			// Hit sky (ambient bounding) or ran out of steps grazing a surface
			// Path Tracing natively evaluates the bounceDir hemisphere into the procedural sky dome!
			
			// Keep sky at full brightness for reflections, but dim it for diffuse ambient lighting
			float skyIntensity = isSpecular ? 1.0 : 0.5; 
			finalColor += throughput * getSkyColor(bounceDir) * skyIntensity;
			break;
		}
		else
		{
			p = p + N * 0.01 + d * bounceDir;
			// Jitter the material sampling point on bounces too, so
			// smooth-union color blending works for indirect lighting.
			vec3 preBounceScene = sceneSdf(p);
			float bounceBlend = preBounceScene.z;
			float bounceJitterMask = smoothstep(0.0, 0.5, bounceBlend);

			vec3 bounceMaterialPos = p + (hash3(seed) - 0.5) * 0.04 * max(d, 1.0) * bounceJitterMask;
			vec3 bounceScene = sceneSdf(bounceMaterialPos);
			int bounceMatId = clamp(int(bounceScene.y), 0, 15);
			vec3 bounceMaterial = getMaterial(bounceMaterialPos, bounceMatId);

			// Update material properties for the next bounce
			roughness = u_materials[bounceMatId].roughness;
			f0 = u_materials[bounceMatId].metallic;

			vec3 emission = max(bounceMaterial - vec3(1.0), vec3(0.0));
			currentAlbedo = min(bounceMaterial, vec3(1.0));

			finalColor += throughput * emission; // Emit light from the glowing object!
		}
	}

	return finalColor;
}

// Standard forward lighting combining all light types and sky reflections
vec3 standardLighting(vec3 p, vec3 rd, vec3 albedo, int materialId, vec3 N)
{
	vec3 V = -rd;

	// === Render Style Parameters ===================================
	int matId = clamp(materialId, 0, 15);
	float f0 = u_materials[matId].metallic;
	float roughness = u_materials[matId].roughness;
	float fresnelPower = 10.0;
	float specExponent = 100.0;
	vec3 specMultiplier = vec3(5.0);
	// ===============================================================

	float fresnel = f0 + (1.0 - f0) * pow(clamp(1.0 - dot(V, N), 0.0, 1.0), fresnelPower);

	vec3 directLighting = vec3(0.0);

	for (int i = 0; i < u_numLights; i++)
	{
		Light light = u_lights[i];

		vec3 L;
		float lightDist;
		float attenuation = 1.0;

		if (light.type == 0)
		{
			// Directional
			L = normalize(light.direction);
			lightDist = u_far * 0.5;
		}
		else if (light.type == 1)
		{
			// Point
			vec3 lightVec = light.position - p;
			lightDist = length(lightVec);
			L = normalize(lightVec);

			float a = 3.0;
			float b = 0.7;
			attenuation = 10.0 / (a * lightDist * lightDist + b * lightDist + 1.0);
		}
		else
		{
			// Cone
			vec3 lightVec = light.position - p;
			lightDist = length(lightVec);
			L = normalize(lightVec);

			float a = 3.0;
			float b = 0.7;
			attenuation = 10.0 / (a * lightDist * lightDist + b * lightDist + 1.0);

			// Cone falloff (spot effect)
			vec3 surfaceToLight = normalize(light.position - p);
			float spotEffect = dot(surfaceToLight, normalize(light.direction));

			float innerCone = 0.95;
			float outerCone = 0.80;

			attenuation *= smoothstep(outerCone, innerCone, spotEffect);
		}

		// Hard Shadows
		float dLight = rayMarch(p + N * 0.01, L);

		// Check if we hit nothing before reaching the light (or ray exhausted steps)
		bool hitObstacle =
			(dLight < lightDist * 0.95) && (sceneSdf(p + N * 0.01 + dLight * L).x < RAYMARCH_EPSILON * 5.0);
		if (!hitObstacle)
		{
			vec3 diffuse = albedo * max(dot(N, L), 0.0) * (1.0 - fresnel);

			vec3 H = normalize(L + V);
			float specPow = pow(max(dot(N, H), 0.0), specExponent);
			vec3 specular = specMultiplier * specPow * fresnel;

			directLighting += (diffuse + specular) * attenuation * light.color.xyz * light.color.w;
		}
	}

	// Ambient and Sky Reflections
	vec3 R = reflect(rd, N);
	vec3 reflectionColor = getSkyColor(R);
	vec3 ambient = albedo * 0.1 + reflectionColor * fresnel;

	return directLighting + ambient;
}



// Returns the linearised (eye-space) depth from a [0,1] depth-buffer value.
float linearizeGBufferDepth(float d)
{
	float z_n = d * 2.0 - 1.0;
	return (2.0 * u_near * u_far) / (u_far + u_near - z_n * (u_far - u_near));
}

vec4 render(in vec2 uv)
{
	// Ray origin
	vec3 ro = -u_camMatrix[3].xyz;
	// Apply camera rotation
	ro = vec3(dot(u_camMatrix[0].xyz, ro), dot(u_camMatrix[1].xyz, ro), dot(u_camMatrix[2].xyz, ro));

	// Ray direction
	vec3 rd = (vec4(normalize(vec3(uv, -u_fov)), 0.0) * u_camMatrix).xyz;

	// Fish eye
#ifdef FISH_EYE
	float z = pow(1.0 - (uv.x * uv.x) - (uv.y * uv.y), 0.5);
	rd = vec3(uv, -z);
	rd = (vec4(rd, 0) * u_camMatrix).xyz;
#endif

	float meshDepthSample = texture(t_depthTexture, v_texCoord).r;  // [0,1], 1.0 = no mesh
	float meshDepthLinear = linearizeGBufferDepth(meshDepthSample);  // eye-space metres

	// Ray march SDF
	float object = rayMarch(ro, rd);

	// --- THE FIX ---
	// Convert Euclidean ray distance to Planar View-Z distance!
	vec3 rdCam = normalize(vec3(uv, -u_fov));
	float planarDepth = object * abs(rdCam.z);

	// Use planar depth to properly compare against the G-Buffer
	float minDepth = planarDepth;

	// Compute the SDF hit depth in the same NDC [0,1] space so gl_FragDepth is consistent
	float z_e = planarDepth;
	float z_n = (u_far + u_near - (2.0 * u_near * u_far) / z_e) / (u_far - u_near);
	float sdfDepthNDC = (z_n + 1.0) * 0.5;

	// Determine whether a valid mesh pixel is closer than the SDF hit
	bool meshValid  = (meshDepthSample < 1.0 - 0.0001);
	bool meshInFront = meshValid && (meshDepthLinear < minDepth);

	// Output color
	vec3 col = getSkyColor(rd);

	// alpha: 1.0 = SDF/sky path (accumulate), 0.0 = mesh path (skip accumulation)
	float accumAlpha = 1.0;

	uint state = floatBitsToUint(uv.x) ^ floatBitsToUint(uv.y) ^ uint(u_frameCounter);
	pcg(state); // Warm up the state
	float seed = uintBitsToFloat(state);

	if (meshInFront)
	{
		// -------------------------------------------------------
		// GBuffer depth comparison
		// -------------------------------------------------------
		// Convert the aspect-corrected (possibly AA-jittered) uv back to [0,1] texcoord.
		// When AA is active this shifts the GBuffer lookup by the same sub-pixel jitter,
		// so mesh shading varies per frame and accumulates correctly in the temporal denoiser.
		// Note: this can produce edge artifacts when the jitter pushes the lookup outside
		// the mesh silhouette — that is an accepted trade-off.
		float _ar = u_resolution.x / u_resolution.y;
		vec2 gbufferUV = vec2(uv.x / _ar, uv.y) * 0.5 + 0.5;

		// --- Mesh surface is in front: apply SDF-aware deferred lighting ---
		vec3  meshAlbedo   = texture(t_gbufferAlbedo,   gbufferUV).rgb;
		vec3  meshWorldPos = texture(t_gbufferWorldPos, gbufferUV).rgb;
		// float meshSpecVal  = texture(t_gbufferAlbedo,   gbufferUV).a;
		vec3  meshNormal   = normalize(texture(t_gbufferNormal, gbufferUV).rgb);

		int materialId = texture(t_gbufferMaterial, gbufferUV).r;
		meshAlbedo *= getMaterial(meshWorldPos, materialId); // Apply material palette color to mesh albedo

#ifdef PATH_TRACING
		col = pathTrace(meshWorldPos, rd, meshAlbedo, materialId, meshNormal, seed);
#else
		col = standardLighting(meshWorldPos, rd, meshAlbedo, materialId, meshNormal);
#endif
		

		// Fog at mesh depth
		float fog = smoothstep(u_far * 0.0, u_far, meshDepthLinear);
		col = mix(col, getSkyColor(rd), fog);

		// Write the mesh depth to the depth buffer
		gl_FragDepth = meshDepthSample;
	}
	else if (minDepth < u_far)
	{
		// --- SDF hit is in front ---
		vec3 p = ro + object * rd;

#ifdef PATH_TRACING
		// Jitter the material sampling point with high-frequency 3D noise.
		// Temporal accumulation will blend material colors at smooth-union
		// boundaries into smooth gradients.
		// Scale jitter by distance so blending stays visible at any range
		vec3 preScene = sceneSdf(p);
		float blendFactor = preScene.z;
		float jitterMask = smoothstep(0.0, 0.5, blendFactor);

		vec3 materialSamplePos = p + (hash3(seed) - 0.5) * 0.01 * max(object, 5.0) * jitterMask;
#else
		vec3 materialSamplePos = p;
#endif

		vec3 sceneResult = sceneSdf(materialSamplePos);
		int materialId = int(sceneResult.y);
		vec3 baseColor = getMaterial(materialSamplePos, materialId);

#ifdef PATH_TRACING
		col = pathTrace(p, rd, baseColor, materialId, getNormal(p), seed);
#else
		col = standardLighting(p, rd, baseColor, materialId, getNormal(p));
#endif

		float fog = smoothstep(u_far * 0.0, u_far, minDepth);
		col = mix(col, getSkyColor(rd), fog);

		gl_FragDepth = sdfDepthNDC;
	}
	else
	{
		// Sky
		gl_FragDepth = sdfDepthNDC;
	}

	return vec4(col, 1.0); // accumAlpha);
}

float rand(vec2 co)
{
	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
	vec2 screenUV = v_texCoord;

	vec2 uv = (2.0 * v_texCoord) - 1.0;
	float aspectRatio = u_resolution.x / u_resolution.y; // TODO: uniform

	// Temporal jitter for anti-aliasing
#ifdef PATH_TRACING
#ifdef ANTIALIASING
	if (u_frameCounter > 0)
	{
		uv += (vec2(rand(uv + u_time), rand(uv + u_time * 1.5)) - 0.5) * 2.0 / u_resolution;
	}
#endif
#endif

	uv.x *= aspectRatio;

	// Because AA jitter is baked into the original UVs being sent to render(),
	// we just evaluate the render directly to extract the jittered target color.
	vec4 data = render(uv);
	vec3 col = data.xyz;

	// Firefly suppression: clamp per-sample luminance before accumulation.
	// Fireflies are caused by rare specular paths hitting a bright light,
	// producing extreme values that take hundreds of frames to average out.
	// Clamping the max component kills them at the source with minimal bias.
	float maxComp = max(col.r, max(col.g, col.b));
	if (maxComp > 3.0)
	{
		col *= 3.0 / maxComp;
	}
	
	// Apply contrast pivoting around mid-gray
	col = mix(vec3(0.5), col, u_contrast);
	// Clamp to ensure no weird artifacts from exceeding bounds
	col = clamp(col, 0.0, 1.0);

#ifdef PATH_TRACING
	if (u_frameCounter > 0)
	{
		vec4 prevColor = texture(t_previousColor, screenUV);
		
		// Temporal Denoiser: Use an Exponential Moving Average (EMA) cap 
		// to prevent the weight from becoming too small, allowing the image to 
		// continually refine and denoise even after many frames.
		float weight = max(1.0 / float(u_frameCounter + 1), 0.02);
		col = mix(prevColor.xyz, col, weight);
	}
#endif

	FragColor = vec4(col, 1.0); // output linear color for accumulation
}
