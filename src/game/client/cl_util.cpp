/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// util.cpp
//
// implementation of class-less helper functions
//

#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include "engine_builds.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

double sqrt(double x);

float Length(const float *v)
{
	int i;
	float length;

	length = 0;
	for (i = 0; i < 3; i++)
		length += v[i] * v[i];
	length = sqrt(length); // FIXME

	return length;
}

void VectorAngles(const float *forward, float *angles)
{
	float tmp, yaw, pitch;

	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

void VectorInverse(float *v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

int EngineFilteredClientCmd(const char *const pszCmdString)
{
	if (gHUD.GetEngineBuild() >= ENGINE_BUILD_ANNIVERSARY_FIRST)
		return gEngfuncs.pfnFilteredClientCmd(pszCmdString);
	else
		return gEngfuncs.pfnClientCmd(pszCmdString);
}

void ConsolePrint(const char *string)
{
	if (gHUD.GetColorCodeAction() == ColorCodeAction::Ignore)
		gEngfuncs.pfnConsolePrint(string);
	else
		gEngfuncs.pfnConsolePrint(RemoveColorCodes(string));
}

HSPRITE LoadSprite(const char *pszName)
{
	char sz[256];
	sprintf(sz, pszName, gHUD.m_iRes);
	return SPR_Load(sz);
}

bool ParseColor(const char *string, Color &color)
{
	Color newColor;
	const char *value = string;

	// Red
	{
		while (*value == ' ')
			value++;

		if (*value < '0' || *value > '9')
			return false;

		newColor[0] = atoi(value);

		value = strchr(value, ' ');
		if (value == NULL)
			return false;
	}

	// Green
	{
		while (*value == ' ')
			value++;

		if (*value < '0' || *value > '9')
			return false;

		newColor[1] = atoi(value);
		value = strchr(value, ' ');
		if (value == NULL)
			return false;
	}

	// Blue
	{
		while (*value == ' ')
			value++;

		if (*value < '0' || *value > '9')
			return false;

		newColor[2] = atoi(value);
	}

	newColor[3] = 255;
	color = newColor;
	return true;
}

//-------------------------------------------------------------------
// Text drawing in console font
//-------------------------------------------------------------------
int DrawConsoleString(int x, int y, char *string, const float *colorOverride)
{
	// How colorcodes work in DrawConsoleString
	// 1) If float *color is set (e.g. team color), it is used, colorcodes are ignored.
	// 2) Otherwise, colorcodes ^0 and ^9 reset color to con_color.

	if (!string || !*string)
		return x;

	if (colorOverride)
	{
		gEngfuncs.pfnDrawSetTextColor(colorOverride[0], colorOverride[1], colorOverride[2]);
	}
	else
		gEngfuncs.pfnDrawConsoleString(x, y, " "); // Reset color to con_color

	if (gHUD.GetColorCodeAction() == ColorCodeAction::Ignore)
		return gEngfuncs.pfnDrawConsoleString(x, y, string);

	char *c1 = string;
	char *c2 = string;
	float r, g, b;
	int colorIndex;
	while (true)
	{
		// Search for next color code
		colorIndex = -1;
		while (*c2 && *(c2 + 1) && !IsColorCode(c2))
			c2++;

		if (IsColorCode(c2))
		{
			colorIndex = *(c2 + 1) - '0';
			*c2 = 0;
		}

		// Draw current string
		x = gEngfuncs.pfnDrawConsoleString(x, y, c1);

		if (colorIndex >= 0)
		{
			// Revert change and advance
			*c2 = '^';
			c2 += 2;
			c1 = c2;

			// Return if next string is empty
			if (!*c1)
				return x;

			// Setup color
			if (!colorOverride && colorIndex <= 9 && gHUD.GetColorCodeAction() == ColorCodeAction::Handle)
			{
				if (colorIndex == 0 || colorIndex == 9)
				{
					gEngfuncs.pfnDrawConsoleString(x, y, " "); // Reset color to con_color
				}
				else
				{
					r = gHUD.GetColorCodeColor(colorIndex)[0] / 255.0;
					g = gHUD.GetColorCodeColor(colorIndex)[1] / 255.0;
					b = gHUD.GetColorCodeColor(colorIndex)[2] / 255.0;
					gEngfuncs.pfnDrawSetTextColor(r, g, b);
				}
			}
			else if (colorOverride)
			{
				gEngfuncs.pfnDrawSetTextColor(colorOverride[0], colorOverride[1], colorOverride[2]);
			}
			continue;
		}

		// Done
		break;
	}
	return x;
}

void GetConsoleStringSize(const char *string, int *width, int *height)
{
	if (gHUD.GetColorCodeAction() == ColorCodeAction::Ignore)
		gEngfuncs.pfnDrawConsoleStringLen(string, width, height);
	else
		gEngfuncs.pfnDrawConsoleStringLen(RemoveColorCodes(string), width, height);
}

//-------------------------------------------------------------------
// Color code utilities
//-------------------------------------------------------------------
void RemoveColorCodesInPlace(char *string)
{
	char *c1 = string;
	char *c2 = string;
	while (*c2)
	{
		if (IsColorCode(c2))
		{
			c2 += 2;
			continue;
		}
		*c1 = *c2;
		c1++;
		c2++;
	}
	*c1 = 0;
}

void RemoveColorCodes(const char *string, char *buf, size_t bufSize)
{
	char *c1 = buf;
	const char *c2 = string;
	char *end = buf + bufSize - 1;
	while (*c2 && c1 < end)
	{
		if (*c2 == '^' && *(c2 + 1) >= '0' && *(c2 + 1) <= '9')
		{
			c2 += 2;
			continue;
		}
		*c1 = *c2;
		c1++;
		c2++;
	}
	*c1 = 0;
}

const char *RemoveColorCodes(const char *string)
{
	static char buffer[1024];
	RemoveColorCodes(string, buffer, sizeof(buffer));
	return buffer;
}
