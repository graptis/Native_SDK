#version 450

#define VERTEX_ARRAY 0
#define NORMAL_ARRAY 1
#define TEXCOORD_ARRAY 2
#define TANGENT_ARRAY 3

layout(location = VERTEX_ARRAY) in highp vec3 inVertex;
layout(location = NORMAL_ARRAY) in highp vec3 inNormal;
layout(location = TEXCOORD_ARRAY) in mediump vec2 inTexCoords;
layout(location = TANGENT_ARRAY) in highp vec3 inTangent;

layout(set = 1, binding = 1) uniform DynamicsPerModel
{
	highp mat4 mWorldViewProjectionMatrix;
	highp mat4 mWorldViewMatrix;
	highp mat4 mWorldViewITMatrix;
};

layout (location = 0) out mediump vec2 vTexCoord;
layout (location = 1) out highp vec3 vNormal;
layout (location = 2) out highp vec3 vTangent;
layout (location = 3) out highp vec3 vBinormal;
layout (location = 4) out highp vec3 vViewPosition;

void main() 
{
	gl_Position = mWorldViewProjectionMatrix * vec4(inVertex, 1.0);

	// Transform normal from model space to eye space
	vNormal = mat3(mWorldViewITMatrix) * inNormal;
	vTangent = mat3(mWorldViewITMatrix) * inTangent;
	vBinormal = cross(vNormal, vTangent);

	// Pass the vertex position in view space for depth calculations
	vViewPosition = (mWorldViewMatrix * vec4(inVertex, 1.0)).xyz;

	// Pass the texture coordinates to the fragment shader
	vTexCoord = inTexCoords;				
}