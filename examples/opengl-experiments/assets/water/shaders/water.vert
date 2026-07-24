#version 300 es

// Vertex attributes
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_color;
layout(location = 3) in vec2 in_texCoord;

// Outputs to fragment shader
out vec3 v_worldPos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_texCoord;

// Uniforms
uniform mat4 u_model;
uniform mat4 u_camMatrix;
uniform float u_time;

// ── Gerstner wave ─────────────────────────────────────────────────────────────
// Each wave moves vertices along a circular orbit:
//   horizontal: Q·A·D·cos(θ)    vertical: A·sin(θ)
//   where Q = steepness, A = amplitude, D = direction, k = wavenumber, θ = k·dot(D,xz) + ω·t
//
// Parameters: direction (normalised), wavelength (world units), amplitude, steepness [0..1]
//
// Outputs displacement and accumulates the partial derivatives needed for the
// analytic normal (no finite-difference required).

struct GerstnerOut
{
	vec3 displacement; // vertex offset (x, y, z)
	vec3 dPos_dx;	   // ∂P/∂x – used to build the normal
	vec3 dPos_dz;	   // ∂P/∂z
};

GerstnerOut gerstner(vec2 xz, vec2 dir, float wavelength, float amplitude, float steepness)
{
	float k = 6.28318 / wavelength; // wavenumber
	float c = sqrt(9.81 / k);		// deep-water phase speed
	float f = k * dot(dir, xz) - c * u_time;
	float sinF = sin(f);
	float cosF = cos(f);

	// Q: steepness controls how peaked the crests are (0 = pure sine, 1 = loops)
	float Q = steepness;

	GerstnerOut o;
	o.displacement = vec3(Q * amplitude * dir.x * cosF, amplitude * sinF, Q * amplitude * dir.y * cosF);

	// Partial derivatives of the displaced position (for normal computation)
	float WA = k * amplitude;
	o.dPos_dx = vec3(-Q * WA * dir.x * dir.x * sinF, WA * dir.x * cosF, -Q * WA * dir.x * dir.y * sinF);
	o.dPos_dz = vec3(-Q * WA * dir.x * dir.y * sinF, WA * dir.y * cosF, -Q * WA * dir.y * dir.y * sinF);

	return o;
}

// ── Rotate a 2-D point ────────────────────────────────────────────────────────
vec2 rot2(vec2 p, float a)
{
	float c = cos(a);
	float s = sin(a);
	return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

void main()
{
	vec2 xz = in_position.xz;

	// ── Domain warp with incommensurable spatial frequencies ──────────────────
	// Use six sinusoids whose spatial periods have no common rational factor.
	// Frequencies chosen as prime-like irrational multiples so the warp pattern
	// never tiles at any human-visible scale.
	float t = u_time;
	vec2 warp = vec2(
		sin(xz.x * 0.1731 + xz.y * 0.0893 + t * 0.071) * 1.6 + cos(xz.x * 0.2473 - xz.y * 0.1337 + t * 0.043) * 0.9,
		cos(xz.x * 0.0971 + xz.y * 0.2011 + t * 0.059) * 1.4 + sin(xz.x * 0.1619 - xz.y * 0.3001 + t * 0.037) * 0.7);
	vec2 xzW = xz + warp;

	// ── Wave cascade A – primary swell direction ──────────────────────────────
	vec3 totalDisp = vec3(0.0);
	vec3 total_dpdx = vec3(1.0, 0.0, 0.0);
	vec3 total_dpdz = vec3(0.0, 0.0, 1.0);

	GerstnerOut g0 = gerstner(xzW, normalize(vec2(1.0, 0.4)), 8.09, 0.110, 0.55);
	totalDisp += g0.displacement;
	total_dpdx += g0.dPos_dx;
	total_dpdz += g0.dPos_dz;

	GerstnerOut g1 = gerstner(xzW, normalize(vec2(-0.5, 1.0)), 5.00, 0.075, 0.45);
	totalDisp += g1.displacement;
	total_dpdx += g1.dPos_dx;
	total_dpdz += g1.dPos_dz;

	GerstnerOut g2 = gerstner(xzW, normalize(vec2(0.4, -1.0)), 3.09, 0.040, 0.30);
	totalDisp += g2.displacement;
	total_dpdx += g2.dPos_dx;
	total_dpdz += g2.dPos_dz;

	// ── Wave cascade B – evaluated in a rotated frame ─────────────────────────
	// Rotation by sqrt(2) radians ≈ 81.03° – irrational, so cascade B can never
	// constructively align with cascade A at any finite scale.
	vec2 xzR = rot2(xzW, 1.41421356);

	GerstnerOut g3 = gerstner(xzR, normalize(vec2(1.0, 0.3)), 4.72, 0.055, 0.40);
	totalDisp += g3.displacement;
	total_dpdx += g3.dPos_dx;
	total_dpdz += g3.dPos_dz;

	GerstnerOut g4 = gerstner(xzR, normalize(vec2(-0.7, 1.0)), 2.62, 0.030, 0.28);
	totalDisp += g4.displacement;
	total_dpdx += g4.dPos_dx;
	total_dpdz += g4.dPos_dz;

	xzR = rot2(xzW, 2.123457);

	GerstnerOut g5 = gerstner(xzR, normalize(vec2(0.9, -0.6)), 1.62, 0.015, 0.18);
	totalDisp += g5.displacement;
	total_dpdx += g5.dPos_dx;
	total_dpdz += g5.dPos_dz;

	GerstnerOut g6 = gerstner(xzR, normalize(vec2(-1.0, -0.3)), 1.00, 0.008, 0.12);
	totalDisp += g6.displacement;
	total_dpdx += g6.dPos_dx;
	total_dpdz += g6.dPos_dz;

	vec3 pos = in_position + totalDisp;

	vec3 waveNormal = normalize(cross(total_dpdz, total_dpdx));

	v_worldPos = vec3(u_model * vec4(pos, 1.0));
	v_normal = waveNormal;
	v_color = in_color;
	v_texCoord = in_texCoord;

	gl_Position = u_camMatrix * vec4(v_worldPos, 1.0);
}
