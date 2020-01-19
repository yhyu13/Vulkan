#version 450

layout ( binding = 1) uniform sampler2D samplerColorMap;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 FragColor;

void main() 
{
	FragColor = texture(samplerColorMap, inUV) * inColor;	
}