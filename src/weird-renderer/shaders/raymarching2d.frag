#version 330 core

#define DITHERING 0
#define SHADOWS_ENABLED 0
#define BLEND_SHAPES 1

uniform float k = 1.5;

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

// Outputs u_staticColors in RGBA
layout(location = 0) out float FragColor;

// Uniforms
uniform int u_loadedObjects;
uniform samplerBuffer myBufferTexture;

uniform vec2 u_resolution;
uniform float u_time;

uniform mat4 u_cameraMatrix;
uniform vec3 u_staticColors[16];
uniform vec3 directionalLightDirection;

uniform sampler2D u_colorTexture;
uniform sampler2D u_depthTexture;

// Constants
const int MAX_STEPS = 100;
const float EPSILON = 0.01;
const float NEAR = 0.1f;
const float FAR = 100.0f;

// Operations
float fOpUnionSoft(float a, float b, float r)
{
  float e = max(r - abs(a - b), 0);
  return min(a, b) - e * e * 0.25 / r;
}

float fOpUnionSoft(float a, float b, float r, float invR)
{
  float e = max(r - abs(a - b), 0);
  return min(a, b) - e * e * 0.25 * invR;
}

float smin(float a, float b, float k)
{
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
  return mix(b, a, h) - k * h * (1.0 - h);
}

vec2 squareFrame(vec2 screenSize, vec2 coord)
{
  vec2 position = 2.0 * (coord.xy / screenSize.xy) - 1.0;
  position.x *= screenSize.x / screenSize.y;
  return position;
}

const float PI = 3.14159265359;

vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d)
{
  return a + b * cos(6.28318 * (c * t + d));
}

// r^2 = x^2 + y^2
// r = sqrt(x^2 + y^2)
// r = length([x y])
// 0 = length([x y]) - r
float shape_circle(vec2 p)
{
  return length(p) - 0.5;
}

// y = sin(5x + t) / 5
// 0 = sin(5x + t) / 5 - y
float shape_sine(vec2 p)
{
  return p.y - sin(p.x * 5.0 + u_time) * 0.2;
}

