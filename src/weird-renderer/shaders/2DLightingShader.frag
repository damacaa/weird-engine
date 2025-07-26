#version 330 core

#define SHADOWS_ENABLED
// #define SOFT_SHADOWS
// #define DITHERING

// #define DEBUG_SHOW_DISTANCE
// #define DEBUG_SHOW_COLORS
// #define DEBUG_SHOW_AO // Uncomment to visualize Ambient Occlusion directly

// Constants
const int MAX_STEPS = 1000; // Max raymarch steps (original was 1000, reduced for general performance)
const float SDF_EPSILON = 0.001; // Epsilon for surface detection (objects are where map(p) <= SDF_EPSILON)
const float NORMAL_CALC_EPS = 0.001; // Epsilon for calculating normals from the SDF

// Raymarch limits (from original shader)
const float NEAR = 0.0f; // Start distance for raymarch (original was 0.1f)
const float FAR = 1.2f;  // Max distance for raymarch

// Outputs to the framebuffer
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader (Note: In this 2D SDF context, most are unused)
in vec3 v_worldPos; // Typically for 3D world coordinates
in vec3 v_normal;   // Typically for 3D surface normals
in vec3 v_color;    // Typically for vertex colors
in vec2 v_texCoord; // Fragment UV coordinates, used as 2D position for SDF

// Uniforms
uniform mat4 u_camMatrix; // Typically for camera transformation in 3D
uniform vec2 u_resolution;
uniform float u_time;

uniform sampler2D t_colorTexture;      // Texture storing object base colors (and alpha for existence)
uniform sampler2D t_distanceTexture;   // Texture storing the Signed Distance Field (SDF)
uniform sampler2D t_backgroundTexture; // Texture for the background image

// Directional light direction (original uniform, used for shadows and new lighting)
uniform vec2 u_directionalLightDirection = vec2(0.7071f, 0.7071f); // Normalized light direction

#ifdef DITHERING

uniform float u_spread = .025;
uniform int u_colorCount = 16;


// Bayer matrices for dithering (copied as-is from original)
uniform int u_bayer2[2 * 2] = int[2 * 2](0, 2, 3, 1);
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

float Getu_bayer2(int x, int y) { return float(u_bayer2[(x % 2) + (y % 2) * 2]) * (1.0f / 4.0f) - 0.5f; }
float Getu_bayer4(int x, int y) { return float(u_bayer4[(x % 4) + (y % 4) * 4]) * (1.0f / 16.0f) - 0.5f; }
float Getu_bayer8(int x, int y) { return float(u_bayer8[(x % 8) + (y % 8) * 8]) * (1.0f / 64.0f) - 0.5f; }
#endif

// Samples the Signed Distance Field texture at a given 2D point 'p'.
// Clamps 'p' to [0,1] range to avoid out-of-bounds texture sampling,
// which can be an issue when calculating normals near scene edges.
float map(vec2 p) {
    p = clamp(p, vec2(0.0), vec2(1.0));
    return texture(t_distanceTexture, p).x;
}

// Calculates the 2D surface normal from the SDF using finite differences.
// 'NORMAL_CALC_EPS' determines the small offset used for sampling the SDF.
vec2 getNormal(vec2 p) {
    vec2 offset = vec2(NORMAL_CALC_EPS, 0.0);
    return normalize(vec2(
    map(p + offset.xy) - map(p - offset.xy),
    map(p + offset.yx) - map(p - offset.yx)
    ));
}

// Performs a 2D raymarch from a ray origin 'ro' in a direction 'rd'.
// It returns the total distance traveled along the ray, and populates 'minDistance'
// with the closest distance found to any object during the march.
float rayMarch(vec2 ro, vec2 rd, out float minDistance) {
    float d_current_step;
    minDistance = 10000.0; // Initialize with a very large distance

    float traveled = 0.0;

    for (int i = 0; i < MAX_STEPS; i++) {
        vec2 p = ro + (traveled * rd);

        // If the ray goes out of the [0,1] UV bounds, it's considered to hit nothing in the scene.
        if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) {
            return FAR; // Ray went out of scene bounds
        }

        d_current_step = map(p); // Distance from current ray point to the nearest object

        minDistance = min(d_current_step, minDistance); // Keep track of the closest distance found

        if (d_current_step <= SDF_EPSILON) { // Ray hit an object (distance is very small or negative)
            break;
        }

        traveled += 0.001; // Advance ray by the distance to the nearest object

        if (traveled >= FAR) { // Ray traveled beyond maximum range without hitting an object
            return FAR;
        }
    }
    return traveled; // Return total distance traveled (or FAR if no hit)
}

