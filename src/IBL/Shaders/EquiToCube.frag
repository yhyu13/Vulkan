#version 450


layout ( binding = 9) uniform sampler2D eqiRectSampler;

layout (location = 1) in vec3 fragPos;

layout (location = 0) out vec4 FragColor;


const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() 
{
	vec2 uv = SampleSphericalMap(normalize(fragPos));
    vec3 color = texture(eqiRectSampler, uv).rgb;
	FragColor = vec4(color, 1.0);
}

