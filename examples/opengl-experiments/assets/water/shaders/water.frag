#version 300 es
precision highp float;
precision highp int;

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform vec3  u_camPos;
uniform float u_time;

// Screen-space scene snapshot (taken before this draw call)
uniform sampler2D u_sceneColor;
uniform sampler2D u_sceneDepth;
uniform vec2      u_screenSize;

// Light array – same layout as sdf_raymarching.frag
struct Light
{
	vec3 position;
	vec3 direction;
	vec4 color;     // .rgb = tint, .a = intensity
	int  type;      // 0 = directional, 1 = point, 2 = spot/cone
};
#define MAX_LIGHTS 8
uniform int   u_numLights;
uniform Light u_lights[MAX_LIGHTS];

// Water material
const vec3  u_shallowColor    = vec3(0.05, 0.35, 0.55);
const vec3  u_deepColor       = vec3(0.01, 0.10, 0.25);
const float u_metallic        = 0.02;   // F0 for water (Fresnel base ~2%)
const float u_fresnelPower    = 5.0;
const float u_specExponent    = 512.0;
const float u_absorptionRate  = 2.0;    // light absorbed per world-unit of depth
const float u_foamBand        = 0.25;   // max foam band width (world units)

// ── Perlin noise ─────────────────────────────────────────────────────────────

