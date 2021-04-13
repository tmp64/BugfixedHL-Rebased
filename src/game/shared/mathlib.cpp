#include <mathlib/mathlib.h>

// Stuff required for some Source SDK mathlib functions.
const Vector vec3_origin(0, 0, 0);
const QAngle vec3_angle(0, 0, 0);
const Vector vec3_invalid(FLT_MAX, FLT_MAX, FLT_MAX);
const int nanmask = 255 << 23;

void NormalizeAngles(float *angles)
{
	int i;
	// Normalize angles
	for (i = 0; i < 3; i++)
	{
		if (angles[i] > 180.0)
		{
			angles[i] -= 360.0;
		}
		else if (angles[i] < -180.0)
		{
			angles[i] += 360.0;
		}
	}
}
