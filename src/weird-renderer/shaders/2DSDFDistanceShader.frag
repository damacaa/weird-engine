#version 330 core

#define BLEND_SHAPES 1
#define MOTION_BLUR 1

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
uniform float u_k = 0.25;
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

vec3 getColor(vec2 p, vec2 uv)
{
    float minDist = 100000.0;
    int finalMaterialId = 0;
    float mask = 1.0;

    /*ADD_SHAPES_HERE*/

    if (minDist <= 0.0)
    {
        return vec3(minDist, finalMaterialId, 0.0);
    }

    float shapeDist = minDist;
    // minDist = 1.0; // Disable blending between balls and shapes

    float inv_k = 1.0 / u_k;

    for (int i = 0; i < u_loadedObjects - (2 * u_customShapeCount); i++)
    {
        vec4 positionSizeMaterial = texelFetch(t_shapeBuffer, i);
        int materialId = int(positionSizeMaterial.w);

        float objectDist = shape_circle(p - positionSizeMaterial.xy);

        // Inside ball mask is set to 0
        mask = objectDist <= 0 ? 0.25 : mask;

        finalMaterialId = objectDist <= minDist ? materialId : finalMaterialId;

        #if BLEND_SHAPES

        minDist = fOpUnionSoft(objectDist, minDist, u_k, inv_k);

        #else

        minDist = min(minDist, objectDist);

        if (minDist < EPSILON)
        {


            break;
        }

        #endif

    }

    minDist = min(minDist, shapeDist);

    return vec3(minDist, finalMaterialId, mask);
}

void main()
{
    // FragColor = vec4(u_customShapeCount);
    // return;

    vec2 uv = (2.0f * v_texCoord) - 1.0f;
    float aspectRatio = u_resolution.x / u_resolution.y;// TODO: uniform
    uv.x *= aspectRatio;

    float zoom = -u_camMatrix[3].z;
    vec2 pos = (zoom * uv) - u_camMatrix[3].xy;


    vec3 result = getColor(pos, v_texCoord);// Same as uv but (0, 0) is bottom left corner
    float distance = result.x;

    float finalDistance =  0.5 * distance / zoom;
    finalDistance *= 0.5 / aspectRatio;



    #if MOTION_BLUR

    vec2 screenUV = v_texCoord;
    vec4 previousColor = texture(t_colorTexture, screenUV.xy);

    float previousDistance = previousColor.x;
    int previousMaterial = int(previousColor.y);

    previousDistance += u_blendIterations * 0.00035;
    previousDistance = mix(finalDistance, previousDistance, 0.95);
    // previousDistance = min(previousDistance + (u_blendIterations * 0.00035), mix(finalDistance, previousDistance, 0.9));

    finalDistance = min(previousDistance, finalDistance);


    // FragColor = previousColor;

    // FragColor = mix(vec4(color.xyz, finalDistance), previousColor, 0.9);

    #else



    #endif

    FragColor = vec4(finalDistance, result.y, result.z, 0);

}
