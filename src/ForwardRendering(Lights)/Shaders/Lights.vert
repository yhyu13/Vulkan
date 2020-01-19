#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	vec3 viewPosition;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
} pushConsts;

layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec3 viewPos;
layout (location = 4) out vec3 normal;
layout (location = 5) out vec3 fragPos;

void main() 
{
	outUV = UV;
	outColor = pushConsts.color;
	viewPos = ubo.viewPosition;
	fragPos = vec3(pushConsts.model * vec4(pos.xyz, 1.0));
	normal = mat3(transpose(inverse(pushConsts.model))) * norm;
	gl_Position = ubo.projView * pushConsts.model * vec4(pos.xyz, 1.0);
	
}