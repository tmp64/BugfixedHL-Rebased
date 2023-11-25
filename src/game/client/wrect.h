//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#if !defined(WRECTH)
#define WRECTH

typedef struct rect_s
{
	int left, right, top, bottom;

	int GetWidth() const { return right - left; }
	int GetHeight() const { return bottom - top; }
} wrect_t;

#endif
