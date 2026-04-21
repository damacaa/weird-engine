#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

#include "../common/shapes.glsl"

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

uniform sampler2D t_colorTexture;
uniform sampler2D t_depthTexture;
uniform highp sampler2D t_shapeBuffer;
uniform int u_loadedObjects;
uniform int u_customShapeCount;

uniform mat4 u_camMatrix;
uniform float u_fov;
uniform vec2 u_resolution;
uniform vec4 u_staticColors[16];

uniform vec3 u_lightPos;
uniform vec3 u_lightDirection;
uniform vec4 u_lightColor;

uniform float u_time;

const int MAX_STEPS = 256;
const float RAYMARCH_EPSILON = 0.001;
const float NORMAL_EPSILON = 0.00001;
const float NEAR = 0.1;
const float FAR = 100.0;

const float OVERSHOOT = 1.0;
const float DOT_BLEND_K = 0.5;

const vec3 background = vec3(0.0);

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
const int bayer4x4[16] = int[16](
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
);

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
	return vec2(
		fract(sin(id * 12.9898) * 43758.5453) * 100.0,
		fract(sin(id * 78.233) * 43758.5453) * 100.0
	);
}

// The reusable Transparency Dither Function
float modifyDistanceBasedOnMaterial(float dist, int materialId, int objectId)
{
	float alpha = materialId < 16 ? u_staticColors[materialId].a : 1.0;
	
	if (alpha < 1.0) 
	{
		vec2 offset = HashID(float(objectId));
		
		// Note: Change 1.0 to 2.0 or 4.0 here if you want chunkier retro dither pixels
		float bayer = GetBayerThreshold((gl_FragCoord.xy + offset) / 1.0); 
		
		if (alpha <= bayer) 
		{
			return FAR + 100.0; // Skip surface by pushing distance past the FAR plane
		}
	}
	return dist;
}
// ==========================================

vec2 sceneSdf(vec3 p)
{
	float minDist = FAR;
	int finalMaterialId = 16;

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

	float traveled = NEAR;

	for (int i = 0; i < MAX_STEPS; i++)
	{
		vec3 p = ro + (traveled * rd);

		hit = sceneSdf(p).x;

		if (abs(hit) < RAYMARCH_EPSILON || traveled > FAR)
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

vec3 getLight(vec3 p, vec3 rd, vec3 color)
{

	vec3 lightPos = u_lightPos;

	vec3 L = normalize(lightPos - p);
	vec3 N = getNormal(p);
	vec3 V = -rd;
	vec3 R = reflect(-L, N);

	// color = vec3(0.0);

	vec3 specColor = vec3(0.5);
	vec3 specular = specColor * pow(clamp(dot(R, V), 0.0, 1.0), 100.0);

	vec3 diffuse = color * clamp(dot(L, N), 0.0, 1.0);

	vec3 ambient = color * 0.05;
	vec3 fresnel = 0.1 * color * pow(1.0 + dot(rd, N), 3.0);

	float d = rayMarch(p - rd * 0.02, normalize(lightPos));

	return (d > length(lightPos - p)) ? diffuse + ambient + specular + fresnel : ambient + fresnel;
}

vec3 getDirectionalLight(vec3 p, vec3 rd, vec3 color)
{
	vec3 L = u_lightDirection;
	vec3 N = getNormal(p);
	vec3 V = -rd;
	vec3 R = reflect(-L, N);

	// color = vec3(0.0);

	vec3 specColor = vec3(1.0);
	vec3 specular = specColor * pow(clamp(dot(R, V), 0.0, 1.0), 100.0);

	vec3 diffuse = color * clamp(dot(L, N), 0.0, 1.0);

	vec3 ambient = color * 0.05;
	vec3 fresnel = 0.1 * color * pow(1.0 + dot(rd, N), 3.0);

	float d = rayMarch(p - rd * 0.02, L);

	return (d >= FAR * length(L)) ? diffuse + ambient + specular + fresnel : ambient + fresnel;
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
	// float z = pow(1.0 - (uv.x * uv.x) - (uv.y * uv.y), 0.5);
	// vec3 rd = vec3(uv, -z);
	// rd = (vec4(rd, 0) * u_camMatrix).xyz;

	// Ray march to find closest SDF
	float object = rayMarch(ro, rd);

	// If ray marched distance is bigger than depth from zbuffer, set alpha to 1
	float minDepth = object;

	float z_e = object;
	float z_n = (FAR + NEAR - (2.0 * NEAR * FAR) / z_e) / (FAR - NEAR);
	float depth = (z_n + 1.0) * 0.5;

	gl_FragDepth = depth;

	//    if(handleTransparency)
	//        return vec4(1,0,0,1);

	// Output color
	vec3 col;

	// Scene alpha over background
	float alpha = 0.0;
	if (minDepth < FAR)
	{

		vec3 p = ro + object * rd;
		vec3 material = getColor(p);
		col = getDirectionalLight(p, rd, material);

		// fog

		// minDepth -= 0.85;
		// alpha = exp(-0.0004 * minDepth * minDepth);

		// col = mix(col, background, alpha);

		// float a = minDepth - (FAR - 980);
		// alpha = max(0.0, 0.001 * a * a *a );

		// alpha = 1.0;

		alpha = 1.0 - smoothstep(FAR * 0.5, FAR, minDepth);
	}

	return vec4(col, alpha);

	// col = mix(col, background, alpha);

	// return col;
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
	uv.x *= aspectRatio;

	// Calculate true z value from the depth buffer:
	// https://stackoverflow.com/questions/6652253/getting-the-true-z-value-from-the-depth-buffer float depth =
	// texture(t_depthTexture, screenUV).r; float z_n = 2.0 * depth - 1.0; float z_e = 2.0 * NEAR * FAR / (FAR + NEAR -
	// z_n * (FAR - NEAR));

	// vec4 originalColor = texture(t_colorTexture, screenUV);

	vec4 data = render(uv);
	float fog = data.a;

	vec3 col = mix(background, data.xyz, fog);

	col = pow(col.xyz, vec3(0.4545));

	FragColor = vec4(col.xyz, 1.0);
}

