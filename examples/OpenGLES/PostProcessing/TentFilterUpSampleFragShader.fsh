#version 310 es

layout(binding = 0) uniform mediump sampler2D sCurrentBlurredImage;
layout(binding = 1) uniform mediump sampler2D sDownsampledCurrentMipLevel;

layout(location = 0) in mediump vec2 vTexCoords[9];
layout(location = 0) out mediump float oColor;

const mediump float weights[9] = float[9](0.25, 0.0625, 0.125, 0.0625, 0.125, 0.0625, 0.125, 0.0625, 0.125);

void main()
{
	mediump float sum = texture(sDownsampledCurrentMipLevel, vTexCoords[0]).r * weights[0];
	
	for(int i = 1; i < 9; ++i)
	{
		sum += texture(sCurrentBlurredImage, vTexCoords[i]).r * weights[i];
	}

	oColor = sum;
}