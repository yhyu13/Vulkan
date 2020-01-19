
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>     
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

class Quaternions {
public:
	float  x, y, z, w;
public:
	Quaternions();
	Quaternions( float X, float Y, float Z, float W);
	
	void Normalize();
	Quaternions Normalized();
	aiMatrix4x4 GetMatrix();
	
	Quaternions Slerp( Quaternions b, float t);	
	Quaternions nLerp(Quaternions b, float t);

	float Dot(Quaternions a, Quaternions b); 
	float Length(Quaternions a);

	Quaternions operator*(float val);

	void Inverse(Quaternions& inv);

	Quaternions operator+(Quaternions& val); 
	Quaternions operator*(Quaternions& val);
	static Quaternions AngleAxis(float angle, aiVector3D axis);
	
};
