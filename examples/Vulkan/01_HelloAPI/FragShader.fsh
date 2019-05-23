#version 320 es

//// Shader Resources ////
layout(set = 0, binding = 0) uniform mediump sampler2D triangleTexture;

//// Vertex Inputs ////
layout(location = 0) in mediump vec2 UV;

//// Fragment Outputs ////
layout(location = 0) out mediump vec4 fragColor;

void main()
{
	// Sample the checkerboard texture and write to the framebuffer attachment
	fragColor = texture(triangleTexture, UV);
}