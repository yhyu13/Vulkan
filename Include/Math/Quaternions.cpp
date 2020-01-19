#include "Quaternions.h"

Quaternions::Quaternions()
{
}

Quaternions::Quaternions(float X, float Y, float Z, float W): x(X), y(Y), z(Z), w(W)
{
}

void Quaternions::Normalize()
{
	float mag = sqrtf( x * x + y * y + z * z + w * w );
	
	x /= mag;
	y /= mag;
	z /= mag;
	w /= mag;
}

Quaternions Quaternions::Normalized()
{
	float mag = sqrtf( x * x + y * y + z * z + w * w);
	return Quaternions( x / mag, y / mag, z / mag, w / mag);
}

aiMatrix4x4 Quaternions::GetMatrix()
{
	float sqw = this->w * this->w;
	float sqx = this->x * this->x;
	float sqy = this->y * this->y;
	float sqz = this->z * this->z;
	float invs = 1.0f / (sqx + sqy + sqz + sqw);

	aiMatrix4x4 matrix;
	matrix.IsIdentity();
	matrix.a1 = (sqx - sqy - sqz + sqw) * invs;
	matrix.b2 = (-sqx + sqy - sqz + sqw) * invs;
	matrix.c3 = (-sqx - sqy + sqz + sqw) * invs;

	float tmp1 = this->x * this->y;
	float tmp2 = this->z * this->w;
	matrix.b1 = 2.0 * (tmp1 + tmp2) * invs;
	matrix.a2 = 2.0 * (tmp1 - tmp2) * invs;

	tmp1 = this->x * this->z;
	tmp2 = this->y * this->w;
	matrix.c1 = 2.0 * (tmp1 - tmp2) * invs;
	matrix.a3 = 2.0 * (tmp1 + tmp2) * invs;

	tmp1 = this->y * this->z;
	tmp2 = this->x  * this->w;
	matrix.c2 = 2.0 * (tmp1 + tmp2) * invs;
	matrix.b3 = 2.0 * (tmp1 - tmp2) * invs;

	return matrix;
}

Quaternions Quaternions::Slerp(Quaternions  b, float t)
{
	Quaternions a(this->x, this->y, this->z, this->w);
	
	a.Normalize();
	b.Normalize();
	float dot_product = Dot(a, b);
	
	Quaternions temp(b.x, b.y, b.z, b.w);
	
	if (abs(dot_product) >= 1.0)
	{
		temp.x = a.x; 
		temp.y = a.y;
		temp.z = a.z;
		temp.w = a.w;

		return temp;
	}
	if (dot_product < 0.0f) 
	{
		dot_product *= -1.0f;
		Inverse(temp);
		
	}

	float theta = acosf(dot_product);
	float sinTheta = 1.0f / sinf(theta);
	a = a * sinf(theta * (1.0f - t))* sinTheta;
	temp = temp * sinf(t * theta)* sinTheta;
	Quaternions result;
	result = a  + temp;
		
	return result.Normalized();
	
}

Quaternions Quaternions::nLerp(Quaternions b, float t)
{
	
	Quaternions a (this->x, this->y, this->z, this->w);

	a.Normalize();
	b.Normalize();

	Quaternions result;
	float dot_product = Dot(a, b);
	
	if (dot_product < 0.0f)
	{
		Quaternions a_val(a.x, a.y, a.z, a.w);
		Quaternions b_val(-b.x, -b.y, -b.z, -b.w);
		a_val = a_val * (1.0f - t);
		b_val = b_val * t;
		result = a_val + b_val;
		
	}
	else
	{
		Quaternions a_val(a.x, a.y, a.z, a.w);
		Quaternions b_val(b.x, b.y, b.z, b.w);
		a_val = a_val * (1.0f - t);
		b_val = b_val * t;

		result = a_val + b_val;
	}

	return result.Normalized();
}

float Quaternions::Dot(Quaternions a, Quaternions b)
{
	return (a.x * b.x * a.y * b.y + a.z * b.z + a.w * b.w);
}

float Quaternions::Length(Quaternions a)
{
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}

Quaternions Quaternions::operator*(float val)
{
	return Quaternions( x * val, y * val, z * val, w * val);
}

void Quaternions::Inverse(Quaternions & inv)
{
	inv.x *= -1.0f;
	inv.y *= -1.0f;
	inv.z *= -1.0f;
	inv.w *= -1.0f;

}

Quaternions Quaternions::operator+(Quaternions & val)
{
	return Quaternions(x + val.x, y + val.y, z + val.z, w + val.w);
}

Quaternions Quaternions::operator*(Quaternions & val)
{
	Quaternions q;
	q.x = w * val.x + x * val.w + y * val.z - z * val.y;
	q.y = w * val.y - x * val.z + y * val.w + z * val.x;
	q.z = w * val.z + x * val.y - y * val.x + z * val.w;
	q.w = w * val.w - x * val.x - y * val.y - z * val.z;

	return q;
}

Quaternions Quaternions::AngleAxis(float angle, aiVector3D axis)
{
	aiVector3D vn = axis.Normalize();

	angle *= 0.0174532925f; // To radians!
	angle *= 0.5f;
	float sinAngle = sin(angle);

	return Quaternions(vn.x * sinAngle, vn.y * sinAngle, vn.z * sinAngle, cos(angle));
}


