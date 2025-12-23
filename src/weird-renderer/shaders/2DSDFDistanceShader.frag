#version 330 core

#include "shapes.glsl"

// #define BLEND_SHAPES
// #define MOTION_BLUR

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
uniform mat4 u_oldCamMatrix;
uniform vec3 u_camPositionChange;
uniform vec3 u_staticColors[16];
uniform vec3 u_directionalLightDirection;

uniform int u_blendIterations;
uniform float u_deltaTime;

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


vec3 getColor(vec2 p, vec2 uv)
{
    float minDist = 100000.0;
    float minColorDist = minDist;

    int finalMaterialId = 0;
    float mask = 0.0;

    #include "custom_shapes"

    if (minDist <= 0.0)
    {
        // return vec3(minDist, finalMaterialId, 0.0);
    }

    minColorDist = minDist;

    // float shapeDist = minDist;
    // minDist = 1.0; // Disable blending between balls and shapes

    float inv_k = 1.0 / u_k;

    for (int i = 0; i < u_loadedObjects - (2 * u_customShapeCount); i++)
    {
        vec4 positionSizeMaterial = texelFetch(t_shapeBuffer, i);
        int materialId = int(positionSizeMaterial.w);

        #ifdef ORIGIN_AT_BOTTOM_LEFT
        float objectDist = shape_circle(p - positionSizeMaterial.xy, 5.0);
        #else
        float objectDist = shape_circle(p - positionSizeMaterial.xy);
        #endif

        // Inside ball mask is set to 0
        mask = objectDist <= 0 ? 0.95 : mask;

        #ifdef BLEND_SHAPES

        finalMaterialId = objectDist <= minColorDist ? materialId : finalMaterialId;

        #else

        finalMaterialId = objectDist <= minDist ? materialId : finalMaterialId;

        #endif


        #ifdef BLEND_SHAPES

        minDist = fOpUnionSoft(objectDist, minDist, u_k, inv_k);
        minColorDist = min(minColorDist, objectDist);

        #else

        minDist = min(minDist, objectDist);

        #endif

    }

    // minDist = min(minDist, shapeDist);

    return vec3(minDist, max(finalMaterialId, 0), mask);
}

// Bilinear
vec2 smoothSample(sampler2D tex, vec2 uv)
{
    vec2 texelSize = 1.0 / u_resolution;

    vec2 P = uv * u_resolution - 0.5;

    // 3. Get the integer indices of the top-left texel (i, j)
    ivec2 i_j = ivec2(floor(P));

    // 4. Calculate the fractional distance (weights) from the top-left center
    // This gives the interpolation factors (fx, fy) between 0.0 and 1.0
    vec2 f = fract(P);

    // 5. Calculate the indices for the four texels (i, j), (i+1, j), (i, j+1), (i+1, j+1)
    // We add 0.5 to center the sampling point inside the texel
    vec2 uv00 = (vec2(i_j) + 0.5) * texelSize;
    vec2 uv10 = (vec2(i_j) + vec2(1.5, 0.5)) * texelSize;
    vec2 uv01 = (vec2(i_j) + vec2(0.5, 1.5)) * texelSize;
    vec2 uv11 = (vec2(i_j) + vec2(1.5, 1.5)) * texelSize;

    // 6. Sample the four texels (d00, d10, d01, d11)
    vec2 d00 = texture(tex, uv00).xy;
    float d10 = texture(tex, uv10).x;
    float d01 = texture(tex, uv01).x;
    float d11 = texture(tex, uv11).x;

    // 7. Perform the Bilinear Interpolation
    float d_top = mix(d00.x, d10, f.x);
    float d_bottom = mix(d01, d11, f.x);
    float d = mix(d_top, d_bottom, f.y);

    return vec2(d, d00.y);
}


void main()
{
    #ifdef ORIGIN_AT_BOTTOM_LEFT
    vec2 uv = v_texCoord;
    #else
    vec2 uv = (2.0f * v_texCoord) - 1.0f;
    #endif

    float aspectRatio = u_resolution.x / u_resolution.y;// TODO: uniform
    uv.x *= aspectRatio;

    float zoom = -u_camMatrix[3].z;
    vec2 pos = (zoom * uv) - u_camMatrix[3].xy;

    vec3 result = getColor(pos, v_texCoord);
    float d = result.x;

    float finalDistance = d / zoom;
    finalDistance *= 0.5 / aspectRatio;

    float material = result.y;
    float mask = result.z;

    #ifdef MOTION_BLUR

    // Compensate camera motion to achieve screen space motion blur
    // TODO: implement zoom compensation
    vec2 previousDistanceOffset = 0.5 * u_camPositionChange.xy / zoom;
    previousDistanceOffset.x /= aspectRatio;

    // Sample previous texture with bilinear filtering because aliasing causes this effect to accumulate error
    vec2 previousData = smoothSample(t_colorTexture, v_texCoord.xy + previousDistanceOffset);
    float previousDistance = previousData.x;
    float previousMaterial = texture(t_colorTexture, v_texCoord.xy).y;

    // Different material for object trail?
    material = finalDistance > 0.0 && previousDistance < 0.0 ? previousMaterial : material;



    #ifdef ORIGIN_AT_BOTTOM_LEFT
    // finalDistance = step(previousDistance, finalDistance - previousDistance);
    float distanceChange = finalDistance - previousDistance;
    distanceChange *= distanceChange > 0.0 ? 0.25 : 0.5;
    finalDistance = previousDistance + (distanceChange * u_deltaTime * 50.0);
    finalDistance = clamp(finalDistance, -1.0, 1.0);

    #else
    previousDistance += 20000.0 / zoom * u_deltaTime * (abs(previousDistance * previousDistance) + 0.0001);
    // previousDistance += u_blendIterations * 0.00035;
    // previousDistance = mix(finalDistance, previousDistance, 0.99);
    // previousDistance = min(previousDistance + (u_blendIterations * 0.00035), mix(finalDistance, previousDistance, 0.9));

    // IDEA! add noise for particles!
    // float distanceFalloff = 10.0 * pow(previousDistance + 0.001, 2);
    // previousDistance += true ? distanceFalloff  : finalDistance - previousDistance;

    finalDistance = min(previousDistance, finalDistance);
    #endif


    #endif

    #ifdef ORIGIN_AT_BOTTOM_LEFT
    // float pixelSize = 0.3 / u_resolution.x;
    // finalDistance = abs(finalDistance - pixelSize) - (pixelSize);
    #endif

    FragColor = vec4(finalDistance, material, mask, 0);

    // This has no visual effect (multiplying by zero), but it forces
    // the compiler to keep u_time because it's used to calculate FragColor.
    FragColor.a += u_time * 0.001;
}
