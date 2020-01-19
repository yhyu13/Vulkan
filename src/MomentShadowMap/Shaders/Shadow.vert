#version 450

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 color;
layout (location = 4) in vec3 tangent;


struct LightInstanceDetails
{
	vec3 position;
	vec3 color;
	mat4 mvpMatrix;
	mat4 viewMatrix;
};

layout ( binding = 6) uniform LightInstance
{
	LightInstanceDetails lightInstanceDetails[1];
} lightInstance;


layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	int textureCount;
	int debugValue;
} pushConsts;

layout (location = 0) out vec4 wolrdPos;

void main()
{
	vec4 pos = inPos;

	gl_Position =  lightInstance.lightInstanceDetails[0].mvpMatrix * pushConsts.model * pos;
	wolrdPos = vec4(gl_Position);
}