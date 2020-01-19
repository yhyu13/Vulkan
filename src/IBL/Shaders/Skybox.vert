#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 tangent;

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	mat4 projMatrix;
	mat4 viewMatrix;
	vec3 viewPosition;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	mat4 proj;
	mat4 view;
} pushConsts;


layout (location = 1) out vec3 fragPos;


void main() 
{
    mat4 rotView = mat4(mat3(ubo.viewMatrix)); // remove translation from the view matrix
    vec4 clipPos = ubo.projMatrix * rotView * vec4(pos, 1.0);
    gl_Position = clipPos.xyww;
	fragPos = pos.xyz;
	//fragPos.y = -fragPos.y;
	gl_Position = clipPos.xyww;;
}