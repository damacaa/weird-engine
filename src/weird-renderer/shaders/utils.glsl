vec3 toLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));// or use inverse gamma
}
vec3 toSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}