#version 300 es
precision highp float;
precision highp int;

// #define CRT
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

	// 2. Determine the texture coordinate of the top-left texel (T_ij)
	// The floor operation finds the integer texel coordinates (i, j)
	vec2 texel_i_j = floor(v_texCoord * u_textureSize);

	// 3. Calculate the normalized UV for the center of the top-left texel (T_ij)
	// This is the (i, j) texel's center UV
	vec2 uv_i_j = (texel_i_j + 0.5) * texelSize;

	// 4. Calculate the fractional distance (weights) from the top-left center
	// This gives the interpolation factors (fx, fy) between 0.0 and 1.0
	vec2 f = (v_texCoord - uv_i_j) / texelSize;

	// 5. Calculate the UVs for the four corner texels T_ij, T_i+1,j, T_i,j+1, T_i+1,j+1
	vec2 uv00 = texel_i_j * texelSize;					  // (i, j) - Note: Using corner for simpler lookup
	vec2 uv10 = (texel_i_j + vec2(1.0, 0.0)) * texelSize; // (i+1, j)
	vec2 uv01 = (texel_i_j + vec2(0.0, 1.0)) * texelSize; // (i, j+1)
	vec2 uv11 = (texel_i_j + vec2(1.0, 1.0)) * texelSize; // (i+1, j+1)

	// 6. Sample the four texels using GL_NEAREST (because we set it)
	// We only care about the Red channel (.x) as it holds the SDF
	float d00 = texture(t_colorTexture, uv00).x;
	float d10 = texture(t_colorTexture, uv10).x;
	float d01 = texture(t_colorTexture, uv01).x;
	float d11 = texture(t_colorTexture, uv11).x;

	// 7. Perform the Bilinear Interpolation
	// Linear interpolation along X (horizontal):
	// Top row interpolation
	float d_top = mix(d00, d10, f.x);
	// Bottom row interpolation
	float d_bottom = mix(d01, d11, f.x);

	// Linear interpolation along Y (vertical):
	float d = mix(d_top, d_bottom, f.y);

	// 8. Use the interpolated distance (d) to render the final smooth shape
	// This is the correct way to anti-alias the SDF:
	float width = fwidth(d);
	float alpha = smoothstep(width, -width, d);

	// Final color (e.g., white shape on a black background)
	vec3 col = vec3(alpha);

	FragColor = vec4(col, 1.0);
}
