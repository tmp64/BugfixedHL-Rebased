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

#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

const Vector vec3_origin(0, 0, 0);

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

HSPRITE LoadSprite(const char *pszName)
{
	int i;
	char sz[256];

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

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
