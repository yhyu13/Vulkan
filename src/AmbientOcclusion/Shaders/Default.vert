#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;
layout (location = 2) in vec2 aUV;

layout ( binding = 0) uniform UBO 
{
	mat4 projView;

} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	int textureTypeCount;
} pushConsts;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outColor;

void main() 
{
	outUV = aUV;
	outColor = pushConsts.color;
	gl_Position = ubo.projView * pushConsts.model * vec4(aPos, 1.0);
}