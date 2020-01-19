#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 tangent;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	mat4 proj;
	mat4 view;
} pushConsts;


layout (location = 1) out vec3 fragPos;


void main() 
{
	fragPos = vec3( vec4(pos.xyz, 1.0));
	//fragPos.y = -fragPos.y;
	gl_Position = pushConsts.proj * pushConsts.view * vec4(pos.xyz, 1.0);
}