#version 450

struct LightInstanceDetails
{
	vec3 position;
	vec3 color;
};


layout ( binding = 1) uniform sampler2D samplerPosition;
layout ( binding = 2) uniform sampler2D samplerAlbedo;
layout ( binding = 3) uniform sampler2D samplerNormal;
layout ( binding = 4) uniform sampler2D samplerSpecular;


layout ( binding = 5) uniform LightData 
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

layout ( binding = 6) uniform LightInstance
{
	LightInstanceDetails lightInstanceDetails[1];
} lightInstance;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 viewPos;


layout (location = 0) out vec4 FragColor;

vec4 CalcPointLight(vec3 fragPos, vec3 normal,  vec3 albedo, vec3 specular, vec3 viewDir, int currentLight);
vec4 CalcSpotLight(vec3 fragPos, vec3 normal,  vec3 albedo, vec3 specular, vec3 viewDir, int currentLight);

void main() 
{
	vec2 UV =  inUV;
	UV.y = 1.0f - UV.y ;
	vec3 FragPos = texture(samplerPosition, UV).rgb;
    vec3 Normal = texture(samplerNormal, UV).rgb;
    vec3 Albedo = texture(samplerAlbedo, UV).rgb;
    vec3 Specular = texture(samplerSpecular, UV).rgb;

	
    vec3 viewDir = normalize( viewPos - FragPos );
	vec4 finalColor = vec4(0);

	for(int i = 0; i < 1; ++i)
	{
		
			finalColor += CalcPointLight(FragPos, Normal, Albedo, Specular, viewDir, i);
		
		
	}

	FragColor = finalColor ;	
	//FragColor = vec4(Albedo,1.0f);
}

vec4 CalcPointLight(vec3 fragPos, vec3 normal,  vec3 albedo, vec3 specular, vec3 viewDir, int currentLight)
{
    vec3 lightDir = normalize(lightInstance.lightInstanceDetails[currentLight].position - fragPos  );

    float diff = 1.0f-max(dot(normal, lightDir), 0.0);
	
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), lightData.shininess);
    float distance = length(lightInstance.lightInstanceDetails[currentLight].position - fragPos );
    float attenuation = 1.0 / (lightData.constant + lightData.linear * distance + lightData.quadratic * (distance * distance));    
    vec4 ambient = lightData.ambient * vec4(albedo, 1.0f);
    vec4 albedoN = lightData.diffuse * diff * vec4(albedo, 1.0f);
    vec4 specularN = lightData.specular * spec * vec4(specular,1.0f);
    ambient *= attenuation;
    albedoN *= attenuation;
    specularN *= attenuation;
    return ambient + albedoN ;
}


   
