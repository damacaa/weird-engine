#version 330 core

// #define CORRECT_INSIDE
#define CORRECT_OUTSIDE

// Constants

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform vec2 u_resolution;
uniform float u_time;
uniform sampler2D t_originalDistanceTexture;
uniform sampler2D t_distanceTexture;

void main()
{
    vec2 screenUV = v_texCoord;
    vec4 color = texture(t_originalDistanceTexture, screenUV);
    vec4 floodResult = texture(t_distanceTexture, screenUV);
    vec2 seed = floodResult.xy;
    float floodDist = sqrt(floodResult.z);

    float realDistance = color.x; // Old distance

    #ifdef CORRECT_INSIDE
    realDistance = realDistance < 0.0 ? -floodDist: realDistance; //
    #endif

    #ifdef CORRECT_OUTSIDE
    realDistance = mix(realDistance, floodDist, smoothstep(0.0025, 0.005, realDistance));
    // realDistance = realDistance > 0.001 ? floodDist: realDistance; //
    #endif

    FragColor = vec4(realDistance, color.yzw);
}