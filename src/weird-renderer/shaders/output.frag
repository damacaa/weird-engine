#version 330 core

// Outputs colors in RGBA
out vec3 FragColor;

uniform vec2 u_resolution;
uniform float u_renderScale;

uniform float u_time;

uniform sampler2D u_colorTexture;

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}




void main()
{
    vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
    vec4 color = texture(u_colorTexture, screenUV);
    float distance = color.w;

    vec3 col = distance <= 0.5 ? color.xyz :  vec3(0.9);

    // Dot effect
    // vec2 TexCoords = fract(gl_FragCoord.xy * u_renderScale);
    // vec2 dist = TexCoords - vec2(0.5f, 0.5f);
    // float mask = ((1.0 -length(dist)) - 0.25) * 2;

    // CRT Line effect
    // float mask = sin(3.14 * (gl_FragCoord.y * u_renderScale));
    // mask = mask > 0.25 ? 1.0 : 0.0;

    //FragColor = color.xyz * mask;
    //FragColor = color.wwww;
    
    FragColor = color.xyz;
}
