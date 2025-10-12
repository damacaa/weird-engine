#version 330 core

#define CORRECT_INSIDE
// #define CORRECT_OUTSIDE

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

    bool condition;

    #ifdef CORRECT_INSIDE
    condition = distance > 0.0;
    #endif

    #ifdef CORRECT_OUTSIDE
    condition = distance < 0.0;
    #endif

    vec2 seed = condition ? screenUV : vec2(-1.0);
    float initDistance = condition ? 0.0 : 1e9;

    FragColor = vec4(seed, initDistance, 0.0);
}