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

uniform sampler2D t_colorTexture;
uniform vec3 u_staticColors[16];

float map(vec2 p)
{
    return texture(t_colorTexture, p).w;
}


vec3 randomColor(int index) {
    float seed = float(index) * 43758.5453;
    float r = fract(sin(seed) * 43758.5453);
    float g = fract(sin(seed + 1.0) * 43758.5453);
    float b = fract(sin(seed + 2.0) * 43758.5453);
    return vec3(r, g, b);
}


void main()
{
    vec2 screenUV = gl_FragCoord.xy / u_resolution.xy;
    vec4 color = texture(t_colorTexture, screenUV);
    float distance = color.x;
    float mask = color.z;
    vec3 c = u_staticColors[int(color.y)];

    vec2 uv = (2.0 * v_texCoord) - 1.0;
    float zoom = -u_camMatrix[3].z;
    vec2 pos = (zoom * uv) - u_camMatrix[3].xy;
    float aspectRatio = u_resolution.x / u_resolution.y;
    vec2 zoomVec = vec2((zoom * aspectRatio) - 1.0, zoom);





    // Decide whether to use blended or background
    // c = distance <= 0.0 ? c : background;

    // FragColor = vec4(c, distance);
    FragColor = vec4(c, mask);
}

