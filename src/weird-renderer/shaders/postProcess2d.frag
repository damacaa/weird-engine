#version 330 core

#define DITHERING 1
#define SHADOWS_ENABLED 1
#define SOFT_SHADOWS 0

#define DEBUG_SHOW_DISTANCE 0

// Constants
const int MAX_STEPS = 1000;
const float EPSILON = 0.0;
const float NEAR = 0.1f;
const float FAR = 1.2f;

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

uniform vec2 u_resolution;
uniform float u_time;

uniform sampler2D t_colorTexture;

uniform vec2 u_directionalLightDirection = vec2(0.7071f, 0.7071f);

#if (DITHERING == 1)

uniform float u_spread = .025;
uniform int u_colorCount = 16;

// Dithering and posterizing
uniform int u_bayer2[2 * 2] = int[2 * 2](
    0, 2,
    3, 1);

uniform int u_bayer4[4 * 4] = int[4 * 4](
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5);

uniform int u_bayer8[8 * 8] = int[8 * 8](
    0, 32, 8, 40, 2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44, 4, 36, 14, 46, 6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
    3, 35, 11, 43, 1, 33, 9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47, 7, 39, 13, 45, 5, 37,
    63, 31, 55, 23, 61, 29, 53, 21);

float Getu_bayer2(int x, int y)
{
  return float(u_bayer2[(x % 2) + (y % 2) * 2]) * (1.0f / 4.0f) - 0.5f;
}

float Getu_bayer4(int x, int y)
{
  return float(u_bayer4[(x % 4) + (y % 4) * 4]) * (1.0f / 16.0f) - 0.5f;
}

float Getu_bayer8(int x, int y)
{
  return float(u_bayer8[(x % 8) + (y % 8) * 8]) * (1.0f / 64.0f) - 0.5f;
}

#endif

float map(vec2 p)
{
  return texture(t_colorTexture, p).w;
}

float rayMarch(vec2 ro, vec2 rd, out float minDistance)
{
  float d;
  minDistance = 10000.0;

  float traveled = 0.0;

  for (int i = 0; i < MAX_STEPS; i++)
  {
    vec2 p = ro + (traveled * rd);

    d = map(p);

    minDistance = min(d, minDistance);

    if (d <= EPSILON)
      break;

    if (p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0)
      return FAR;

    // traveled += 0.01;
    traveled += d;
    // traveled += min(d, 0.01);

    if (traveled >= FAR)
    {
      return FAR;
    }
  }

  return traveled;
}

float render(vec2 uv)
{

#if SHADOWS_ENABLED

  // Point light
  vec2 rd = normalize(vec2(1.0) - uv);

  // Directional light
  // vec2 rd = u_directionalLightDirection.xy;

  float d = map(uv);
  float minD;
  vec2 offsetPosition = uv + (2.0 / u_resolution) * rd;
  if (d <= 0.0)
  {

    // float dd =  -max(-0.05, d) - 0.01;
    // dd = max(0.0, dd);
    float distanceFallof = 1.25;
    float maxDistance = 0.05;

    float dd = mix(0.0, 1.0, -(distanceFallof * d)); // - 0.005;
    dd = min(maxDistance, dd);

    // Get distance at offset position
    d = rayMarch(offsetPosition, rd, minD);
    return d < FAR ? 1.0 + (-1.5f * dd) : 2.0; // * dot(n,  rd);
  }

  d = rayMarch(uv, rd, minD);

  // return (10.0 * minD)+0.5;

#if SOFT_SHADOWS
  return mix(0.85, 1.0, d / FAR);
#else
  return d < FAR ? 0.85 : 1.0;
#endif

#else

  return 1.0;

#endif
}

void main()
{
  vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
  vec4 color = texture(t_colorTexture, screenUV);
  float distance = color.w;

#if DEBUG_SHOW_DISTANCE

  if (abs(distance) < (0.5 / u_resolution.x))
  {
    FragColor = vec4(vec3(0.0), 1.0);
  }
  else
  {
    float value = 0.5 * (cos(500.0 * distance) + 1.0);
    value = value * value * value;
    vec3 debugColor = distance > 0 ? mix(vec3(1), vec3(0.2), value) :                    // outside
                          (distance + 1.0) * mix(vec3(1.0, 0.2, 0.2), vec3(0.1), value); // inside

    FragColor = vec4(debugColor, 1.0);
  }

  return;
#endif

  float light = render(screenUV);
  vec3 col = light * color.xyz;

  // col = vec3(light);

#if (DITHERING == 1)

  int x = int(gl_FragCoord.x);
  int y = int(gl_FragCoord.y);
  col = col + u_spread * Getu_bayer4(x, y);

  col.r = floor((u_colorCount - 1.0f) * col.r + 0.5) / (u_colorCount - 1.0f);
  col.g = floor((u_colorCount - 1.0f) * col.g + 0.5) / (u_colorCount - 1.0f);
  col.b = floor((u_colorCount - 1.0f) * col.b + 0.5) / (u_colorCount - 1.0f);

#endif

  // FragColor = vec4(vec3(light), 1.0);
  FragColor = vec4(col.xyz, 1.0);
  // FragColor = vec4(vec3(5.0*map(screenUV)), 1.0);
}
