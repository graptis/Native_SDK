#version 450 core
const int COLOR_ARRAY_SIZE = 11;

layout(location = 0) in highp vec3	myVertex;

layout(std140, set = 0, binding = 0) uniform DynamicData
{
        uniform highp mat4 transform;
        uniform highp mat4 viewMatrix;
        uniform highp vec3 lightDir;
};

layout(std140, set = 1, binding = 0) uniform StaticData
{
        uniform mat4 shadowMatrix;
};

layout (push_constant) uniform push_constant
{
    lowp vec4 myColour;
} pushConstant;


void main(void)
{
	vec4 worldPos = shadowMatrix * vec4(myVertex, 1.0);
	worldPos.y += 0.0001;
	gl_Position = transform * worldPos;
}