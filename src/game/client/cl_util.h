/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// cl_util.h
//

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <tier0/dbg.h>
#include "cvardef.h"

inline void HookMessage(const char *pszName, UserMsgHookFn fn)
{
	gEngfuncs.pfnHookUserMsg((char *)pszName, fn);
}

inline void HookCommand(const char *pszName, CmdFunction fn)
{
	gEngfuncs.pfnAddCommand((char *)pszName, fn);
}

inline float CVAR_GET_FLOAT(const char *x)
{
	return gEngfuncs.pfnGetCvarFloat(x);
}
inline const char *CVAR_GET_STRING(const char *x)
{
	return gEngfuncs.pfnGetCvarString(x);
}
inline struct cvar_s *CVAR_CREATE(const char *cv, const char *val, const int flags)
{
	return gEngfuncs.pfnRegisterVariable(cv, val, flags);
}

#define SPR_Load    (*gEngfuncs.pfnSPR_Load)
#define SPR_Set     (*gEngfuncs.pfnSPR_Set)
#define SPR_Frames  (*gEngfuncs.pfnSPR_Frames)
#define SPR_GetList (*gEngfuncs.pfnSPR_GetList)

// SPR_Draw  draws a the current sprite as solid
#define SPR_Draw (*gEngfuncs.pfnSPR_Draw)
// SPR_DrawHoles  draws the current sprites,  with color index255 not drawn (transparent)
#define SPR_DrawHoles (*gEngfuncs.pfnSPR_DrawHoles)
// SPR_DrawAdditive  adds the sprites RGB values to the background  (additive transulency)
#define SPR_DrawAdditive (*gEngfuncs.pfnSPR_DrawAdditive)

// SPR_EnableScissor  sets a clipping rect for HUD sprites.  (0,0) is the top-left hand corner of the screen.
#define SPR_EnableScissor (*gEngfuncs.pfnSPR_EnableScissor)
// SPR_DisableScissor  disables the clipping rect
#define SPR_DisableScissor (*gEngfuncs.pfnSPR_DisableScissor)
//
#define FillRGBA (*gEngfuncs.pfnFillRGBA)

// ScreenHeight returns the height of the screen, in pixels
#define ScreenHeight (gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ScreenWidth (gHUD.m_scrinfo.iWidth)

#define BASE_XRES 640.f

// use this to project world coordinates to screen coordinates
#define XPROJECT(x) ((1.0f + (x)) * ScreenWidth * 0.5f)
#define YPROJECT(y) ((1.0f - (y)) * ScreenHeight * 0.5f)

#define XRES(x) (x * ((float)ScreenWidth / 640))
#define YRES(y) (y * ((float)ScreenHeight / 480))

#define GetScreenInfo   (*gEngfuncs.pfnGetScreenInfo)
#define ServerCmd       (*gEngfuncs.pfnServerCmd)
#define EngineClientCmd (*gEngfuncs.pfnClientCmd)
#define SetCrosshair    (*gEngfuncs.pfnSetCrosshair)
#define AngleVectors    (*gEngfuncs.pfnAngleVectors)

// Gets the height & width of a sprite,  at the specified frame
inline int SPR_Height(HSPRITE x, int f) { return gEngfuncs.pfnSPR_Height(x, f); }
inline int SPR_Width(HSPRITE x, int f) { return gEngfuncs.pfnSPR_Width(x, f); }

inline client_textmessage_t *TextMessageGet(const char *pName) { return gEngfuncs.pfnTextMessageGet(pName); }
inline int TextMessageDrawChar(int x, int y, int number, int r, int g, int b)
{
	return gEngfuncs.pfnDrawCharacter(x, y, number, r, g, b);
}

inline int DrawConsoleString(int x, int y, const char *string)
{
	return gEngfuncs.pfnDrawConsoleString(x, y, (char *)string);
}

inline void GetConsoleStringSize(const char *string, int *width, int *height)
{
	gEngfuncs.pfnDrawConsoleStringLen(string, width, height);
}

inline int ConsoleStringLen(const char *string)
{
	int _width, _height;
	GetConsoleStringSize(string, &_width, &_height);
	return _width;
}

inline void ConsolePrint(const char *string)
{
	gEngfuncs.pfnConsolePrint(string);
}

inline void CenterPrint(const char *string)
{
	gEngfuncs.pfnCenterPrint(string);
}

inline char *safe_strcpy(char *dst, const char *src, int len_dst)
{
	if (len_dst <= 0)
	{
		return NULL; // this is bad
	}

	strncpy(dst, src, len_dst);
	dst[len_dst - 1] = '\0';

	return dst;
}

// sound functions
inline void PlaySound(char *szSound, float vol) { gEngfuncs.pfnPlaySoundByName(szSound, vol); }
inline void PlaySound(int iSound, float vol) { gEngfuncs.pfnPlaySoundByIndex(iSound, vol); }

using std::abs;
using std::max;
using std::min;

void ScaleColors(int &r, int &g, int &b, int a);

float Length(const float *v);
void VectorInverse(float *v);

// disable 'possible loss of data converting float to int' warning message
#pragma warning(disable : 4244)
// disable 'truncation from 'const double' to 'float' warning message
#pragma warning(disable : 4305)

inline void UnpackRGB(int &r, int &g, int &b, unsigned long ulRGB)
{
	r = (ulRGB & 0xFF0000) >> 16;
	g = (ulRGB & 0xFF00) >> 8;
	b = ulRGB & 0xFF;
}

HSPRITE LoadSprite(const char *pszName);
