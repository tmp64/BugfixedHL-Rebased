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
// pm_shared.h
//
#ifndef PM_SHARED_H
#define PM_SHARED_H

enum class EUseSlowDownType
{
	//! Old method (before HL25). Used by old servers and AG.
	//! Player immediately slows down if +use is held down.
	Old,

	//! New method from HL25.
	//! Player slowly slows down if +use is held down.
	New,

#ifdef CLIENT_DLL
	//! Detect automatically.
	AutoDetect,
#endif

#ifdef CLIENT_DLL
	_Min = Old,
	_Max = AutoDetect,
#else
	_Min = Old,
	_Max = New,
#endif
};

void PM_Init(struct playermove_s *ppmove);
void PM_Move(struct playermove_s *ppmove, int server);
char PM_FindTextureType(char *name);

void PM_SetIsAG(int state);

EUseSlowDownType PM_GetUseSlowDownType();
void PM_SetUseSlowDownType(EUseSlowDownType value);

#ifdef CLIENT_DLL
enum class EBHopCap
{
	Disabled = 0,
	Enabled = 1,
	AutoDetect = 2,

	_Min = Disabled,
	_Max = AutoDetect,
};

int PM_GetOnGround();
int PM_GetWaterLevel();
int PM_GetMoveType();
EBHopCap PM_GetBHopCapState();
void PM_SetBHopCapState(EBHopCap state);
void PM_ResetBHopDetection();
void PM_ResetUseSlowDownDetection();
#else
int PM_GetBHopCapEnabled();
void PM_SetBHopCapEnabled(int state);
#endif

// Spectator Movement modes (stored in pev->iuser1, so the physics code can get at them)
#define OBS_NONE         0
#define OBS_CHASE_LOCKED 1
#define OBS_CHASE_FREE   2
#define OBS_ROAMING      3
#define OBS_IN_EYE       4
#define OBS_MAP_FREE     5
#define OBS_MAP_CHASE    6

#ifdef CLIENT_DLL
// Spectator Mode
extern int pm_iJumpSpectator;
#ifndef DISABLE_JUMP_ORIGIN
extern float pm_vJumpOrigin[3];
extern float pm_vJumpAngles[3];
#endif
#endif

#endif
