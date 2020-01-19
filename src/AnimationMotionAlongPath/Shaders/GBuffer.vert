#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 color;
layout (location = 4) in vec3 tangent;
layout (location = 5) in vec4 boneWeight;
layout (location = 6) in vec4 boneId;

layout ( binding = 0) uniform UBO 
{
	mat4 projView;
	vec3 viewPosition;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	mat4 model_temp;
	vec4 color;
	vec3 linePos;
	int textureCount;
	int animationEnabled;
	int boneLine;

} pushConsts;

layout ( binding = 10) uniform Bones
{
	mat4 boneData[100];
} bones;

layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec3 viewPos;
layout (location = 4) out vec3 fragPos;
layout (location = 5) out vec3 tang;
layout (location = 6) out flat int tex;
layout (location = 7) out flat int boneDraw;


void main() 
{
	if(pushConsts.boneLine==0)
	{
		if(pushConsts.animationEnabled == 1)
		{
	
			highp int temp = int(boneId.x);
			mat4 boneTransform = bones.boneData[temp] * boneWeight[0];
			temp = int(boneId.y);
			boneTransform     += bones.boneData[temp] * boneWeight[1];
			temp = int(boneId.z);
			boneTransform     += bones.boneData[temp] * boneWeight[2];
			temp = int(boneId.w);
			boneTransform     += bones.boneData[temp] * boneWeight[3];	

			fragPos = vec3(pushConsts.model * boneTransform * vec4(pos.xyz, 1.0));
			normal = mat3(transpose(inverse(pushConsts.model * boneTransform))) * norm;
			tang = mat3(transpose(inverse(pushConsts.model * boneTransform))) * tangent;
			vec3 t = pos;
			//t.y = -t.y;
			gl_Position = ubo.projView * pushConsts.model * boneTransform * vec4(t.xyz, 1.0);

		
		}
		else
		{
			fragPos = vec3(pushConsts.model * vec4(pos.xyz, 1.0));
			normal = mat3(transpose(inverse(pushConsts.model))) * norm;
			tang = mat3(transpose(inverse(pushConsts.model))) * tangent;
			gl_Position = ubo.projView * pushConsts.model * vec4(pos.xyz, 1.0);
			//fragPos.y = -fragPos.y;

		}

		outUV = UV;
		outColor = pushConsts.color;
		viewPos = ubo.viewPosition;
	
		tex = pushConsts.textureCount;
		boneDraw = 0;
	}
	else
	{	
		boneDraw = 1;
		
		if(gl_VertexIndex == 0)
		{
			
			gl_Position = ubo.projView * pushConsts.model *  vec4(-1.0f,1.0f,0.0f, 1.0f);
			
		}
		else if(gl_VertexIndex == 1)
		{
			gl_Position = ubo.projView * pushConsts.model *  vec4(1.0f,1.0f,0.0f, 1.0f);
		}
		else if(gl_VertexIndex == 2)
		{
			gl_Position = ubo.projView * pushConsts.model_temp *  vec4(1.0f,-1.0f,0.0f, 1.0f);
		}
		else if(gl_VertexIndex == 3)
		{
			gl_Position = ubo.projView * pushConsts.model_temp *  vec4(1.0f,-1.0f,0.0f, 1.0f);
		}
		else if(gl_VertexIndex == 4)
		{
			gl_Position = ubo.projView * pushConsts.model_temp *  vec4(-1.0f,-1.0f,0.0f, 1.0f);
		}
		else if(gl_VertexIndex == 5)
		{
			gl_Position = ubo.projView * pushConsts.model *  vec4(-1.0f,1.0f,0.0f, 1.0f);
		}
		fragPos = gl_Position.xyz;
		
	}
	
}