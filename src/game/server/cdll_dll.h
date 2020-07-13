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
//
//  cdll_dll.h

// this file is included by both the game-dll and the client-dll,

#ifndef CDLL_DLL_H
#define CDLL_DLL_H

#define MAX_WEAPONS     32 // ???
#define MAX_WEAPON_NAME 128

#define MAX_WEAPON_POSITIONS 10 // this is the max number of weapon items in each bucket on a client
#define MAX_WEAPON_SLOTS     6 // hud item selection slots on a client
#define MAX_ITEM_TYPES       6 // weapont item slots on a server

#define MAX_ITEMS 5 // hard coded item types

#define HIDEHUD_WEAPONS    (1 << 0)
#define HIDEHUD_FLASHLIGHT (1 << 1)
#define HIDEHUD_ALL        (1 << 2)
#define HIDEHUD_HEALTH     (1 << 3)

#define MAX_AMMO_TYPES 32 // ???
#define MAX_AMMO_SLOTS 32 // not really slots

#define HUD_PRINTNOTIFY  1
#define HUD_PRINTCONSOLE 2
#define HUD_PRINTTALK    3
#define HUD_PRINTCENTER  4

#define WEAPON_SUIT 31

enum
{
	MAX_PLAYERS = 64,
	MAX_PLAYER_NAME = 32,
	MAX_TEAMS = 64,
	MAX_TEAM_NAME = 16,
};

#endif
