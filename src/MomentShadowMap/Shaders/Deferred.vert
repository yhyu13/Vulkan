#version 450

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	mat4 viewMat;
	vec3 viewPosition;
} ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 viewPos;
layout (location = 2) out flat int debug;
layout (location = 3) out flat float bias;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	int textureCount;
	int debugValue;
	float bias;
} pushConsts;

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
	debug = pushConsts.debugValue;
	bias = pushConsts.bias;
		
}