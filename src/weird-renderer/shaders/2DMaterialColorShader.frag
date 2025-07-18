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

    float pixel = 0.2 / u_resolution.y;
    vec3 background = mix(u_staticColors[3], u_staticColors[2],
    min(fract(0.1 * pos.x), fract(0.1 * pos.y)) > pixel * zoom ? 1.0 : 0.0);

    // Accumulate colors of neighboring cells
    vec3 blendedColor = vec3(0.0);
    float totalWeight = 0.0;

    for (int x = -10; x <= 10; ++x) {
        for (int y = -10; y <= 10; ++y) {
            vec2 offset = vec2(float(x), float(y)) / u_resolution;
            vec4 neighbor = texture(t_colorTexture, screenUV + offset);
            float w = 1.0;
            if (x == 0 && y == 0) {
                w = 2.0; // Weight center more if desired
            }
            vec3 neighborColor = u_staticColors[int(neighbor.y)];
            blendedColor += neighborColor * w;
            totalWeight += w;
        }
    }

    blendedColor /= totalWeight;

    // Decide whether to use blended or background
    c = false || distance <= 0.0 ? mix(c, blendedColor, mask) : background;

    FragColor = vec4(c, distance);
}

