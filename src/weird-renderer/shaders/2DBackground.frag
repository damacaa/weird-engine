#version 330 core

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

void main()
{
    vec2 screenUV = gl_FragCoord.xy / u_resolution.xy;

    vec2 uv = (2.0 * v_texCoord) - 1.0;
    uv.x *= (u_resolution.x / u_resolution.y);
    
    float zoom = -1.5 * u_camMatrix[3].z;
    vec2 pos = (zoom * uv) - u_camMatrix[3].xy;
    float aspectRatio = u_resolution.x / u_resolution.y;
    vec2 zoomVec = vec2((zoom * aspectRatio) - 1.0, zoom);

    // vec2 pixel = zoom / u_resolution;
    // pixel.x *= aspectRatio;
    
    vec3 background = mix(vec3(0.55), vec3(0.7),
    (fract(0.1 * pos.x) > 0.01 && fract(0.1 * pos.y) > 0.01) ? 1.0 : 0.0);

    FragColor = vec4(background, 1.0);
}