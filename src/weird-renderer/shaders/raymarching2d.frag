#version 330 core

#define BLEND_SHAPES 1
#define MOTION_BLUR 1

uniform float k = 0.1;

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Uniforms
uniform sampler2D u_colorTexture;

uniform int u_loadedObjects;
uniform samplerBuffer u_shapeBuffer;

uniform vec2 u_resolution;
uniform float u_time;

uniform mat4 u_cameraMatrix;
uniform vec3 u_staticColors[16];
uniform vec3 directionalLightDirection;

uniform int u_blendIterations;

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
  float d = 100000.0;

  vec3 col = vec3(0.0);

  for (int i = 0; i < u_loadedObjects; i++)
  {
    vec4 positionAndMaterial = texelFetch(u_shapeBuffer, 2 * i);
    int materialId = int(positionAndMaterial.w);
    vec4 extraParameters = texelFetch(u_shapeBuffer, (2 * i) + 1);

    float objectDist = shape_circle((p - positionAndMaterial.xy) / (extraParameters.x));

#if BLEND_SHAPES

    d = fOpUnionSoft(objectDist, d, k);
    float delta = 1 - (max(k - abs(objectDist - d), 0.0) / k); // After new d is calculated
    col = mix(getMaterial(p, materialId), col, delta);

#else

    d = min(d, objectDist);
    col = d == objectDist ? getMaterial(positionAndMaterial.xy, materialId) : col;

#endif
  }

  // Wavy floor
  float floorDist = 1.0 * (p.y - 1.0 * sin(0.5 * p.x + 1.0f * u_time));
  d = min(d, floorDist);
  col = d == floorDist ? getMaterial(p, 0) : col;

  // Domain repetition balls
  float scale = 1.0 / 10.0;
  vec2 pp = p + 5.0;
  vec2 roundPos = ((scale * pp) - round(scale * pp)) * 10.0;
  roundPos.x += cos(u_time + round(0.1 * pp.x));
  roundPos.y += sin(u_time + round(0.1 * pp.x));

  // Remove repetition
  roundPos = p - vec2(25.0f, 30.0f);

  float infiniteShere = length(roundPos) - 5.0f;
  float displacement = 1.0 * sin(5.0f * atan(roundPos.y, roundPos.x) - 5.0f * u_time);


  d = min(d, infiniteShere + displacement);
  

  float d2 = 0.1f * sin(10.0*p.x) * sin(10.0 * p.y);
  //d += d2;
  
  //d = fOpUnionSoft(infiniteShere, d, k);
  col = d == (infiniteShere + displacement) ? getMaterial(p, 11) : col;

  // Set background color
  vec3 background = mix(u_staticColors[2], u_staticColors[3], mod(floor(.1 * p.x) + floor(.1 * p.y), 2.0));
  col = d > 0.0 ? background : col;

  return vec4(col, d);
}

void main()
{
  vec2 uv = (2.0 * gl_FragCoord.xy - u_resolution.xy) / u_resolution.y;
  float zoom = -u_cameraMatrix[3].z;
  vec2 pos = (zoom * uv) - u_cameraMatrix[3].xy;

  vec4 color = getColor(pos);
  float distance = color.w;

  float finalDistance = 0.6667 * 0.5 * distance / zoom;

#if MOTION_BLUR

  vec2 screenUV = (gl_FragCoord.xy / u_resolution.xy);
  vec4 previousColor = texture(u_colorTexture, screenUV.xy);
  float previousDistance = previousColor.w;

  previousDistance += u_blendIterations * 0.00035;
  previousDistance = mix(finalDistance, previousDistance, 0.95);
  // previousDistance = min(previousDistance + (u_blendIterations * 0.00035), mix(finalDistance, previousDistance, 0.9));

  FragColor = previousDistance < finalDistance ? vec4(previousColor.xyz, previousDistance) : vec4(color.xyz, finalDistance);
  if (FragColor.w > 0.0)
    FragColor = vec4(color.xyz, FragColor.w);

    // FragColor = previousColor;

    // FragColor = mix(vec4(color.xyz, finalDistance), previousColor, 0.9);

#else

  FragColor = vec4(color.xyz, finalDistance);

#endif
}
