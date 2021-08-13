namespace Maths
{
	constexpr float PI = 3.14159265359f;

	inline float deg2rad(float deg)
	{
		return PI * deg / 360.0f;
	}

	inline float rad2deg(float rad)
	{
		return rad * 360.0f / PI;
	}
};