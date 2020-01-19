#version 450



layout ( binding = 10) uniform samplerCube eqiRectSampler;

layout ( binding = 16) uniform samplerCube irradiansSampler;
layout (location = 1) in vec3 fragPos;

layout (location = 0) out vec4 FragColor;


const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() 
{
	 vec3 envColor = texture(eqiRectSampler, fragPos).rgb;
    
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2)); 

   const float gamma = 2.2;
   //const float gamma = 1.0;
   // Exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-envColor * 5.00);
    // Gamma correction 
    mapped = pow(mapped, vec3( gamma));

    FragColor = vec4(mapped, 1.0);

}