vec2 _hash22(vec2 p)
{
	p = vec2(dot(p, vec2(127.1, 311.7)),
	         dot(p, vec2(269.5, 183.3)));
	return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float _perlin(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f); // Hermite smoothstep
	return mix(
		mix(dot(_hash22(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0)),
		    dot(_hash22(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
		mix(dot(_hash22(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
		    dot(_hash22(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y);
}

// Fractal Brownian Motion – returns [0, 1]
float fbm(vec2 p)
{
	float v = 0.0, a = 0.5, s = 1.0;
	for (int i = 0; i < 4; i++) { v += a * _perlin(p * s); s *= 2.1; a *= 0.5; }
	return v * 0.5 + 0.5;
}

// ── Depth helpers ────────────────────────────────────────────────────────────
// Match SDF3DRenderPipeline::NEAR_PLANE / FAR_PLANE
uniform float u_near;
uniform float u_far;

// Convert a non-linear depth buffer value [0,1] to a linear view-space distance.
float linearizeDepth(float d)
{
	float ndcZ = d * 2.0 - 1.0;
	return (2.0 * u_near * u_far) / (u_far + u_near - ndcZ * (u_far - u_near));
}

// ── Sky colour (matches getSkyColor in sdf_raymarching.frag) ──────────────────

vec3 getSkyColor(vec3 dir)
{
	float t           = max(dir.y, 0.0);
	vec3  zenithColor  = vec3(0.05, 0.25, 0.7);
	vec3  horizonColor = vec3(0.5,  0.6,  0.7);
	vec3  groundColor  = vec3(0.1,  0.1,  0.12);

	vec3 sky = mix(horizonColor, zenithColor, pow(t, 0.4));
	if (dir.y < 0.0)
		sky = mix(horizonColor, groundColor, pow(-dir.y, 0.5));

	for (int i = 0; i < u_numLights; i++)
	{
		if (u_lights[i].type == 0)
		{
			float sun = max(dot(dir, normalize(u_lights[i].direction)), 0.0);
			sky += vec3(1.0, 0.85, 0.6) * pow(sun, 4000.0) * 2.0;
			sky += vec3(1.0, 0.6,  0.2) * pow(sun,  200.0) * 0.4;
			sky *= u_lights[i].color.w > 0.0 ? 1.0 : 0.0;
		}
	}
	return sky;
}

// ── Water surface lighting ────────────────────────────────────────────────────

vec3 surfaceLighting(vec3 p, vec3 rd, vec3 albedo, vec3 N)
{
	vec3  V       = -rd;
	float f0      = u_metallic;
	float fresnel = f0 + (1.0 - f0) * pow(clamp(1.0 - dot(V, N), 0.0, 1.0), u_fresnelPower);

	vec3 directLighting = vec3(0.0);

	for (int i = 0; i < u_numLights; i++)
	{
		Light light = u_lights[i];
		vec3  L;
		float attenuation = 1.0;

		if (light.type == 0)
		{
			L = normalize(light.direction);
		}
		else if (light.type == 1)
		{
			vec3  lv   = light.position - p;
			float dist = length(lv);
			L          = normalize(lv);
			attenuation = 10.0 / (3.0 * dist * dist + 0.7 * dist + 1.0);
		}
		else
		{
			vec3  lv   = light.position - p;
			float dist = length(lv);
			L          = normalize(lv);
			attenuation = 10.0 / (3.0 * dist * dist + 0.7 * dist + 1.0);
			attenuation *= smoothstep(0.80, 0.95,
			                          dot(normalize(lv), normalize(light.direction)));
		}

		vec3  diffuse  = albedo * max(dot(N, L), 0.0) * (1.0 - fresnel);
		vec3  H        = normalize(L + V);
		float specPow  = pow(max(dot(N, H), 0.0), u_specExponent);
		vec3  specular = vec3(5.0) * specPow * fresnel;

		directLighting += (diffuse + specular) * attenuation * light.color.rgb * light.color.a;
	}

	vec3 R               = reflect(rd, N);
	vec3 reflectionColor = getSkyColor(R);
	vec3 ambient         = albedo * 0.1 + reflectionColor * fresnel;

	return directLighting + ambient;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main()
{
	vec3  N  = normalize(v_normal);
	vec3  rd = normalize(v_worldPos - u_camPos);

	vec2 screenUV = gl_FragCoord.xy / u_screenSize;

	// ── Sample scene depth and compute underwater depth ───────────────────────
	float rawDepth   = texture(u_sceneDepth, screenUV).r;
	bool  hasGeom    = rawDepth < 0.9999; // false = sky / far-plane background

	// Linear depth avoids the banding caused by perspective non-linearity in
	// the depth buffer; the difference is the actual view-space distance
	// between the water surface and the geometry behind it.
	float sceneLinearZ = linearizeDepth(rawDepth);
	float waterLinearZ = linearizeDepth(gl_FragCoord.z);
	float underwaterDepth = max(0.0, sceneLinearZ - waterLinearZ);

	// ── Refracted scene colour (nudge UV by the wave normal) ────────────────
	// Small refraction offset gives the characteristic underwater distortion.
	vec2 refractUV = clamp(screenUV + N.xz * 0.0, vec2(0.001), vec2(0.999));

	vec3 sceneColor;
	if (hasGeom)
	{
		sceneColor = texture(u_sceneColor, refractUV).rgb;
		// Exponential light absorption: deeper objects are tinted toward deep water colour
		float absorption = exp(-underwaterDepth * u_absorptionRate);
		sceneColor       = mix(u_deepColor * 0.3, sceneColor, absorption);
	}
	else
	{
		// No geometry behind this pixel – sample the sky through a refracted ray
		// so distant views through the water look naturally blue rather than showing
		// the opaque background colour baked into the scene snapshot.
		vec3 refractDir = refract(rd, N, 1.0 / 1.33); // water IOR ≈ 1.33
		sceneColor      = getSkyColor(refractDir);
	}

	// ── Water surface colour ─────────────────────────────────────────────────
	float depthFactor  = clamp(abs(v_worldPos.y) * 2.0, 0.0, 1.0);
	vec3  waterAlbedo  = mix(u_shallowColor, u_deepColor, depthFactor);
	vec3  surfaceColor = surfaceLighting(v_worldPos, rd, waterAlbedo, N);

	// Fresnel: grazing angles are near-fully reflective; normal incidence is mostly transparent
	float f0      = u_metallic;
	float fresnel = f0 + (1.0 - f0) * pow(clamp(1.0 - dot(-rd, N), 0.0, 1.0), u_fresnelPower);
	float alpha   = mix(0.4, 1.0, fresnel); // [0.4 .. 1.0]

	// ── Composite: water surface over (absorbed) scene colour ────────────────
	vec3 finalColor = mix(sceneColor, surfaceColor, alpha);

	// ── Foam at object–water intersection ────────────────────────────────────
	// if (hasGeom)
	// {
	// 	// Animated FBM noise makes the foam edge irregular and natural
	// 	float noise   = fbm(v_worldPos.xz * 4.0 + u_time * vec2(0.3, 0.5));
	// 	float bandMax = u_foamBand * (0.5 + noise); // noise ∈ [0,1] → band [0.5·w .. 1.5·w]
	// 	float foam = 1.0 - smoothstep(0.0, bandMax, underwaterDepth);
	// 	foam         *= smoothstep(-0.2, 0.04, underwaterDepth); // fade above surface


	// 	vec3 foamColor = vec3(1.0);
	// 	finalColor     = mix(finalColor, foamColor, foam);
	// }

	//finalColor = vec3((sceneLinearZ) - 2.5);
	// finalColor = vec3(underwaterDepth);

	// Output fully opaque – compositing was done manually above
	FragColor = vec4(finalColor, 1.0);
}