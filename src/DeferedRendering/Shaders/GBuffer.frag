#version 450


layout ( binding = 7) uniform sampler2D samplerAlbedo;
layout ( binding = 8) uniform sampler2D samplerNormal;
layout ( binding = 9) uniform sampler2D samplerSpecular;

layout (location = 0) in vec3 normal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec3 viewPos;
layout (location = 4) in vec3 fragPos;
layout (location = 5) in vec3 tangent;
layout (location = 6) in flat int texcount;

layout (location = 0) out vec4 outGPosition;
layout (location = 1) out vec4 outGNormal;
layout (location = 2) out vec4 outGAlbedo;
layout (location = 3) out vec4 outGSpecular;

void main() 
{
	vec3 N = normalize(normal);
	N = -N;
	vec3 T = normalize(tangent);
	vec3 B = cross(N, T);
	mat3 TBN = mat3(T, B, N);
	//
	vec3 tnorm = vec3(0.0f);
	if( texcount > 1)
	{
		 tnorm = TBN * normalize(texture(samplerNormal, inUV).xyz * 2.0 - vec3(1.0));
	}
	else
	{
		 tnorm = -N;
	}


	outGPosition = vec4(fragPos, 1.0);
	outGNormal = vec4(tnorm, 1.0);
	outGAlbedo.rgb = texture(samplerAlbedo, inUV).rgb;

	
	
	if(texcount >2 )
	{
		outGSpecular = texture(samplerSpecular, inUV);
	}
	else
	{
		outGSpecular = vec4(0.0f);
	}
}

