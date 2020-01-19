#version 450

layout ( binding = 1) uniform sampler2D samplerColorMap;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inColor;
layout (location = 2) in  vec3 FragPos;
layout (location = 3) in  vec3 Normal;

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedo;

void main() 
{
    gPosition = vec4(FragPos,1.0);

    gNormal = normalize(vec4(Normal,1.0));

    gAlbedo = texture(samplerColorMap, inUV) * inColor;
}