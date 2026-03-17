#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform sampler2D t_texture;
uniform sampler2D t_noise;
uniform sampler2D t_flameShape;

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

    // Use the flame shape to mask the noise
    float bottomFade = texture(t_flameShape, vec2(uv.x, min(uv.y, 0.5))).r;
    bottomFade = smoothstep(0.5, 1.0, bottomFade);

    // Fade at the top
    float topFade = 1.0 - uv.y;
    bottomFade *= topFade;

    // Blur is less strong when its farther from the camera, same math as fog
    float depthMask = max(1.0 - (linearDepth(gl_FragCoord.z, 0.1, 100.0) * 1.5), 0.0);

    // Reduce strength when the flame is looking up
    float dotWorldY = 1.0f - abs(dot(normalize(v_normal), vec3(0, 1, 0)));

    // Combine all masks
    float mask = bottomFade * depthMask * dotWorldY;

    // Calculate scene texture uv offset
    vec2 offset = vec2(0, mask * 0.1f * ((texture(t_noise, (2.0f * uv) - (1.0f * u_time)).x) - 0.5));

	FragColor = texture(t_texture, screenUV + offset);
    // FragColor = vec4(vec3(mask), 1.0); // Debug mask
}