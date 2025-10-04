#version 330 core

// Constants

// Outputs u_staticColors in RGBA
layout(location = 0) out vec4 FragColor;

// Inputs from vertex shader
in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_texCoord;

// Uniforms
uniform vec2 u_resolution;
uniform float u_time;
uniform sampler2D t_distanceTexture;

float sampleSign(vec2 uv) {
    return sign(texture(t_distanceTexture, uv).r);
}

vec2 computeGradient(vec2 uv, vec2 texelSize) {
    float dx = texture(t_distanceTexture, uv + vec2(texelSize.x, 0)).r -
    texture(t_distanceTexture, uv - vec2(texelSize.x, 0)).r;

    float dy = texture(t_distanceTexture, uv + vec2(0, texelSize.y)).r -
    texture(t_distanceTexture, uv - vec2(0, texelSize.y)).r;

    return normalize(vec2(dx, dy));
}

float raycastDistance(vec2 uv, vec2 dir, vec2 texelSize, int maxSteps, float stepSize) {
    float signStart = sampleSign(uv);
    vec2 cur = uv;
    for (int i = 0; i < maxSteps; i++) {

        vec2 dir2 = computeGradient(cur, texelSize);
        cur += -signStart *dir2 * stepSize * texelSize; // march
        float s = sampleSign(cur);
        if (s != signStart) {
            // linear interpolation between last two steps for better precision
            return length((cur - uv) / texelSize) * stepSize;
        }
    }
    return float(maxSteps) * stepSize; // no intersection found
}

void main() {

    vec2 screenUV = v_texCoord;
    vec4 color = texture(t_distanceTexture, screenUV);
    float distance = color.x;

    if(distance > 0.0)
    {
        FragColor = color;
        return;
    }



    vec2 texSize = textureSize(t_distanceTexture, 0);
    vec2 texelSize = 1.0 / texSize;

    float signHere = sampleSign(v_texCoord);
    vec2 grad = computeGradient(v_texCoord, texelSize);

    // march in both directions to catch sign flips in either direction
    float d1 = raycastDistance(v_texCoord, -signHere * grad, texelSize, 1000, 0.5);

    float realDist = d1 * signHere;

    FragColor = vec4(realDist, grad * 0.5 + 0.5, 1.0);
}

/*void main()
{
    vec2 screenUV = v_texCoord;
    vec4 color = texture(t_distanceTexture, screenUV);
    float distance = color.x;

    float realDistance = distance;
    FragColor = vec4(realDistance, color);
}*/