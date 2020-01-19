#version 450

layout (location = 0) out vec4  samplerShadowOut;

float near_plane = 0.1f;
 float far_plane = 100.0f;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}
layout (location = 0) in vec4 wolrdPos;
void main()
{      
	float temp = gl_FragCoord.z;
	
	//temp = temp / far_plane;
	//float temp = wolrdPos.w ;
	float lightDistance = length(vec3(0.0f, 3.0f,2.0f));
	float far = lightDistance + 2;
	float near = lightDistance - 2;
	 //temp = 1.0f - (temp - near)/ (far - near);

	float x = temp;
	float y = x * temp;
	float z = y * temp;
	float w = z * temp;
	
	samplerShadowOut = vec4( x , y , z , w );

}