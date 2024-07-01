#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

uniform vec2 u_resolution;
uniform float u_renderScale;

uniform float u_time;

uniform sampler2D u_colorTexture;
uniform sampler2D u_depthTexture;

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}


void main()
{
    vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
    vec3 col = texture(u_colorTexture, screenUV).xyz;


    vec2 TexCoords = fract(gl_FragCoord.xy * u_renderScale);

    // Dot effect
    //vec2 dist = TexCoords - vec2(0.5f, 0.5f);
    //float mask = ((1-length(dist)) - 0.25) * 2;
    //col *= mask;

    // CRT Line effect
    //float mask = sin(3.14 * (gl_FragCoord.y * u_renderScale));
    //mask = mask > 0.25 ? 1.0 : 0.0;
    //col *= mask;

    FragColor = vec4(col, 1.0);
}
