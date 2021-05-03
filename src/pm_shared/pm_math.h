#ifndef PM_MATH_H
#define PM_MATH_H
#include <mathlib/mathlib.h>

void AngleVectors(const Vector &angles, Vector *forward, Vector *right, Vector *up);
void AngleVectorsTranspose(const Vector &angles, Vector *forward, Vector *right, Vector *up);

#endif
