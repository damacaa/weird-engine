#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform sampler2D t_scene;
uniform sampler2D t_lines;

const int NUM_STOPS = 4;
uniform vec3 colors[4] = vec3[](
    vec3(0.82, 0.74, 0.88),  // Muted lavender
    vec3(0.78, 0.94, 0.76),  // Pale pistachio
    vec3(0.98, 0.85, 0.72),  // Dusty pastel orange
    vec3(0.68, 0.96, 0.95)   // Soft cyan mist
);


uniform float stops[NUM_STOPS] = float[](0.0, 0.33, 0.66, 1.0);

vec3 getGradientColor(float t) 
{
    // This could be replace with a texture, but this approach makes it easier to iterate and choose colors
    for (int i = 0; i < NUM_STOPS - 1; ++i) {
        if (t >= stops[i] && t <= stops[i+1]) {
            float localT = (t - stops[i]) / (stops[i+1] - stops[i]);
            return mix(colors[i], colors[i+1], localT);
        }
    }

    return colors[NUM_STOPS - 1]; // fallback
}

void main()
{          
    vec2 uv = v_texCoord;

    vec4 scene = texture(t_scene, uv);
    vec4 lines = texture(t_lines, uv);

    vec3 sceneFilter = getGradientColor(scene.r);

    vec3 color = mix(vec3(0.576, 0.376, 0.729), sceneFilter, 1.0 - lines.a);

    FragColor = vec4(color, 1.0);
}
