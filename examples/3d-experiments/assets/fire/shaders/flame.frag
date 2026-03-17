#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;


uniform float u_time;

uniform sampler2D t_noise;
uniform sampler2D t_flameShape;

const int NUM_STOPS = 4;
uniform vec3 colors[NUM_STOPS] = vec3[](
    vec3(0.1, 0.1, 0.1), // Grey
    vec3(0.8, 0.3, 0.2), // Red
    vec3(1.1, 0.7, 0.2), // Orange
    vec3(1.2, 1.2, 1.2)  // White
);

uniform float stops[NUM_STOPS] = float[](0.0, 0.7, 0.9, 1.0);

vec3 getGradientColor(float t) 
{
    // This could be replace with a texture, but this approach makes it easier to iterate and choose colors
    for (int i = 0; i < NUM_STOPS - 1; ++i) {
        if (t >= stops[i] && t <= stops[i+1]) {
            float localT = (t - stops[i]) / (stops[i+1] - stops[i]);
            return mix(colors[i], colors[i+1], localT);
        }
    }

    return colors[NUM_STOPS - 1]; // fallback
}


void main()
{
    vec2 uv = v_texCoord;

    // Noise0
	float noise0 = texture(t_noise, fract((3.0f * uv) - vec2(0.0f, u_time))).x - 0.5f;

    // Noise1
	float noise1 = texture(t_noise, fract((2.0 * uv) - vec2(0.0f, 1.2f * u_time))).x - 0.5f;

    // Combine all noise
    float sumNoise = noise0 + noise1;

    // Reduce noise at the bottom of the flame
    float noiseMask = clamp(3.0f * (v_texCoord.y - 0.35f), 0, 1);
    sumNoise *= noiseMask;

    // Reduce overall noise strength
    float noiseIntensity = 0.1f;
    sumNoise *= noiseIntensity;

    // Sample texture with noise applied to uv coordinates
    float alpha = texture(t_flameShape, clamp(uv + sumNoise, 0, 1)).x;

    alpha = smoothstep(.3, 1.0, alpha);
    alpha = alpha * (alpha + 0.01f);
    alpha = clamp(alpha, 0.0, 1.0);

	FragColor = vec4(getGradientColor(alpha), alpha);
    // FragColor = vec4(vec3(sumNoise / noiseIntensity), 1.0f); // Debug noise
    // FragColor = vec4(vec3(alpha), 1.0f); // Debug alpha
}