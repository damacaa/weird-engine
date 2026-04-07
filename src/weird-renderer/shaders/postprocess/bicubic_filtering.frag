#version 330 core

// #define CRT
in vec2 v_texCoord;

// Outputs colors in RGBA
out vec3 FragColor;

uniform vec2 u_resolution;
uniform vec2 u_textureSize;
uniform float u_renderScale;

uniform float u_time;

uniform sampler2D t_colorTexture;

// NOTE: The 'weights' uniform is not used in the final Bicubic implementation below.
// I recommend removing it unless you plan to use it for something else (like a convolution filter).
// uniform float weights[9] = float[9](
// 	// 1,2,1,2,4,2,1,2,1
// 	// 0.5, 1, 0.5, 1, 10, 1, 0.5, 1, 0.5
// 	0, 0, 0, 4, 8, 4, 0, 0, 0);

float rand(vec2 co)
{
	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

// --- Helper Function: Catmull-Rom Spline Basis ---
// Returns the weight for the 4 points (P0, P1, P2, P3) for a fractional distance t [0, 1]
vec4 CatmullRomWeights(float t)
{
	float t2 = t * t;
	float t3 = t2 * t;

	// Catmull-Rom basis: [-0.5*t^3 + t^2 - 0.5*t, 1.5*t^3 - 2.5*t^2 + 1.0, -1.5*t^3 + 2.0*t^2 + 0.5*t, 0.5*t^3 -
	// 0.5*t^2]
	return vec4(-0.5 * t3 + t2 - 0.5 * t,		// P0 (i-1)
				1.5 * t3 - 2.5 * t2 + 1.0,		// P1 (i)
				-1.5 * t3 + 2.0 * t2 + 0.5 * t, // P2 (i+1)
				0.5 * t3 - 0.5 * t2				// P3 (i+2)
	);
}

// --- Bicubic Interpolation Main Function ---
float BicubicInterpolate(sampler2D tex, vec2 uv, vec2 size)
{
	vec2 texelSize = 1.0 / size;

	// 1. Determine the 4x4 grid of texel indices and fractional distance
	vec2 P = uv * size - 0.5;	 // Offset to center the interpolation region
	ivec2 i_j = ivec2(floor(P)); // Top-left index of the 4x4 grid is effectively i-1, j-1
	vec2 f = fract(P);			 // Fractional distance (weights)

	// 2. Calculate the Catmull-Rom weights for X and Y
	vec4 weightsX = CatmullRomWeights(f.x);
	vec4 weightsY = CatmullRomWeights(f.y);

	// 3. Sample the 4x4 grid and perform horizontal interpolation (4 times)
	vec4 finalDistances; // Stores the 4 horizontally interpolated distances (d_j-1 to d_j+2)

	for (int l = 0; l < 4; l++) // l corresponds to the Y-index (j-1, j, j+1, j+2)
	{
		// Get the 4 horizontal sample values (d_i-1,j+l to d_i+2,j+l)
		vec4 samplesX;
		for (int k = 0; k < 4; k++) // k corresponds to the X-index (i-1, i, i+1, i+2)
		{
			// Calculate the UV for the center of texel (i+k-1, j+l-1)
			// i_j is the index (i-1, j-1)
			vec2 sampleUV = (vec2(i_j) + vec2(float(k) - 0.5, float(l) - 0.5)) * texelSize;

			samplesX[k] = texture(tex, sampleUV).x;
		}

		// Horizontal interpolation (dot product of samples and weightsX)
		finalDistances[l] = dot(samplesX, weightsX);
	}

	// 4. Final vertical interpolation (dot product of the 4 horizontal results and weightsY)
	float d = dot(finalDistances, weightsY);

	return d;
}

void main()
{
	// *** The Bicubic Interpolation is applied here: ***
	float d = BicubicInterpolate(t_colorTexture, v_texCoord, u_textureSize);

	// 8. Apply SDF Anti-aliasing
	float width = fwidth(d);
	// Clamp width to prevent huge jaggies from causing extreme blending near edges
	// width = max(width, 0.001); // Optional, for stability
	float alpha = smoothstep(width, -width, d);

	// Final color (e.g., white shape on a black background)
	vec3 col = vec3(alpha);

	FragColor = col;
}