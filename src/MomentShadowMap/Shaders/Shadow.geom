#version 450

#define LIGHT_COUNT 1

layout (location = 0) out vec4 outShadow;

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

layout (triangles, invocations = LIGHT_COUNT) in;
layout (triangle_strip, max_vertices = 3) out;



layout (location = 0) in int inInstanceIndex[];

void main() 
{
	vec4 instancedPos = vec4( lightInstance.lightInstanceDetails[0].position, 0.f); 
	for (int i = 0; i < gl_in.length(); i++)
	{
		gl_Layer = gl_InvocationID;
		vec4 tmpPos = gl_in[i].gl_Position;
		gl_Position  = lightInstance.lightInstanceDetails[0].mvpMatrix *  tmpPos;
		outShadow = lightInstance.lightInstanceDetails[0].mvpMatrix *  tmpPos;
		EmitVertex();
	}
	EndPrimitive();
}