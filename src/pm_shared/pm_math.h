#ifndef PM_MATH_H
#define PM_MATH_H
#include <mathlib/mathlib.h>

// TODO: Remove when vec3_t is replaced everywhere.
using vec3_t = Vector;

void AngleVectors(const Vector &angles, Vector *forward, Vector *right, Vector *up);
void AngleVectorsTranspose(const Vector &angles, Vector *forward, Vector *right, Vector *up);

#endif
