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
//  cl_dll.h
//

// 4-23-98  JOHN

//
//  This DLL is linked by the client when they first initialize.
// This DLL is responsible for the following tasks:
//		- Loading the HUD graphics upon initialization
//		- Drawing the HUD graphics every frame
//		- Handling the custum HUD-update packets
//
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int func_t; //
typedef unsigned int string_t; // from engine's pr_comp.h;
typedef float vec_t;

#include <mathlib/mathlib.h>

#ifndef EXPORT
#ifdef _WIN32
#define EXPORT _declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
#endif

#include "../engine/cdll_int.h"
#include "cdll_dll.h"

extern cl_enginefunc_t gEngfuncs;

//! Causes a fatal client error. Shows a dialog and crashes the game.
//! @param  fmt     Printf arguments.
[[noreturn]] void HUD_FatalError(const char *fmt, ...);
