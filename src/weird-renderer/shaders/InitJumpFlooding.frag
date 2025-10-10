#version 330 core

// Constants

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D t_distanceTexture;

void main()
{
    vec2 screenUV = v_texCoord;
    vec4 color = texture(t_distanceTexture, screenUV);
    float distance = color.x;

    vec2 seed = distance > 0.0 ? screenUV : vec2(-1.0);
    float initDistance = distance >= 0.0 ? 0.0 : 1e9;
    FragColor = vec4(seed, initDistance, 0.0);
}