// Calculates the direct shadow factor for a given UV coordinate based on original shader's logic.
// This function determines how much direct light reaches the pixel.
float calculateDirectShadow(vec2 uv, vec2 lightDir) {
    #ifdef SHADOWS_ENABLED
    vec2 rd = lightDir; // Use the provided directional light vector

    float d_at_uv = map(uv); // Distance at the current pixel's position
    float minD_shadow_ray; // Will hold the minimum distance found by the shadow ray

    // Offset position slightly towards the light. This is part of the original shader's
    // heuristic for self-shadowing/depth effects on objects.
    vec2 offsetPosition_for_self_shadow = uv + (2.0 / u_resolution) * rd;

    if (d_at_uv <= SDF_EPSILON) { // If the current pixel is inside or on the surface of an object
        // This branch computes a custom self-shadowing/interior darkening effect.
        float distanceFallof = 1.25;
        float maxDistance = 0.05;
        // 'dd' increases as 'd_at_uv' becomes more negative (i.e., deeper inside the object).
        float dd = mix(0.0, 1.0, -(distanceFallof * d_at_uv));
        dd = min(maxDistance, dd);

        // Raymarch from the offset position towards the light.
        // This part checks if external geometry is blocking light to this point.
        float shadow_ray_travel = rayMarch(offsetPosition_for_self_shadow, rd, minD_shadow_ray);

        // If the shadow ray hits something (shadow_ray_travel < FAR):
        // The light factor is 1.0 minus a factor related to the interior depth ('dd').
        // This makes deeper points darker.
        // If the shadow ray hits nothing (shadow_ray_travel >= FAR):
        // The original shader returns 2.0, potentially making unshadowed interiors brighter than 1.0.
        return shadow_ray_travel < FAR ? 1.0 : 1.0;
    } else { // Current pixel is outside an object
        // This is a more standard direct shadow raymarch from the current pixel towards the light.
        float shadow_ray_travel = rayMarch(uv, rd, minD_shadow_ray);

        #ifdef SOFT_SHADOWS
        // Soft shadows: Blend between 0.85 (shadowed) and 1.0 (fully lit)
        // based on how far the ray traveled before hitting an occluder.
        return mix(0.85 , 1.0, shadow_ray_travel / FAR);
        #else
        // Hard shadows: If minD_shadow_ray is very small (ray hit an occluder),
        // the factor approaches 0 (dark shadow). If large (no hit), it approaches 1 (fully lit).
        return min(100.0 * minD_shadow_ray, 1.0);
        #endif
    }
    #else // SHADOWS_ENABLED not defined
    return 1.0; // If shadows are disabled, return full light
    #endif
}

// Calculates Ambient Occlusion (AO), a local approximation of Global Illumination.
// It estimates how much a point on a surface is occluded by nearby geometry,
// making crevices darker. This uses the SDF for local scene information.
float calculateAmbientOcclusion(vec2 p, vec2 n) {
    float ao = 0.0;
    int ao_samples = 8; // Number of samples taken along the normal
    float ao_radius = 0.1; // Maximum distance to check for occlusion
    float ao_strength = 1.0; // Overall strength multiplier for the AO effect

    // Sample along the normal direction at increasing distances from the point 'p'.
    for (int i = 0; i < ao_samples; ++i) {
        // Calculate the sample point's distance from 'p'
        float sample_dist = ao_radius * (float(i) + 0.5) / float(ao_samples);
        // Get the SDF value at the sample point (p + n * sample_dist)
        float d = map(p + n * sample_dist);

        // If 'd' (distance to surface from sample point) is less than 'sample_dist',
        // it means there's geometry within the sample range, contributing to occlusion.
        // 'max(0.0, sample_dist - d)' calculates the "overlap" or "penetration" amount.
        ao += max(0.0, sample_dist - d); // Accumulate the occlusion contribution
    }

    // Normalize 'ao' and invert it to get an occlusion factor:
    // 1.0 means no occlusion, 0.0 means full occlusion.
    return 1.0 - clamp(ao_strength * ao / (ao_radius * float(ao_samples)), 0.0, 1.0);
}


