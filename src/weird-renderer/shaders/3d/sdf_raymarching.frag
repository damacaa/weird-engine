#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

#include "../common/shapes.glsl"

#define PATH_TRACING
// #define FISH_EYE

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
uniform sampler2D t_depthTexture;
uniform highp sampler2D t_shapeBuffer;
uniform int u_loadedObjects;
uniform int u_customShapeCount;
uniform int u_frameCounter;

uniform mat4 u_camMatrix;
uniform float u_fov;
uniform vec2 u_resolution;
uniform vec4 u_staticColors[16];

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
const float NORMAL_EPSILON = 0.00001;
uniform float u_near;
uniform float u_far;

const float OVERSHOOT = 1.0;
const float DOT_BLEND_K = 0.2;

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
	float alpha = materialId < 16 ? u_staticColors[materialId].a : 1.0;

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

vec2 sceneSdf(vec3 p)
{
	float minDist = u_far;
	int finalMaterialId = 16;

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

	// These are just spheres
	for (int i = 0; i < u_loadedObjects - (2 * u_customShapeCount); i++)
	{
		vec4 positionSizeMaterial = texelFetch(t_shapeBuffer, ivec2(i, 0), 0);
		int materialId = int(positionSizeMaterial.w);
		float objectDist = fSphere(p - positionSizeMaterial.xyz, 0.5);

		// Apply the dither function!
		objectDist = modifyDistanceBasedOnMaterial(objectDist, materialId, i);

		finalMaterialId = objectDist <= minDist ? materialId : finalMaterialId;
		minDist = fOpUnionSoft(objectDist, minDist, DOT_BLEND_K);
	}

	return vec2(minDist, max(float(finalMaterialId), 0.0));
}

vec3 getMaterial(vec3 p, int id)
{
	vec3 color = id < 16 ? u_staticColors[id].xyz : (0.83 * vec3(1.0));
	if (id == 0)
	{
		float checker = 0.2 + 0.4 * mod(floor(p.x) + floor(p.z), 2.0);
		return color * (0.8 + checker);
	}

	return color;
}

vec3 getColor(vec3 p)
{
	vec2 scene = sceneSdf(p);
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
// Based on typical hash
vec2 hash2(inout float seed)
{
	seed += 1.0;
	return fract(sin(vec2(seed, seed + 1.0) * vec2(12.9898, 78.233)) * 43758.5453);
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
	}

	return sky;
}

// Common Path Tracing Function running multiple lights!
vec3 pathTrace(vec3 p, vec3 rd, vec3 initialColor, inout float seed)
{
	vec3 throughput = vec3(1.0);

	// Energy Conservation: Base color > 1.0 acts as light emission
	vec3 finalColor = max(initialColor - vec3(1.0), vec3(0.0));
	vec3 currentAlbedo = min(initialColor, vec3(1.0)); // Albedo reflects 100% light max

	vec3 currentRd = rd;

	// === Render Style Parameters ===================================
	// roughness: lower = sharper mirror-like reflections
	float roughness = 0.1f; // Retro: 0.02,      Realistic: 0.15

	// f0: base reflectivity looking straight on
	float f0 = 0.3f; // Retro: 0.45,      Realistic: 0.04

	// fresnelPower: exponent for viewing angle reflection falloff
	float fresnelPower = 3.0f; // Retro: 2.0,       Realistic: 5.0

	// specExponent: tightness of the direct specular highlight
	float specExponent = 2000.0f; // Retro: 400.0,     Realistic: 100.0

	// specMultiplier: intensity/scaling of the direct specular lobe
	// Retro just blows it out for that classic '90s CG highlight.
	vec3 specMultiplier = vec3(5.0f);
	// ===============================================================

	
	// Perform bounces
	int bounceCount = 3;

	for (int bounce = 0; bounce < bounceCount; bounce++)
	{
		float currentF0 = f0;
		float currentRoughness = roughness;

		vec3 N = getNormal(p);

		float fresnel = currentF0 + ((1.0 - currentF0) * pow(clamp(1.0 - dot(-currentRd, N), 0.0, 1.0), fresnelPower));

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
		{
			// Specular bounce takes the Fresnel energy, but we factored that into the probability
			// so just multiply by the tint
			throughput *= vec3(1.0);
		}
		else
		{
			throughput *= currentAlbedo;
		}

		bool bounceHitObstacle = (d < u_far) && (sceneSdf(p + N * 0.01 + d * bounceDir).x < RAYMARCH_EPSILON * 5.0);

		if (!bounceHitObstacle)
		{
			// Hit sky (ambient bounding) or ran out of steps grazing a surface
			// Path Tracing natively evaluates the bounceDir hemisphere into the procedural sky dome!
			finalColor += throughput * getSkyColor(bounceDir);
			break;
		}
		else
		{
			p = p + N * 0.01 + d * bounceDir;
			vec3 bounceMaterial = getColor(p);

			vec3 emission = max(bounceMaterial - vec3(1.0), vec3(0.0));
			currentAlbedo = min(bounceMaterial, vec3(1.0));

			finalColor += throughput * emission; // Emit light from the glowing object!

			// Prevent black pixels on maximum bounce depth
			if (bounce == bounceCount - 1)
			{
				vec3 nextN = getNormal(p);
				// Cheap ambient sky reflection for the very last hit
				finalColor += throughput * getSkyColor(reflect(bounceDir, nextN)) * (1.0 - currentRoughness) + finalColor;
			}
		}
	}

	return finalColor;
}

