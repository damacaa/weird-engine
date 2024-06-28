#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

uniform vec2 u_resolution;
uniform float u_time;

uniform sampler2D u_colorTexture;
uniform sampler2D u_depthTexture;

void main()
{
    vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
    // vec3 col = texture(u_colorTexture, gl_FragCoord.xy / (4*vec2(1200, 800))).xyz;
    vec3 col = texture(u_colorTexture, screenUV).xyz;
    FragColor = vec4(col, 1.0);
    //FragColor = vec4(u_resolution, 0.0, 1.0);
}
