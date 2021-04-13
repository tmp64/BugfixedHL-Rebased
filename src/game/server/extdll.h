/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
#ifndef EXTDLL_H
#define EXTDLL_H

//
// Global header file for extension DLLs
//

// Allow "DEBUG" in addition to default "_DEBUG"
#ifdef _DEBUG
#define DEBUG 1
#endif

// Silence certain warnings
#pragma warning(disable : 4244) // int or float down-conversion
#pragma warning(disable : 4305) // int or float data truncation
#pragma warning(disable : 4201) // nameless struct/union
#pragma warning(disable : 4514) // unreferenced inline function removed
#pragma warning(disable : 4100) // unreferenced formal parameter

#if defined(_WIN32) && defined(USE_METAMOD)

// Metamod requires windows.h on Windows
// Defines prevent tons of unused windows definitions
#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOMINMAX
#include "winsani_in.h"
#include "windows.h"
#include "winsani_out.h"

#endif

#include "archtypes.h" // DAL
#include <tier0/platform.h>

// Misc C-runtime library headers
#include "stdio.h"
#include "stdlib.h"
#include "math.h"

// Min/max
#include <algorithm>
using std::max;
using std::min;

// Header file containing definition of globalvars_t and entvars_t
typedef unsigned int func_t; //
typedef unsigned int string_t; // from engine's pr_comp.h;
typedef float vec_t; // needed before including progdefs.h

// Source SDK mathlib
#include <mathlib/mathlib.h>

#ifdef USE_METAMOD
// Defining it as a (bogus) struct helps enforce type-checking
// Only used in Metamod headers
using vec3_t = Vector;
#endif

// Shared engine/DLL constants
#include "const.h"
#include "progdefs.h"
#include "edict.h"

// Shared header describing protocol between engine and DLLs
#include "eiface.h"

// Shared header between the client DLL and the game DLLs
#include "cdll_dll.h"

#endif //EXTDLL_H
