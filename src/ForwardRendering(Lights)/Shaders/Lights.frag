#version 450

struct LightInstanceDetails
{
	vec3 position;
	vec3 color;
};
layout ( binding = 1) uniform sampler2D samplerColorMap;

layout ( binding = 2) uniform LightData 
{
	int lightType;
	float constant;
	float linear;
	float quadratic;
	vec3 direction;
	float shininess;
	vec4 ambient;
    vec4 diffuse;
    vec4 specular;

	float cutOff;
    float outerCutOff;
} lightData;

layout ( binding = 3) uniform LightInstance
{
	LightInstanceDetails lightInstanceDetails[100];
} lightInstance;

layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec3 viewPos;
layout (location = 4) in vec3 normal;
layout (location = 5) in vec3 fragPos;

layout (location = 0) out vec4 FragColor;

vec4 CalcPointLight(vec3 normal,  vec3 viewDir, int currentLight);
vec4 CalcSpotLight( vec3 normal, vec3 viewDir, int currentLight);

void main() 
{

	vec3 norm = normalize(normal);
    vec3 viewDir = normalize(viewPos - fragPos);
	vec4 finalColor = vec4(0);

	for(int i = 0; i< 100; ++i)
	{
		if(lightData.lightType == 0)
		{
			finalColor += CalcPointLight(norm, viewDir, i);
		}
		else
		{
			finalColor += CalcSpotLight(norm, viewDir, i);
		}
		
	}

	FragColor = finalColor * inColor;	
}

vec4 CalcPointLight( vec3 normal, vec3 viewDir, int currentLight)
{
    vec3 lightDir = normalize(lightInstance.lightInstanceDetails[currentLight].position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), lightData.shininess);
    float distance = length(lightInstance.lightInstanceDetails[currentLight].position - fragPos);
    float attenuation = 1.0 / (lightData.constant + lightData.linear * distance + lightData.quadratic * (distance * distance));    
    vec4 ambient = lightData.ambient * texture(samplerColorMap, inUV);
    vec4 diffuse = lightData.diffuse * diff * texture(samplerColorMap, inUV);
   // vec4 specular = lightData.specular * spec;
    ambient *= attenuation;
    diffuse *= attenuation;
    //specular *= attenuation;
    return (ambient + diffuse );
}

vec4 CalcSpotLight( vec3 normal, vec3 viewDir, int currentLight)
{
     vec3 lightDir = normalize(lightInstance.lightInstanceDetails[currentLight].position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), lightData.shininess);
    // attenuation
    float distance = length(lightInstance.lightInstanceDetails[currentLight].position - fragPos);
    float attenuation = 1.0 / (lightData.constant + lightData.linear * distance + lightData.quadratic * (distance * distance));    
    // spotlight intensity
    float theta = dot(lightDir, normalize(-lightData.direction)); 
    float epsilon = lightData.cutOff - lightData.outerCutOff;
    float intensity = clamp((theta - lightData.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results
    vec4 ambient = lightData.ambient * texture(samplerColorMap, inUV);
    vec4 diffuse = lightData.diffuse * diff * texture(samplerColorMap, inUV);
    vec4 specular = lightData.specular * spec;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse );
}