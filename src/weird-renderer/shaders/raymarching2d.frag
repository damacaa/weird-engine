#version 330 core


out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform sampler2D u_diffuse;
uniform sampler2D u_specular;

uniform vec4  u_lightColor;
uniform vec3  u_lightPos;
uniform vec3  u_camPos;

uniform float u_time;

// Custom

#define BLEND_SHAPES 0
#define MOTION_BLUR 1

uniform float u_k = 0.25;
// Uniforms
uniform sampler2D t_colorTexture;

uniform int u_loadedObjects;
uniform samplerBuffer t_shapeBuffer;

uniform vec2 u_resolution;

uniform mat4 u_camMatrix;
uniform vec3 u_staticColors[16];
uniform vec3 u_directionalLightDirection;

uniform int u_blendIterations;

uniform int u_customShapeCount;

uniform float u_uiScale = 50.0f;

// Constants
const int MAX_STEPS = 100;
const float EPSILON = 0.01;
const float NEAR = 0.1f;
const float FAR = 100.0f;

// Custom shape variables
#define var8 u_time
#define var9 p.x
#define var10 p.y
#define var11 u_uiScale * uv.x
#define var12 u_uiScale * uv.y

#define var0 parameters0.x
#define var1 parameters0.y
#define var2 parameters0.z
#define var3 parameters0.w
#define var4 parameters1.x
#define var5 parameters1.y
#define var6 parameters1.z
#define var7 parameters1.w

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

float smin(float a, float b, float u_k)
{
  float h = clamp(0.5 + 0.5 * (b - a) / u_k, 0.0, 1.0);
  return mix(b, a, h) - u_k * h * (1.0 - h);
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

vec3 draw_line(float d, float thicu_kness)
{
  const float aa = 3.0;
  return vec3(smoothstep(0.0, aa / u_resolution.y, max(0.0, abs(d) - thicu_kness)));
}

vec3 draw_line(float d)
{
  return draw_line(d, 0.0025);
}

vec3 getMaterial(vec2 p, int materialId)
{
  return u_staticColors[materialId];
}

vec4 getColor(vec2 p, vec2 uv)
{
    float minDist = 100000.0;

    vec3 col = vec3(0.0);
    float minZ = 1000.0f;

    float zoom = -u_camMatrix[3].z;

    float aspectRatio = u_resolution.x / u_resolution.y;
    vec2 zoomVec = vec2((zoom * aspectRatio) - 1.0, zoom);

    bool bestIsScreenSpace = false;

    /*ADD_SHAPES_HERE*/

    if(minDist < EPSILON)
    {
        return vec4(col, minDist);
    }

    for (int i = 0; i < u_loadedObjects - (2 * u_customShapeCount); i++)
    {
        vec4 positionSizeMaterial = texelFetch(t_shapeBuffer, i);
        int materialId = int(positionSizeMaterial.w);
        // vec4 extraParameters = texelFetch(t_shapeBuffer, (2 * i) + 1);

        float z = positionSizeMaterial.z;

        float objectDist = shape_circle(p - positionSizeMaterial.xy);

        #if BLEND_SHAPES

        minDist = fOpUnionSoft(objectDist, minDist, u_k);
        float delta = 1 - (max(u_k - abs(objectDist - minDist), 0.0) / u_k); // After new d is calculated
        col = mix(getMaterial(p, materialId), col, delta);

        #else

        if(objectDist < minDist)
        {
            minDist = objectDist;
            col = getMaterial(positionSizeMaterial.xy, materialId);
            minZ = z;

            if(minDist < EPSILON)
            {
                break;
            }
        }

        #endif

    }

    // Set bacu_kground color
    // vec3 bacu_kground = mix(u_staticColors[2], u_staticColors[3], mod(floor(.1 * p.x) + floor(.1 * p.y), 2.0));
    float pixel = 0.2 / u_resolution.y;
    vec3 background = mix(u_staticColors[3], u_staticColors[2], min(fract(0.1 * p.x), fract(0.1 * p.y)) > pixel * zoom ? 1.0 : 0.0);
    col = minDist > 0.0 ? background : col;

    return vec4(col, minDist);
}

void main()
{
  // FragColor = vec4(u_customShapeCount);
  // return;
  
  vec2 uv = (2.0f * v_texCoord) - 1.0f;

  float zoom = -u_camMatrix[3].z;
  vec2 pos = (zoom * uv) - u_camMatrix[3].xy;

  vec4 color = getColor(pos, v_texCoord); // Same as uv but (0, 0) is bottom left corner
  float distance = color.w;

  float finalDistance = 0.6667 * 0.5 * distance / zoom;

#if MOTION_BLUR

  vec2 screenUV = v_texCoord;
  vec4 previousColor = texture(t_colorTexture, screenUV.xy);
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