float shape_box2d(vec2 p, vec2 b)
{
  vec2 d = abs(p) - b;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float shape_line(vec2 p, vec2 a, vec2 b)
{
  vec2 dir = b - a;
  return abs(dot(normalize(vec2(dir.y, -dir.x)), a - p));
}

float shape_segment(vec2 p, vec2 a, vec2 b)
{
  float d = shape_line(p, a, b);
  float d0 = dot(p - b, b - a);
  float d1 = dot(p - a, b - a);
  return d1 < 0.0 ? length(a - p) : d0 > 0.0 ? length(b - p)
                                             : d;
}

float shape_circles_smin(vec2 p, float t)
{
  return smin(shape_circle(p - vec2(cos(t))), shape_circle(p + vec2(sin(t), 0)), 0.8);
}

vec3 draw_line(float d, float thickness)
{
  const float aa = 3.0;
  return vec3(smoothstep(0.0, aa / u_resolution.y, max(0.0, abs(d) - thickness)));
}

vec3 draw_line(float d)
{
  return draw_line(d, 0.0025);
}

vec3 getMaterial(vec2 p, int materialId)
{
  return u_staticColors[materialId];
}

vec4 getColor(vec2 p)
{
  float d = FAR;
  // d = p.y - 2.5 * sin(0.5 * p.x);
  vec3 col = vec3(0.0);

  for (int i = 0; i < u_loadedObjects; i++)
  {
    vec4 positionAndMaterial = texelFetch(myBufferTexture, 2 * i);
    int materialId = int(positionAndMaterial.w);

    float objectDist = shape_circle(p - positionAndMaterial.xy);

#if BLEND_SHAPES
    d = fOpUnionSoft(objectDist, d, k);
    float delta = 1 - (max(k - abs(objectDist - d), 0.0) / k); // After new d is calculated
    col = mix(getMaterial(p, materialId), col, delta);
#else
    d = min(d, objectDist);
    col = d == objectDist ? getMaterial(positionAndMaterial.xy, materialId) : col;
#endif
  }

  return vec4(col, d);
}

float map(vec2 p)
{
  // if (p.x < 0.0 || p.x > 30.0 || p.y <= 0)
  //   return -1.0;

  float d = FAR;
  // d = p.y - 2.5 * sin(0.5 * p.x);

  for (int i = 0; i < u_loadedObjects; i++)
  {
    vec4 positionAndMaterial = texelFetch(myBufferTexture, 2 * i);
    float objectDist = shape_circle(p - positionAndMaterial.xy);

#if BLEND_SHAPES
    d = fOpUnionSoft(objectDist, d, k);
#else
    d = min(d, objectDist);
#endif
  }

  return d;
}

float rayMarch(vec2 ro, vec2 rd)
{
  float d;

  float traveled = NEAR;

  for (int i = 0; i < MAX_STEPS; i++)
  {
    vec2 p = ro + (traveled * rd);

    d = map(p);

    if (d < EPSILON || traveled > FAR)
      break;

    traveled += d;
  }

  return traveled;
}

vec3 render(vec2 uv)
{
  if (uv.x < 0.0 || uv.x > 30.0 || uv.y <= 0)
    return vec3(-100.0);

  /*float d = map(uv);
  d = uv.y < 0.0 ? -1.0 : d;
  vec3 col = vec3(d < 0.0 ? 1.0 : 0.0);*/

  vec4 col = getColor(uv);
 
  /*
  vec2 p = uv;
  vec2 e = vec2(EPSILON, 0.0);
  //vec2 n = vec2(map(p) - map(p - e.xy), map(p - e.yx));
  vec2 n = vec2(map(p)) - vec2(map(p - e.xy), map(p - e.yx));
  col = vec4(vec3(normalize(n), 0.0), -10.0);
  */

  // Paint walls
  // col = uv.y < 0.0 || uv.x < 0.0 || uv.x > 30.0 ? vec4(1.0, 1.0, 1.0, -1.0) : col;

  vec3 background = vec3(0.2 + 0.4 * mod(floor(uv.x) + floor(uv.y), 2.0));

#if SHADOWS_ENABLED
  if (col.w > 0)
  {

    float d = rayMarch(uv, directionalLightDirection.xy);
    return (d < FAR ? 0.3 : 1.0) * background;

    // float d = rayMarch(uv, normalize(vec2(15) - uv));
    // return (d < length(vec2(15) - uv) ? 0.3 : 1.0) * background;

    // return vec3(0.001 * d);
  }
#else
  col = col.w < 0 ? col : vec4(background, 1.0);
#endif

  return col.xyz;
}

float scale = 10.f;

void main()
{
  vec2 uv = (2.0 * gl_FragCoord.xy - u_resolution.xy) / u_resolution.y;
  vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
  vec2 pos = (-u_cameraMatrix[3].z * uv) - u_cameraMatrix[3].xy;

  float aux = shape_circle(0.01 * pos - vec2(0));
  if(u_time > 2.0)
    aux = 0.0;

  //float distance = 0.5 * round(texture(u_colorTexture, gl_FragCoord.xy).r) + clamp(0.1 * map(pos),0.0,1.0);
  float distance = map(pos);
  float distanceScaled = (distance / scale) + 0.5f;
  distanceScaled =  clamp(distanceScaled, 0.0, 1.0);

  float prevD = texture(u_colorTexture, screenUV.xy).r;
  float d = min( prevD + 0.003, distanceScaled);
  // float d = mix(1.1 * texture(u_colorTexture, gl_FragCoord.xy).r, distanceScaled, 0.5);


  
  //FragColor = aux + prevD;//+ texture(u_colorTexture, gl_FragCoord.xy).r + (0.01 * gl_FragCoord.x / u_resolution.x);
  FragColor = d;
}