// Standard forward lighting combining all light types and sky reflections
vec3 standardLighting(vec3 p, vec3 rd, vec3 albedo)
{
	vec3 N = getNormal(p);
	vec3 V = -rd;

	// === Render Style Parameters ===================================
	float f0 = 0.04;
	float fresnelPower = 5.0;
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
	vec3 ambient = albedo * 0.05 + reflectionColor * fresnel;

	return directLighting + ambient;
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

	// Ray march to find closest SDF
	float object = rayMarch(ro, rd);

	// If ray marched distance is bigger than depth from zbuffer, set alpha to 1
	float minDepth = object;

	float z_e = object;
	float z_n = (u_far + u_near - (2.0 * u_near * u_far) / z_e) / (u_far - u_near); // Convert to non-linear depth
	float depth = (z_n + 1.0) * 0.5;

	gl_FragDepth = depth;

	// Output color (initialized to sky passing the primary ray direction)
	vec3 col = getSkyColor(rd);

	float seed = uv.x * 1234.5 + uv.y * 6789.0 + float(u_frameCounter) * 111.0;

	// Scene alpha over background is abandoned, evaluate objects fully opaque against sky
	if (minDepth < u_far)
	{
		vec3 p = ro + object * rd;
		vec3 material = getColor(p);

#ifdef PATH_TRACING
		col = pathTrace(p, rd, material, seed);
#else
		col = standardLighting(p, rd, material);
#endif

		// Apply distance fog to smoothly blend the object into the sky using the view ray (rd) color
		float fog = smoothstep(u_far * 0.0, u_far, minDepth);
		col = mix(col, getSkyColor(rd), fog);
	}

	// Always return 1.0 alpha now
	return vec4(col, 1.0);
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
	if (u_frameCounter > 0)
	{
		uv += (vec2(rand(uv + u_time), rand(uv + u_time * 1.5)) - 0.5) * 2.0 / u_resolution;
	}
#endif

	uv.x *= aspectRatio;

	// Because AA jitter is baked into the original UVs being sent to render(),
	// we just evaluate the render directly to extract the jittered target color.
	vec4 data = render(uv);
	vec3 col = data.xyz;

	// === Render Style Parameters (Post-Processing) =================
	// contrast: artificially boosts contrast and crushes colors for that raw 90s CG render look
	float contrast = 1.2; // Retro: 1.35, Realistic: 1.0
	// ===============================================================
	
	// Apply contrast pivoting around mid-gray
	col = mix(vec3(0.5), col, contrast);
	// Clamp to ensure no weird artifacts from exceeding bounds
	col = clamp(col, 0.0, 1.0);

#ifdef PATH_TRACING
	if (u_frameCounter > 0)
	{
		vec4 prevColor = texture(t_previousColor, screenUV);
		prevColor.xyz = pow(prevColor.xyz, vec3(2.2)); // to linear
		col = mix(prevColor.xyz, col, 1.0 / float(u_frameCounter + 1));
	}
#endif

	FragColor = vec4(pow(col, vec3(0.4545)), 1.0); // to sRGB
												   // FragColor = vec4(1.0); // all white
}
