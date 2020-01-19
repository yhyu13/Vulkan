#version 450

struct LightInstanceDetails
{
	vec3 position;
	vec3 color;
};
layout ( binding = 1) uniform sampler2D albedoSampler;
layout ( binding = 2) uniform sampler2D normalSampler;
layout ( binding = 3) uniform sampler2D heightSampler;
layout ( binding = 4) uniform sampler2D aoSampler;
layout ( binding = 5) uniform sampler2D metalSampler;
layout ( binding = 6) uniform sampler2D roughSampler;


layout ( binding = 7) uniform LightData 
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

layout ( binding = 8) uniform LightInstance
{
	LightInstanceDetails lightInstanceDetails[100];
} lightInstance;

layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec3 viewPos;
layout (location = 4) in vec3 normal1;
layout (location = 5) in vec3 fragPos;
layout (location = 6) in mat3 TBN;
layout (location = 9) in mat3 model;

layout (location = 0) out vec4 FragColor;

vec4 CalcPointLight(vec3 normal,  vec3 viewDir, int currentLight);
vec4 CalcSpotLight( vec3 normal, vec3 viewDir, int currentLight);

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return max(F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0),0.0);
}
vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}
vec3 getNormalFromMap()
{
	vec3 norm = texture(normalSampler, inUV).rgb;

    vec3 tangentNormal =  norm * 2.0 - 1.0;

    vec3 Q1  = dFdx(fragPos);
    vec3 Q2  = dFdy(fragPos);
    vec2 st1 = dFdx(inUV);
    vec2 st2 = dFdy(inUV);

    vec3 N   = normalize(normal1);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize( mat3(T, B, N) * tangentNormal);
}
vec3 albedo;
float metallic;
float roughness;
float ao;

void main() 
{


	albedo     = pow(texture(albedoSampler, inUV).rgb, vec3(2.2));
    metallic  = texture(metalSampler, inUV).r;
    roughness = texture(roughSampler, inUV).r;
    ao        = texture(aoSampler, inUV).r;

    vec3 normal =   texture(normalSampler, inUV).rgb;
	
    vec3 tangentNormal =  normal * 2.0 - 1.0;
	//tangentNormal.y = -tangentNormal.y;
	vec3 norm =  tangentNormal;
	//vec3 norm =  normalize(normal);

    vec3 viewDir = normalize( viewPos -  fragPos );
	vec4 finalColor = vec4(0);

	

	for(int i = 0; i< 1; ++i)
	{
		if(lightData.lightType == 0)
		{
			finalColor += CalcPointLight(norm, viewDir, i);
		}
		else
		{
			//finalColor += CalcSpotLight(norm, viewDir, i);
		}
		
	}
	 vec3 ambient = vec3(0.01) * albedo * ao;
	 vec3 color = ambient + finalColor.xyz;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 

	FragColor = vec4(color, 1.0);
}

vec4 CalcPointLight( vec3 normal, vec3 viewDir, int currentLight)
{
    vec3 lightDir = normalize((TBN*lightInstance.lightInstanceDetails[currentLight].position) - fragPos);
    vec3 H = normalize(viewDir + lightDir);
	 float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), lightData.shininess);
    float distance = length((TBN*lightInstance.lightInstanceDetails[currentLight].position) - fragPos);
    float attenuation = 1.0 / (lightData.constant + lightData.linear * distance + lightData.quadratic * (distance * distance));    
    vec3 radiance = vec3(1.0) * attenuation *max(dot(H, viewDir), 0.0);
	
	vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

	
	// Cook-Torrance BRDF
        float NDF = DistributionGGX(normal, H, roughness);   
        float G   = GeometrySmith(normal, viewDir, lightDir, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);
 
   vec3 nominator    = NDF * G * F; 
        float denominator = 4 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
        vec3 specular = nominator / denominator;
     
	
        vec3 kS = F;
          vec3 kD = vec3(1.0) - kS;
          kD *= (1.0 - metallic);	

        float NdotL = max(dot(normal, lightDir), 0.0);          
		
		vec4 result = vec4((kD * albedo / PI + specular) * radiance * NdotL,1.0);
		//	vec4 result = texture(normalSampler, inUV);
        return result; 
   
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
   // vec4 ambient = lightData.ambient * texture(samplerColorMap, inUV);
   // vec4 diffuse = lightData.diffuse * diff * texture(samplerColorMap, inUV);
   // vec4 specular = lightData.specular * spec;
   // ambient *= attenuation * intensity;
   // diffuse *= attenuation * intensity;
   // specular *= attenuation * intensity;
    return vec4(1.0 );
}