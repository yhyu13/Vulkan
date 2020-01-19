#version 450

layout (location = 0) in vec2 inUV;
layout (location = 1) in mat4 projMatrix;
layout (location = 5) in vec4 viewPosition;

layout ( binding = 2) uniform sampler2D samplerPosition;
layout ( binding = 3) uniform sampler2D samplerNormal;
layout ( binding = 4) uniform sampler2D samplerAlbedo;
layout ( binding = 7) uniform sampler2D ssaoBlurInput;

layout (location = 0) out vec4 FragColor;

layout ( binding = 9) uniform Light 
{
	vec3 lightPos;
	vec3 lightColor;
	float dummy1;
	float Linear;
	float dummy2;
	float dummy3;
	float dummy4;
	float Quadratic;
} light;

void main() 
{
   
  // retrieve data from gbuffer
    vec3 FragPos = texture(samplerPosition, inUV).rgb;
	//FragPos.y = -FragPos.y;
    vec3 Normal = texture(samplerNormal, inUV).rgb;
	Normal.y = -Normal.y;
    vec3 Diffuse = texture(samplerAlbedo, inUV).rgb;
    float AmbientOcclusion =1.0- texture(ssaoBlurInput, inUV).r ;
 
    // then calculate lighting as usual
    vec3 ambient = vec3(0.2 * Diffuse * AmbientOcclusion);
    vec3 lighting  = ambient; 
    vec3 viewDir  = normalize( viewPosition.xyz -FragPos); 
    // diffuse
	vec3 lightPostion = light.lightPos.xyz;
	lightPostion.y = -lightPostion.y;
    vec3 lightDir = normalize(lightPostion - FragPos);
    vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.lightColor;
    // specular
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 8.0);
    vec3 specular = light.lightColor * spec;
    // attenuation
    float distance = length(lightPostion- FragPos);
    float attenuation = 1.0 / (1.0 + light.Linear * distance + light.Quadratic * distance * distance);
    diffuse *= attenuation;
    specular *= attenuation;
    lighting += diffuse;

    FragColor = vec4(lighting, 1.0);
}