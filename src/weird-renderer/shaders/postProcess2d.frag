#version 330 core

#define DITHERING 1
#define SHADOWS_ENABLED 1


// Constants
const int MAX_STEPS = 1000;
const float EPSILON = 0.002;
const float NEAR = 0.1f;
const float FAR = 100.0f;

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Uniforms
uniform int u_loadedObjects;
uniform samplerBuffer myBufferTexture;

uniform vec2 u_resolution;
uniform float u_time;

uniform mat4 u_cameraMatrix;
uniform vec3 u_staticColors[16];

uniform sampler2D u_colorTexture;
uniform sampler2D u_depthTexture;

uniform vec2 u_directionalLightDirection = vec2(0.7071, 0.7071);

#if (DITHERING == 1)

uniform float _Spread = .15f;
uniform int _ColorCount = 5;

// Dithering and posterizing
uniform int bayer2[2 * 2] = int[2 * 2](
    0, 2,
    3, 1);

uniform int bayer4[4 * 4] = int[4 * 4](
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5);

uniform int bayer8[8 * 8] = int[8 * 8](
    0, 32, 8, 40, 2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44, 4, 36, 14, 46, 6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
    3, 35, 11, 43, 1, 33, 9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47, 7, 39, 13, 45, 5, 37,
    63, 31, 55, 23, 61, 29, 53, 21);

float GetBayer2(int x, int y)
{
  return float(bayer2[(x % 2) + (y % 2) * 2]) * (1.0f / 4.0f) - 0.5f;
}

float GetBayer4(int x, int y)
{
  return float(bayer4[(x % 4) + (y % 4) * 4]) * (1.0f / 16.0f) - 0.5f;
}

float GetBayer8(int x, int y)
{
  return float(bayer8[(x % 8) + (y % 8) * 8]) * (1.0f / 64.0f) - 0.5f;
}

#endif

float map(vec2 p)
{
  return texture(u_colorTexture, p).w;
}

float rayMarch(vec2 ro, vec2 rd)
{
  float d;

  float traveled = 0.0;

  for (int i = 0; i < MAX_STEPS; i++)
  {
    vec2 p = ro + (traveled * rd);

    d = map(p);

    if (d <= EPSILON)
      break;

    if(p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0)
      return 1.0;

    //traveled += 0.001;
    traveled += d;

  }

  return traveled;
}

vec3 render(vec2 uv)
{

  vec3 background = vec3(0.4 + 0.4 * mod(floor(10.0 * uv.x) + floor(10.0* (u_resolution.y / u_resolution.x) * uv.y), 2.0));
  //vec3 background = vec3(1.0);


#if SHADOWS_ENABLED

  float d = rayMarch(uv, u_directionalLightDirection.xy);
  return (d < 1.0 ? 0.1: 1.0) * background;
  //return d * background;

#else

  return background;

#endif

}


void main()
{
    vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
    vec4 color = texture(u_colorTexture, screenUV);
    float distance = color.w;

    vec3 col = distance <= 0.0 ? color.xyz : render(screenUV);

#if (DITHERING == 1)

  int x = int(gl_FragCoord.x);
  int y = int(gl_FragCoord.y);
  col = col + _Spread * GetBayer4(x, y);

  col.r = floor((_ColorCount - 1.0f) * col.r + 0.5) / (_ColorCount - 1.0f);
  col.g = floor((_ColorCount - 1.0f) * col.g + 0.5) / (_ColorCount - 1.0f);
  col.b = floor((_ColorCount - 1.0f) * col.b + 0.5) / (_ColorCount - 1.0f);

#endif

  FragColor = vec4(col.xyz, 1.0);
}
