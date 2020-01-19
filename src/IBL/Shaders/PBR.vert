#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 tangent;

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	mat4 projMatrix;
	mat4 viewMatrix;
	vec3 viewPosition;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec4 color;
	mat4 proj;
	mat4 view;
} pushConsts;

layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec3 viewPos;
layout (location = 4) out vec3 normal;
layout (location = 5) out vec3 fragPos;
layout (location = 6) out mat3 TBN;
layout (location = 9) out mat3 model;

void main() 
{
	outUV = UV;
	outColor = pushConsts.color;
	
	vec3 tempNormal = norm;
	//tempNormal.y =-tempNormal.y;
	//tempNormal.x = tempNormal.x;
	mat3 normalMatrix = transpose(inverse(mat3( pushConsts.model)));
    vec3 T = normalize(  normalMatrix * tangent);
    vec3 N = normalize(normalMatrix * tempNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross( N,T);

    TBN = transpose(mat3(T, B, N));  


	viewPos =  (ubo.viewPosition);
	fragPos =  vec3(pushConsts.model * vec4(pos.xyz, 1.0));
	//fragPos.y = -fragPos.y;
	//normal = mat3(transpose(inverse(pushConsts.model))) * norm;
	normal = mat3(pushConsts.model) * norm;
	gl_Position = ubo.projView * pushConsts.model * vec4(pos.xyz, 1.0);
	//gl_Position =  ubo.projMatrix* pushConsts.view * pushConsts.model * vec4(pos.xyz, 1.0);
}