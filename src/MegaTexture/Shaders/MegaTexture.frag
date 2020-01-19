#version 450
#extension GL_ARB_sparse_texture2 : enable
#extension GL_ARB_sparse_texture_clamp : enable

layout ( binding = 1) uniform sampler2D samplerColorMap;

layout (binding = 2) uniform sampler2D sparseTextureSampler;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inColor;

layout (location = 2) flat in int modelIndex;
layout (location = 3) flat in int debugDraw;
layout (location = 0) out vec4 FragColor;



#define SUB_TEXTURE_SIZE 16384
#define SUB_TEXTURE_MIPCOUNT 15
#define FEED_BACK_WIDTH 64
#define PAGE_TILE_SIZE 128


struct Page
{
		 uint miplevel;
		 uint pageNumber;
		  uint pageAlive;
		  uint dummy;
};


layout (std430,  binding = 3) buffer FeedBackBuffer
{
    Page pageList[];
} ;

float MipLevel( vec2 uv )
{
  vec2 dx = dFdx( uv * SUB_TEXTURE_SIZE );
  vec2 dy = dFdy( uv * SUB_TEXTURE_SIZE );
  float d = max( dot( dx, dx ), dot( dy, dy ) );

  // Clamp the value to the max mip level counts
  const float rangeClamp = pow(2, (SUB_TEXTURE_MIPCOUNT ) * 2);
  d = clamp(d, 1.0, rangeClamp);
      
  float mipLevel = 0.5 * log2(d);
  mipLevel = floor(mipLevel);   
  
  return mipLevel;
}

float MipLevel1( vec2 uv)
{
   vec2 dx = dFdx( uv * 16384 );
   vec2 dy = dFdy( uv * 16384 );
   float d = max( dot( dx, dx ), dot( dy, dy ) );

   return max( 0.5 * log2( d ), 0 );
}

vec3 COLOR_MASKS[8];
  


void main() 
{
	
	COLOR_MASKS[0] = vec3( 1.0, 0.0, 0.0 );
  COLOR_MASKS[1] = vec3( 0.5, 0.8, 0.1 );
  COLOR_MASKS[2] = vec3( 0.0, 0.5, 0.5 );
  COLOR_MASKS[3] = vec3( 0.5, 1.0, 0.5 );
  COLOR_MASKS[4] = vec3( 0.5, 0.5, 0.2 );
  COLOR_MASKS[5] = vec3( 0.5, 1.0, 0.7 );
  COLOR_MASKS[6] = vec3( 0.6, 0.5, 0.1 );
  COLOR_MASKS[7] = vec3( 1.0, 0.3, 0.5 );

	uint x_Coord = uint((gl_FragCoord.x/1280) * FEED_BACK_WIDTH);
	uint y_Coord = uint((gl_FragCoord.y/720) * FEED_BACK_WIDTH);

	Page pageTemp;
	pageTemp.miplevel = uint(MipLevel1(inUV));
	if(pageTemp.miplevel >7)
	{
		pageTemp.pageAlive = 0;
	}
	else
	{
		uint powMip = uint( 14 - pageTemp.miplevel);
		uint mipOffset = uint( pow( 2, powMip ))/PAGE_TILE_SIZE;
		uint offsetX = uint(mipOffset * inUV.x);
		uint offsetY = uint(mipOffset * inUV.y);
		pageTemp.pageNumber = uint(offsetY * mipOffset + offsetX);
		pageTemp.pageAlive = 1;
		pageTemp.dummy = 1;

		pageList[(y_Coord*FEED_BACK_WIDTH) + x_Coord] = pageTemp;
	
		
	}

	vec4 color = vec4(0.0);
		float minLod = pageTemp.miplevel;
		int residencyCode = sparseTextureARB(sparseTextureSampler, inUV, color, minLod);

		
		while (!sparseTexelsResidentARB(residencyCode)) 
		{
			minLod += 1.0f;
			if(minLod > 7.0)
			{
				break;
			}
			residencyCode = sparseTextureClampARB(sparseTextureSampler, inUV, minLod, color);
			
		} 
		
		
		//residencyCode = sparseTextureLodARB(sparseTextureSampler, inUV, 0.0, color);

		bool texelResident = sparseTexelsResidentARB(residencyCode);

		if (!texelResident)
		{
			//color = vec4(1.0, 0.0, 0.0, 1.0);
			sparseTextureLodARB(sparseTextureSampler, inUV, 7.0, color);
			minLod = 7.0f;
		}
		
		
		if(debugDraw == 1)
		{
			uint powMip = uint( 14 - minLod);
			float width = pow( 2, powMip );
			uint xOffset = uint(inUV.x * width);
			uint yOffset = uint(inUV.y * width);
			xOffset = xOffset % PAGE_TILE_SIZE;
			yOffset = yOffset % PAGE_TILE_SIZE;

			
			if(xOffset<5 || xOffset>PAGE_TILE_SIZE-5)
			{
				FragColor =vec4( COLOR_MASKS[uint(minLod)],1.0);
			}
			else if(yOffset<5 || yOffset>PAGE_TILE_SIZE-5)
			{
				FragColor = vec4(COLOR_MASKS[uint(minLod)],1.0);
			}
			else
			{
				FragColor =color;
			}
		}
		else
		{
			FragColor =color;
		}
	
}