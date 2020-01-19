#version 450

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	vec3 viewPosition;
} ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 viewPos;

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
		
}