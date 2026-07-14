#version 300 es
precision highp float;
in vec2 v_texCoord;
out vec4 FragColor;

uniform sampler2D t_distanceTexture;
uniform sampler2D t_textSdfTexture;
uniform mat4 u_camMatrix;
uniform vec2 u_resolution;
uniform vec2 u_textPos;
uniform vec2 u_textSize;
uniform int u_materialId;
uniform float u_overscan;

void main() {
    vec2 uv = (2.0 * v_texCoord) - 1.0;
    uv *= (1.0 + u_overscan);
    float aspectRatio = u_resolution.x / u_resolution.y;
    uv.x *= aspectRatio;
    
    float zoom = -u_camMatrix[3].z;
    vec2 pos = (zoom * uv) - u_camMatrix[3].xy;
    
    vec2 textUv = (pos - u_textPos) / u_textSize;
    
    textUv.y = 1.0 - textUv.y;
    
    vec4 current = texture(t_distanceTexture, v_texCoord);
    float minDist = current.x;
    float material = current.y;
    float mask = current.z;
    
    if (textUv.x >= 0.0 && textUv.x <= 1.0 && textUv.y >= 0.0 && textUv.y <= 1.0) {
        float textDist = texture(t_textSdfTexture, textUv).r;
        textDist = (128.0/255.0 - textDist) * u_textSize.y * 0.1; // approximate distance
        
        // Convert to normalized distance
        float finalDist = textDist / zoom;
        finalDist *= 0.5 / aspectRatio;
        
        if (finalDist < minDist) {
            minDist = finalDist;
            material = float(u_materialId);
        }
    }
    
    FragColor = vec4(minDist, material, mask, 0.0);
}
