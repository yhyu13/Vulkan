#version 450

layout (location = 0) in vec2 inUV;
layout (location = 1) in mat4 projMatrix;
layout ( binding = 2) uniform sampler2D samplerPosition;
layout ( binding = 3) uniform sampler2D samplerNormal;



layout ( binding = 5) uniform SSAOKernal 
{
	vec4 ssaoKernal[64];
} kernalSSAO;
layout ( binding = 6) uniform SSAONoise
{
	vec4 noise[16];
} noiseSSAO;

layout (location = 0) out float ssaoTextureOut;

int kernelSize = 64;
float radius = 0.5;
float bias = 0.025;
const vec2 noiseScale = vec2(1280.0/4.0, 720.0/4.0); 

void main() 
{
   
    vec3 fragPos = texture(samplerPosition, inUV).xyz;
    vec3 normal = normalize(texture(samplerNormal, inUV).rgb);
	uint x_Val = uint(inUV.x*4.0) ;
		uint y_Val = uint(inUV.y*4.0) ;
	uint xOffset =  uint(x_Val%4);
	uint yOffset =  uint(y_Val%4);

    vec3 randomVec = noiseSSAO.noise[yOffset*4+xOffset].xyz;
	//normalize(texture(texNoise, inUV * noiseScale).xyz);
   
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        
        vec3 sampleVal = TBN * kernalSSAO.ssaoKernal[i].xyz; 
        sampleVal = fragPos + sampleVal * radius; 
        
       
        vec4 offset = vec4(sampleVal, 1.0);
        offset = projMatrix * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        
        float sampleDepth = texture(samplerPosition, offset.xy).z; // get depth value of kernel sample
        
       
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= sampleVal.z + bias ? 1.0 : 0.0) * rangeCheck;           
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    
    ssaoTextureOut = occlusion;
}