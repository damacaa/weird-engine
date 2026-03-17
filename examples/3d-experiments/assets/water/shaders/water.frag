#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform sampler2D t_texture;
uniform sampler2D t_noise;

uniform float u_time = 0.1f;
uniform vec2 u_resolution;

// Fog calculations used to modulate distortion with distance
float linearDepth(float z, float near, float far)
{
    return (2.0 * near) / (far + near - z * (far - near));
}

void main()
{
    // Mesh uv to sample mask texture
    vec2 uv = v_texCoord;
    // Screen uv to sample scene texture
    vec2 screenUV = gl_FragCoord.xy / u_resolution;


    // Blur is less strong when its farther from the camera, same math as fog
    float depthMask = max(1.0 - (linearDepth(gl_FragCoord.z, 0.1, 100.0) * 1.5), 0.0);

    // Reduce strength when the flame is looking up
    float dotWorldY = 1.0f - abs(dot(normalize(v_normal), vec3(0, 1, 0)));


    // Calculate scene texture uv offset
    vec2 offset = vec2(0.005 * sin(0.2 * gl_FragCoord.y + u_time), 0);
    float maskk = 1.0 - clamp(0.05 * length(v_worldPos), 0.0, 1.0);
    vec4 backgroundColor = texture(t_texture, screenUV + (maskk * offset)) + (maskk * 0.1);

	FragColor = backgroundColor;
    // FragColor = vec4(vec3(maskk), 1.0); // Debug mask
}