namespace Maths
{
	constexpr float PI = 3.14159265359f;

	inline float deg2rad(float deg)
	{
		return PI * deg / 180.0f;
	}

	inline float rad2deg(float rad)
	{
		return rad * 180.0f / PI;
	}
};