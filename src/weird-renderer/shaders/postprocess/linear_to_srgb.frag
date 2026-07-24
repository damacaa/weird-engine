#version 300 es
precision highp float;
in vec2 v_texCoord;
out vec4 FragColor;
uniform sampler2D t_input;
void main()
{
	vec4 col = texture(t_input, v_texCoord);
	FragColor = vec4(pow(col.rgb, vec3(0.4545)), col.a);
}