void main() {
    vec2 screenUV = v_texCoord; // Current fragment's UV coordinate
    vec4 objectColorSample = texture(t_colorTexture, screenUV); // Base color and alpha of the object
    vec3 objectBaseColor = objectColorSample.rgb;

    float current_pixel_distance = map(screenUV); // SDF distance at this fragment's position

    vec3 finalColor;

    // Anti-aliasing / Edge blending:
    // `surfaceAlpha` controls how much the object's shaded color blends with the background.
    // It creates a smooth transition zone at the object's edge based on SDF distance.
    // Pixels inside or exactly on the surface will have high alpha (close to 1.0).
    // Pixels just outside the surface will blend from 1.0 to 0.0 over `aa_blend_width`.
    float aa_blend_width = 1.0 / u_resolution.x; // Defines the width of the blend zone in UV space
    float surfaceAlpha = 1.0 - smoothstep(-aa_blend_width, aa_blend_width, current_pixel_distance);

    // Final blend: Mix the calculated object color (lit/occluded) with the background color
    // based on the `surfaceAlpha` calculated from the SDF distance.
    objectBaseColor = mix(vec3(1.0), objectBaseColor, surfaceAlpha);

    // Check if this pixel belongs to an actual object drawn on the color texture.
    // The alpha channel of `t_colorTexture` determines if there's an object present.

        // This pixel is on an object's surface or inside it. Apply shading.
        vec2 normal = getNormal(screenUV); // Calculate the 2D surface normal

        // --- Direct Lighting Calculation ---
        float directLightFactor = 1.0 * calculateDirectShadow(screenUV, u_directionalLightDirection); // Get shadow factor
        // CORRECTED: Explicitly construct vec3 from vec2 and 0.0 before normalizing
        vec3 lightDirection = normalize(vec3(u_directionalLightDirection, 0.0)); // Normalized direction to the light
        float diffuse = current_pixel_distance < 0.0 ? max(0.0, dot(normal, lightDirection.xy)) : 1.0; // 2D diffuse component (light hitting surface)

        // Base lighting: simple combination of ambient and diffuse illumination
        vec3 litColor = objectBaseColor * (0.1 + 0.9 * diffuse); // 0.1 ambient, 0.9 diffuse strength

        // Apply the direct light shadow factor to the lit color
        // litColor *= directLightFactor;

        // --- Global Illumination Approximation (Ambient Occlusion) ---
        float ambientOcclusion = current_pixel_distance < 0.0 ? calculateAmbientOcclusion(screenUV, normal) : 1.0;
        litColor *=  ambientOcclusion ; // Apply AO as a darkening factor to simulate self-shadowing in crevices

        finalColor = litColor;

        #ifdef DEBUG_SHOW_AO
        // If DEBUG_SHOW_AO is enabled, visualize the AO factor directly (grayscale)
        finalColor = vec3(ambientOcclusion);
        #endif





    // --- Debug Modes (Original shader's debug logic preserved) ---
    #ifdef DEBUG_SHOW_DISTANCE
    // Visualizes the SDF distance as a striped pattern
    float debug_value = 0.5 * (cos(500.0 * current_pixel_distance) + 1.0);
    debug_value = debug_value * debug_value * debug_value;
    vec3 debug_color = current_pixel_distance > 0 ? mix(vec3(1), vec3(0.2), debug_value) :                    // Outside: white to dark grey stripes
    (current_pixel_distance + 1.0) * mix(vec3(1.0, 0.2, 0.2), vec3(0.1), debug_value); // Inside: red to darker red stripes
    FragColor = vec4(debug_color, 1.0);
    return; // Exit shader if debug mode is active
    #endif

    #ifdef DEBUG_SHOW_COLORS
    // If enabled, simply shows the base object color, bypassing all lighting/shadows.
    FragColor = vec4(objectBaseColor, 1.0);
    return; // Exit shader if debug mode is active
    #endif

    // --- Dithering and Posterizing (Original shader's logic preserved) ---
    // Applies dithering to reduce color banding and create a "pixelated" or "posterized" look.
    #ifdef DITHERING
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    finalColor = finalColor + u_spread * Getu_bayer4(x, y); // Adds Bayer matrix noise
    // Posterizes colors to a fixed number of steps
    finalColor.r = floor((u_colorCount - 1.0f) * finalColor.r + 0.5) / (u_colorCount - 1.0f);
    finalColor.g = floor((u_colorCount - 1.0f) * finalColor.g + 0.5) / (u_colorCount - 1.0f);
    finalColor.b = floor((u_colorCount - 1.0f) * finalColor.b + 0.5) / (u_colorCount - 1.0f);
    #endif

    // Final fragment color output
    FragColor = vec4(finalColor, 1.0);
}