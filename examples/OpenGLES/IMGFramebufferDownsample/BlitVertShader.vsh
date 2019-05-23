#version 310 es

out mediump vec2 TexCoord;

void main()
{
	highp vec2 texcoord = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
	TexCoord = texcoord;
	gl_Position = vec4(texcoord * 2.0 + -1.0, 0.0, 1.0);
}