#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 color;
layout (location = 4) in vec3 tangent;

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	vec3 viewPosition;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	int textureCount;
	int debugValue;
} pushConsts;

layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec3 viewPos;
layout (location = 4) out vec3 fragPos;
layout (location = 5) out vec3 tang;
layout (location = 6) out flat int tex;

void main() 
{

	outUV = UV;
	outColor = pushConsts.color;
	viewPos = ubo.viewPosition;
	fragPos = vec3(pushConsts.model * vec4(pos.xyz, 1.0));
	fragPos.y = -fragPos.y;
	normal = mat3(transpose(inverse(pushConsts.model))) * norm;
	tang = mat3(transpose(inverse(pushConsts.model))) * tangent;
	gl_Position = ubo.projView * pushConsts.model * vec4(pos.xyz, 1.0);
	tex = pushConsts.textureCount;
	
}