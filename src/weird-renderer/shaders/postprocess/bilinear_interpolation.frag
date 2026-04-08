#version 300 es
precision highp float;
precision highp int;

in vec2 v_texCoord;

// Outputs colors in RGBA
out vec4 FragColor;

uniform vec2 u_resolution;
uniform vec2 u_textureSize;
uniform float u_renderScale;

uniform float u_time;

uniform sampler2D t_colorTexture;

float rand(vec2 co)
{
	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

const float weights[9] = float[9](
	// 1,2,1,2,4,2,1,2,1
	// 0.5, 1, 0.5, 1, 10, 1, 0.5, 1, 0.5
	0.0, 0.0, 0.0, 4.0, 8.0, 4.0, 0.0, 0.0, 0.0);

void main()
{
	// 1. Calculate the size of a single texel
	vec2 texelSize = 1.0 / u_textureSize;

	// 2. Determine the coordinates of the *center* of the top-left texel (T_ij)
	// The -0.5 is a common trick to ensure the interpolation point falls cleanly
	// between the texel centers, rather than on a texel center.
	vec2 P = v_texCoord * u_textureSize - 0.5;

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
	float d00 = texture(t_colorTexture, uv00).x;
	float d10 = texture(t_colorTexture, uv10).x;
	float d01 = texture(t_colorTexture, uv01).x;
	float d11 = texture(t_colorTexture, uv11).x;

	// 7. Perform the Bilinear Interpolation
	float d_top = mix(d00, d10, f.x);
	float d_bottom = mix(d01, d11, f.x);
	float d = mix(d_top, d_bottom, f.y);

	// 8. Apply SDF Anti-aliasing
	float width = fwidth(d);
	// Clamp width to prevent huge jaggies from causing extreme blending near edges
	// width = max(width, 0.001); // Optional, for stability
	float alpha = smoothstep(width, -width, d);

	// Final color (e.g., white shape on a black background)
	vec3 col = vec3(alpha);

	FragColor = vec4(col, 1.0);
}
