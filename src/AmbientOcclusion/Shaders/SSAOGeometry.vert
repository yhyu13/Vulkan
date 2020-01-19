#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;
layout (location = 2) in vec2 aUV;

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

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outColor;
layout (location = 2) out  vec3 FragPos;
layout (location = 3) out  vec3 Normal;
void main() 
{
	outUV = aUV;
	outColor = pushConsts.color;
	gl_Position = ubo.projView * pushConsts.model * vec4(aPos, 1.0);

	vec4 viewPos = ubo.viewMatrix * pushConsts.model * vec4(aPos, 1.0);
    FragPos = viewPos.xyz; 

	mat3 normalMatrix = transpose(inverse(mat3(ubo.viewMatrix * pushConsts.model)));
    Normal = normalMatrix * aNorm;
}