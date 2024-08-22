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

#ifndef GAME_H
#define GAME_H
#include "convar.h"

extern void GameDLLInit(void);

extern cvar_t displaysoundlist;

// Spectator settings
extern cvar_t allow_spectators;
extern cvar_t spectator_cmd_delay;

// multiplayer server rules
extern cvar_t teamplay;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t friendlyfire;
extern cvar_t bunnyhop;
extern cvar_t falldamage;
extern cvar_t weaponstay;
extern cvar_t selfgauss;
extern cvar_t forcerespawn;
extern cvar_t flashlight;
extern cvar_t aimcrosshair;
extern cvar_t decalfrequency;
extern cvar_t teamlist;
extern cvar_t teamoverride;
extern cvar_t defaultteam;
extern cvar_t allowmonsters;
extern cvar_t mp_notify_player_status;
extern cvar_t mp_welcomecam;
extern cvar_t mp_respawn_fix;

extern cvar_t mp_dmg_crowbar;
extern cvar_t mp_dmg_glock;
extern cvar_t mp_dmg_357;
extern cvar_t mp_dmg_mp5;
extern cvar_t mp_dmg_shotgun;
extern cvar_t mp_dmg_xbow_scope;
extern cvar_t mp_dmg_xbow_noscope;
extern cvar_t mp_dmg_rpg;
extern cvar_t mp_dmg_gauss_primary;
extern cvar_t mp_dmg_gauss_secondary;
extern cvar_t mp_dmg_egon;
extern cvar_t mp_dmg_hornet;
extern cvar_t mp_dmg_hgrenade;
extern cvar_t mp_dmg_satchel;
extern cvar_t mp_dmg_tripmine;
extern cvar_t mp_dmg_m203;

// Engine Cvars
extern cvar_t *g_psv_gravity;
extern cvar_t *g_psv_aim;
extern cvar_t *g_psv_allow_autoaim;
extern cvar_t *g_footsteps;

// AMXX
extern cvar_t *g_amxmodx_version;

#endif // GAME_H
