#version 450

#define SHADOW_FACTOR 0.25

struct LightInstanceDetails
{
	vec3 position;
	vec3 color;
	mat4 mvpMatrix;
	mat4 viewMatrix;
};


layout ( binding = 1) uniform sampler2D samplerPosition;
layout ( binding = 2) uniform sampler2D samplerAlbedo;
layout ( binding = 3) uniform sampler2D samplerNormal;
layout ( binding = 4) uniform sampler2D samplerSpecular;
layout (binding = 10) uniform sampler2D  samplerShadowMap;

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

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	mat4 viewMat;
	vec3 viewPosition;
} ubo;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 viewPos;
layout (location = 2) in flat int debug;
layout (location = 3) in flat float bias;

layout (location = 0) out vec4 FragColor;


vec4 CalcSpotLight(vec4 fragPos, vec3 normal,  vec3 albedo, vec3 specular, vec3 viewDir, int currentLight);



float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * 0.1f * 100.0f) / (100.0f + 0.1f - z * (100.0f - 0.1f));
}

void main() 
{
	vec2 UV =  inUV;
	//UV.y = 1.0f - UV.y ;
	vec4 FragPos = texture(samplerPosition, UV);
    vec3 Normal = texture(samplerNormal, UV).rgb;
    vec3 Albedo = texture(samplerAlbedo, UV).rgb;
    vec3 Specular = texture(samplerSpecular, UV).rgb;

	

    vec3 viewDir = normalize( viewPos - FragPos.rgb );
	vec4 finalColor = vec4(0);

	for(int i = 0; i < 1; ++i)
	{
			finalColor += CalcSpotLight(FragPos, Normal, Albedo, Specular, viewDir, i);
	}
	
	if(debug == 0)
	{
		FragColor = finalColor ;
	}
	else if(debug == 1)
	{

		FragColor = vec4(FragPos.rgb,1.0f);
	}
	else if(debug == 2)
	{
		FragColor = vec4(Normal,1.0f);
	}
	else if(debug == 3)
	{
		FragColor = vec4(Albedo,1.0f);
	}
	else if(debug == 4)
	{
		//float x = LinearizeDepth(texture(samplerShadowMap,UV).r);
		//FragColor = vec4(vec3(x)/100.0f,1.0f);
	}
}

float det3(vec3 a, vec3 b, vec3 c)
{
    return a.x * (b.y * c.z - b.z * c.y) + a.y 
        * (b.z * c.x - b.x * c.z) + a.z * (b.x * c.y - b.y * c.x);
}
float textureProj(vec4 P, vec3 norm, vec3 fragPosition)
{
	
	vec4 shadowCoord = P / P.w;
	//float alphaValue =  max(0.05 * (1.0 - dot(norm,vec3(0.0f,-1.0f,-1.0f))), bias);
	float alphaValue =  bias;
	if (shadowCoord.z  < 0.0f || shadowCoord.z  > 1.0f)
    {
        return 0.0f;
    }

	shadowCoord.st = shadowCoord.st * 0.5 + 0.5;
	
	vec4 momentValue = vec4(texture(samplerShadowMap, shadowCoord.st).rgba);
	
    
	//float alphaValue =  0.05f;
	vec4 bPrime = momentValue.xyzw * (1.0f - alphaValue) + alphaValue * vec4(0.5f, 0.5f, 0.5f, 0.5f);

	vec3 aVector = vec3(1.0f, bPrime.x, bPrime.y);
    vec3 bVector = vec3(bPrime.xyz);
    vec3 cVector = vec3(bPrime.y, bPrime.z, bPrime.w);

	 float d = det3(aVector,  bVector, cVector);

	 float fragmentZPow2 = pow(shadowCoord.z , 2.0f);

	 vec3 zVector = vec3(1.0f, shadowCoord.z , fragmentZPow2);

	float c1 = det3(zVector, bVector, cVector) / d;
    float c2 = det3(aVector, zVector, cVector) / d;
    float c3 = det3(aVector, bVector, zVector) / d;

	float a = c3;
    float b = c2;
    float c = c1;

	 float discriminant = sqrt((b * b) - (4.0f * a * c));
    float c1_2 = 2.0f * a;

    float positive_z = (-b + discriminant) / c1_2;
    float minus_z = (-b - discriminant) / c1_2;
     
    float min_z = min(positive_z, minus_z);
    float max_z = max(positive_z, minus_z);

    float shadow_factor = 0.0f;

    if(shadowCoord.z  <= min_z )
    {
        return 0.0f;
    }
    else if(shadowCoord.z <= max_z)
    {
        float numerator = (shadowCoord.z * max_z - bPrime.x * (shadowCoord.z  + max_z) + bPrime.y);
        float denominator = (max_z - min_z) * (shadowCoord.z - min_z);
        shadow_factor = numerator / denominator;
    }
    else
    {
        float numerator = min_z * max_z - bPrime.x * (min_z + max_z) + bPrime.y;
        float denominator = (shadowCoord.z - min_z) * (shadowCoord.z  - max_z);
        shadow_factor = 1.0f - (numerator / denominator);
    }
	
	return clamp(shadow_factor, 0.0f, 1.0f);
}



vec4 CalcSpotLight(vec4 fragPos, vec3 normal,  vec3 albedo, vec3 specular, vec3 viewDir, int currentLight)
{
     vec3 lightDir = normalize(lightInstance.lightInstanceDetails[currentLight].position  - fragPos.rgb);
	 float theta =  dot(lightDir, normalize(-lightData.direction)); 

	 
		float diff = 1.0 - max(dot(normal, lightDir), 0.0);
   
		vec3 reflectDir =  reflect(-lightDir, normal);
		float spec = pow(max(dot(viewDir, reflectDir), 0.0), lightData.shininess);
    
		float distance = length(  lightInstance.lightInstanceDetails[currentLight].position - fragPos.rgb  );
		float attenuation = 1.0 / (lightData.constant + lightData.linear * distance + lightData.quadratic * (distance * distance));    
    
    
		float epsilon = lightData.cutOff - lightData.outerCutOff;
		float intensity = clamp((theta - lightData.outerCutOff) / epsilon, 0.0, 1.0);
    
		vec4 ambient = lightData.ambient * vec4(albedo, 1.0f);
		vec4 albedoN = lightData.diffuse * diff * vec4(albedo, 1.0f);
		vec4 specularN = lightData.specular * spec * vec4(specular,1.0f);

		ambient *= attenuation * intensity;
		albedoN *= attenuation * intensity;
		specularN *= attenuation * intensity;


		
		

		vec4 shadowClip	= lightInstance.lightInstanceDetails[currentLight].mvpMatrix * vec4(fragPos.xyz, 1.0f);
		float shadow = textureProj(shadowClip, normal, fragPos.rgb);
		return ambient + (1.0f - shadow)*(albedoN);

	 
   
}