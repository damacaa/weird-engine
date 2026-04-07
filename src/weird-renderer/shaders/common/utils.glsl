vec3 toLinear(vec3 srgb)
{
	return pow(max(srgb, vec3(0.0)), vec3(2.2));
}

vec3 toSRGB(vec3 linear)
{
	return pow(max(linear, vec3(0.0)), vec3(1.0 / 2.2));
}

float hash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}