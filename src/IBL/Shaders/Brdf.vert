#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 tangent;


layout (location = 1) out vec2 outUV;

void main() 
{
	
	if(gl_VertexIndex == 0)
	{
		outUV = vec2(0.0,1.0);
		gl_Position =  vec4(-1.0,1.0,0.0, 1.0);
	}
	else if(gl_VertexIndex == 1)
	{
		outUV = vec2(1.0,1.0);
		gl_Position =  vec4(1.0,1.0,0.0, 1.0);
	}
	else if(gl_VertexIndex == 2)
	{
		outUV = vec2(0.0,0.0);
		gl_Position =  vec4(-1.0,-1.0,0.0, 1.0);
	}
	else if(gl_VertexIndex == 3)
	{
		outUV = vec2(0.0,0.0);
		gl_Position =  vec4(-1.0,-1.0,0.0, 1.0);
	}
	else if(gl_VertexIndex == 4)
	{
		outUV = vec2(1.0,1.0);
		gl_Position =  vec4(1.0,1.0,0.0, 1.0);
	}
	else if(gl_VertexIndex == 5)
	{
		outUV = vec2(1.0,0.0);
		gl_Position =  vec4(1.0,-1.0,0.0, 1.0);
	}
	
	
}