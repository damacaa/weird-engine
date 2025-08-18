#version 330 core

#define DEBUG_SHOW_DISTANCE 0

// Constants

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

uniform mat4 u_camMatrix;
uniform vec2 u_resolution;
uniform float u_time;

uniform sampler2D t_materialDataTexture;
uniform sampler2D t_currentColorTexture;
uniform vec3 u_staticColors[16];

vec3 randomColor(int index) {
    float seed = float(index) * 43758.5453;
    float r = fract(sin(seed) * 43758.5453);
    float g = fract(sin(seed + 1.0) * 43758.5453);
    float b = fract(sin(seed + 2.0) * 43758.5453);
    return vec3(r, g, b);
}

void main()
{
    vec2 screenUV = v_texCoord;
    vec4 color = texture(t_materialDataTexture, screenUV);
    float distance = color.x;
    int materialId = int(color.y);
    float mask = color.z;
    vec3 c = u_staticColors[materialId];

    float zoom = -u_camMatrix[3].z;

    vec3 currentColor = texture(t_currentColorTexture, screenUV).xyz;

    // TODO: uniform?
    float zoomFactor = (zoom - 10.0) * 0.02;
    zoomFactor = smoothstep(0.0, 1.0, zoomFactor);
    c = mix(c, currentColor, 0.9 * (1.0 - zoomFactor) * mask);

    FragColor = vec4(c, mask);
}

