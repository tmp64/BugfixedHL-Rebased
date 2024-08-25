#ifndef PM_MATH_H
#define PM_MATH_H
#include <mathlib/mathlib.h>

void PM_AngleVectors(const Vector &angles, Vector *forward, Vector *right, Vector *up);
void PM_AngleVectorsTranspose(const Vector &angles, Vector *forward, Vector *right, Vector *up);

//! More precise than VectorNormalize from mathlib
//! mathlib's VectorNormalize returns very small positive number for null vectors
//! but PM code needs it to be == 0.0f.
float PM_VectorNormalize(Vector &v);

#endif
