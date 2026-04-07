#version 330 core

// Constants

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform vec2 u_originalResolution;
uniform vec2 u_targetResolution;
uniform float u_overscan;
uniform sampler2D t_data;

// Bilinear
float smoothSample(sampler2D tex, vec2 uv)
{
	vec2 texelSize = 1.0 / u_originalResolution;

	vec2 P = uv * u_originalResolution - 0.5;

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
	float d00 = texture(tex, uv00).x;
	float d10 = texture(tex, uv10).x;
	float d01 = texture(tex, uv01).x;
	float d11 = texture(tex, uv11).x;

	// 7. Perform the Bilinear Interpolation
	float d_top = mix(d00, d10, f.x);
	float d_bottom = mix(d01, d11, f.x);
	float d = mix(d_top, d_bottom, f.y);

	return d;
}

void main()
{
	vec2 adjustedUV = 0.5 + (v_texCoord.xy - 0.5) / (1.0 + u_overscan);
	adjustedUV = clamp(adjustedUV, vec2(0.0), vec2(1.0));

	vec4 data = texture(t_data, adjustedUV);

	// Sample just the Y component using texelFetch (No filtering)
	// textureSize(t_data, 0) gets the resolution of the texture at mip level 0
	ivec2 texelCoords = ivec2(adjustedUV * textureSize(t_data, 0));
	data.y = texelFetch(t_data, texelCoords, 0).y;

	// data.x = smoothSample(t_data, v_texCoord.xy);

	FragColor = data;
}
