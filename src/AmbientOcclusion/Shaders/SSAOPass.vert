#version 450

layout (location = 0) out vec2 outUV;
layout (location = 1) out mat4 projMatrix;
layout (location = 5) out vec4 viewPosition;
layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	mat4 viewMatrix;
	mat4 projMatrix;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	vec4 viewPosition;
} pushConsts;
void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
	projMatrix = ubo.projMatrix;
	viewPosition = pushConsts.viewPosition;
}