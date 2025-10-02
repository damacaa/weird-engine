#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

in vec2 v_texCoord;

uniform mat4 u_camMatrix;
uniform float u_fov;
uniform vec2 u_resolution;

uniform float gridScale = 1.0;    // Size of each grid cell in world units
uniform vec3 gridColor = vec3(1);     // Color of the grid lines
uniform vec3 backgroundColor = vec3(0);
uniform float lineThickness = 0.2; // Thickness of the grid lines


// ChatGPT: write a glsl functio to transform 3D cartesian coordinates to spherical
vec3 cartesianToSpherical(vec3 v) 
{
    float r = length(v);
    float theta = atan(v.z, v.x);
    float phi = acos(v.y / r);
    return vec3(r, theta, phi);
}

float creteGrid(vec3 sphericalCoord, float frequency, float opacity, float threshold)
{
    float latLines = 0.5 * (sin(frequency * sphericalCoord.y) + 1.0);
    float lonLines = 0.5 * (sin(frequency * sphericalCoord.z) + 1.0);

    float grid = max(latLines, lonLines);

    // Anti-aliased line thickness
    float aa = 0.01;
    grid = smoothstep(threshold - aa, threshold + aa, grid) * opacity;

    return grid;
}

void main()
{          
    // Bring 0,0 to the center of the screen
    vec2 uv = (2.0 * v_texCoord) - 1.0;
    float aspectRatio = u_resolution.x / u_resolution.y;
    uv.x *= aspectRatio;

    // Calculate the direction of a ray that goes from origin towards the frag coord
    // This formula is often used in ray marching, I don't know where I got it for the first 
    // time at this point
    vec3 rd = (vec4(normalize(vec3(uv, 2.0 * u_fov)), 0.0) * u_camMatrix).xyz;

    // Tranform ray direction to spherical coords
    vec3 sphericalCoord = cartesianToSpherical(rd);

    // Calculate two grids of different sizes and opacities
    float grid = creteGrid(sphericalCoord, 100.0, 0.25, 0.9999);
    float grid2 = creteGrid(sphericalCoord, 500.0, 0.05, 0.9);

    // Combine grids
    grid = (grid + grid2);

    // Optional: fade near poles (poles look ugly with this method)
    float poleMask = pow(sin(sphericalCoord.z), 5.0);

    // Color
    vec3 color = vec3(grid) * poleMask;

    FragColor = vec4(color, 1.0);

}
