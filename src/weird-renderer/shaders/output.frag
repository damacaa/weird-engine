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


const int MAX_STEPS = 100;
const float EPSILON = 0.01;
const float NEAR = 0.1f;
const float FAR = 100.0f;
float rayMarch(vec2 ro, vec2 rd)
{
  float d;

  float traveled = NEAR;

  for (int i = 0; i < MAX_STEPS; i++)
  {
    vec2 p = ro + (traveled * rd);

    d = texture(u_colorTexture, p).w;

    if (d < EPSILON || traveled > FAR)
      break;

    traveled += d;
  }

  return traveled;
}

void main()
{
    vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
    vec4 color = texture(u_colorTexture, screenUV);
    float distance = color.w;


     vec2 texelOffset = vec2(0.005, 0.0025);

    // Sample the texture at the center and neighboring pixels
    vec4 colorCenter = texture(u_colorTexture, screenUV);
    vec4 colorRight = texture(u_colorTexture, screenUV + vec2(texelOffset.x, 0.0));
    vec4 colorLeft = texture(u_colorTexture, screenUV - vec2(texelOffset.x, 0.0));
    vec4 colorUp = texture(u_colorTexture, screenUV + vec2(0.0, texelOffset.y));
    vec4 colorDown = texture(u_colorTexture, screenUV - vec2(0.0, texelOffset.y));

    // Simple average of the neighboring pixels for antialiasing
    vec4 averagedColor = (colorCenter + colorRight + colorLeft + colorUp + colorDown) / 5.0;

    //distance = averagedColor.r;

    //distance = sin(100.0 * distance);

    //vec3 col = vec3(distance);
    vec3 col = distance <= 0.5 ? color.xyz :  vec3(0.9);

    //vec2 TexCoords = fract(gl_FragCoord.xy * u_renderScale);

    // Dot effect
    //vec2 dist = TexCoords - vec2(0.5f, 0.5f);
    //float mask = ((1-length(dist)) - 0.25) * 2;
    //col *= mask;

    // CRT Line effect
    //float mask = sin(3.14 * (gl_FragCoord.y * u_renderScale));
    //mask = mask > 0.25 ? 1.0 : 0.0;
    //col *= mask;

    //FragColor = color.wwww;
    FragColor = vec4(col, 1.0);
}
