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
	int modelIndex;
	int dummy1;
	int dummy2;
	int dummy3;
	int debugDraw;
} pushConsts;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outColor;
layout (location = 2) out int modelIndex;
layout (location = 3) out int debugDraw;
void main() 
{
	vec2 uv = aUV;
	uv = uv/4.0;

	uint yOffsetUV = uint(pushConsts.modelIndex/4);
	uint xOffsetUV = uint(pushConsts.modelIndex % 4) ;
	uv.x = float(xOffsetUV * 0.25) + uv.x;
	uv.y = float(yOffsetUV * 0.25) + uv.y ;



	outUV = uv;
	outColor = pushConsts.color;
	gl_Position = ubo.projView * pushConsts.model * vec4(aPos, 1.0);
	modelIndex = pushConsts.modelIndex;
	debugDraw = pushConsts.debugDraw;
}