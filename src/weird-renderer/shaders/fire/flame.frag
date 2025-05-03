#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

// Imports the current position from the Vertex Shader
in vec3 crntPos;
// Imports the normal from the Vertex Shader
in vec3 Normal;
// Imports the color from the Vertex Shader
in vec3 color;
// Imports the texture coordinates from the Vertex Shader
in vec2 texCoord;


uniform float u_time;

uniform sampler2D u_noise0;
uniform sampler2D u_flameShape;

vec3 getGradientColor(float t) {
    const int NUM_STOPS = 4;
    vec3 colors[NUM_STOPS] = vec3[](
        vec3(0.1, 0.1, 0.1), // Red at 0.0
        vec3(0.8, 0.3, 0.2), // Green at 0.3
        vec3(0.9, 0.7, 0.2), // Blue at 0.7
        vec3(1.0, 1.0, 1.0)  // White at 1.0
    );
    float stops[NUM_STOPS] = float[](0.0, 0.6, 0.9, 1.0);

    for (int i = 0; i < NUM_STOPS - 1; ++i) {
        if (t >= stops[i] && t <= stops[i+1]) {
            float localT = (t - stops[i]) / (stops[i+1] - stops[i]);
            return mix(colors[i], colors[i+1], localT);
        }
    }
    return colors[NUM_STOPS - 1]; // fallback
}



float getGradient(float x, float y)
{
//    float f = -0.2f;
//    return f + (y * (1.0f - f));
    
    return min(1.0f, y * 1);
}

void main()
{
    vec2 uv = (texCoord);

    // Noise0
	float noise0 = texture(u_noise0, fract((1.2f * uv) - vec2(0.0f, u_time))).x - 0.5f;

    // Noise1
	float noise1 = texture(u_noise0, fract(uv - vec2(0.0f, 1.2f * u_time))).x - 0.5f;


    // Combine all noise
    float sumNoise = noise0 + noise1;
    sumNoise *= 0.05f;




    float shapeMask = texture(u_flameShape, clamp(uv + sumNoise, 0, 1)).x;




    // Add noise


    float alpha = shapeMask;
    // alpha = clamp(alpha, 0.0f, 1.0f);
    alpha = smoothstep(0,1,alpha);

    //shapeMask = shapeMask * sh * shapeMask;

	//float gradientMask = min(3 * (1.0f - uv.y), 1.0f);
	// gradientMask *= noiseMask;

	



	FragColor = vec4(getGradientColor(alpha), alpha);
	// FragColor = vec4(vec3(alpha), 1.0f);

//    if(alpha < 0.1f)
//        discard;

	//	FragColor = vec4(fract(texCoord + vec2(0, u_time)),0,1);
	//FragColor = vec4(vec3(sin(100*crntPos.x)),1);

    //FragColor = vec4(vec3(noiseMask), 1.0f);
}