//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Functionality for the observer chase camera
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
// observer.cpp
//
#include "extdll.h"
#include "game.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#define NEXT_OBSERVER_INPUT_DELAY 0.02

extern int gmsgCurWeapon;
extern int gmsgSetFOV;
extern int gmsgTeamInfo;
extern int gmsgSpectator;

extern int g_teamplay;

//=========================================================
// Player has become a spectator. Set it up.
//=========================================================
void CBasePlayer::StartObserver(void)
{
	// clear any clientside entities attached to this player
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_KILLPLAYERATTACHMENTS);
	WRITE_BYTE(ENTINDEX(edict())); // index number of primary entity
	MESSAGE_END();

	// Let's go of tanks
	if (m_pTank != NULL)
		m_pTank->Use(this, this, USE_OFF, 0);

	// Remove all the player's stuff
	RemoveAllItems(FALSE);

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	MESSAGE_END();

	// reset FOV
	pev->fov = m_iFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
	WRITE_BYTE(m_iFOV);
	MESSAGE_END();

	// Store view offset to use it later
	Vector view_ofs = pev->view_ofs;

	// Setup flags
	m_iHideHUD = HIDEHUD_WEAPONS | HIDEHUD_HEALTH;
	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->effects = EF_NODRAW;
	pev->view_ofs = g_vecZero;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	ClearBits(m_afPhysicsFlags, PFLAG_DUCKING);
	ClearBits(pev->flags, FL_DUCKING);
	pev->deadflag = DEAD_RESPAWNABLE;
	pev->health = 1;

	// Clear out the status bar
	m_fInitHUD = TRUE;

	// Clear welcome cam status
	m_bInWelcomeCam = FALSE;

	// Move player to same view position he had on entering spectator
	UTIL_SetOrigin(pev, pev->origin + view_ofs);

	// Delay between observer inputs
	m_flNextObserverInput = 0;

	// Setup spectator mode
	Observer_SetMode(m_iObserverMode);

	// Update Team Status
	//pev->team = 0;
	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
	WRITE_BYTE(ENTINDEX(edict())); // index number of primary entity
	WRITE_STRING("");
	MESSAGE_END();

	// Send Spectator message (it is not used in client dll)
	MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
	WRITE_BYTE(ENTINDEX(edict())); // index number of primary entity
	WRITE_BYTE(1);
	MESSAGE_END();
}

//=========================================================
// Leave observer mode
//=========================================================
void CBasePlayer::StopObserver(void)
{
	// Turn off spectator
	pev->iuser1 = pev->iuser2 = 0;
	m_iHideHUD = 0;

	GetClassPtr((CBasePlayer *)pev)->Spawn();
	pev->nextthink = -1;

	// Send Spectator message (it is not used in client dll)
	MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
	WRITE_BYTE(ENTINDEX(edict())); // index number of primary entity
	WRITE_BYTE(0);
	MESSAGE_END();

	// Update Team Status
	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
	WRITE_BYTE(ENTINDEX(edict())); // index number of primary entity
	if (g_teamplay || g_amxmodx_version)
		WRITE_STRING(TeamID());
	else
		WRITE_STRING("Players");
	MESSAGE_END();
}

//=========================================================
// Attempt to change the observer mode
//=========================================================
void CBasePlayer::Observer_SetMode(int iMode)
{
	// Just abort if we're changing to the mode we're already in
	if (iMode == pev->iuser1)
		return;

	// is valid mode ?
	if (iMode < OBS_CHASE_LOCKED || iMode > OBS_MAP_CHASE)
		iMode = OBS_ROAMING; // now it is

	if (iMode == OBS_ROAMING && m_hObserverTarget)
	{
		// Set view point at same place where we may be looked in First Person mode
		pev->angles = m_hObserverTarget->pev->v_angle;
		pev->fixangle = TRUE;
		// Compensate view offset
		UTIL_SetOrigin(pev, m_hObserverTarget->pev->origin + m_hObserverTarget->pev->view_ofs);
	}

	// set spectator mode
	m_iObserverMode = iMode;
	pev->iuser1 = iMode;
	pev->iuser3 = 0;

	Observer_CheckTarget();

	// print spectator mode on client screen
	char modemsg[16];
	sprintf(modemsg, "#Spec_Mode%i", pev->iuser1);
	ClientPrint(pev, HUD_PRINTCENTER, modemsg);
}

// Find the next client in the game for this player to spectate
// If bOverview will be set then observer will be placed above targeted player looking down on him
void CBasePlayer::Observer_FindNextPlayer(bool bReverse, bool bOverview)
{
	// MOD AUTHORS: Modify the logic of this function if you want to restrict the observer to watching
	//				only a subset of the players. e.g. Make it check the target's team.

	int iStart;
	// If target not present or was a spot, set starting index to current player
	if (!m_hObserverTarget || (iStart = ENTINDEX(m_hObserverTarget->edict())) > gpGlobals->maxClients)
		iStart = ENTINDEX(edict());

	m_hObserverTarget = NULL;

	int iCurrent = iStart;
	int iDir = bReverse ? -1 : 1;
	do
	{
		iCurrent += iDir;

		// Loop through the clients
		if (iCurrent > gpGlobals->maxClients)
			iCurrent = 1;
		if (iCurrent < 1)
			iCurrent = gpGlobals->maxClients;

		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(iCurrent);
		if (!pPlayer)
			continue; // Don't spectate not connected players
		if (pPlayer == this)
			continue;
		// Don't spectate observers
		if (pPlayer->IsObserver())
			continue;

		m_hObserverTarget = pPlayer;
		break;

	} while (iCurrent != iStart);

	// Did we find a target?
	if (m_hObserverTarget)
	{
		if (bOverview)
		{
			// Move above the target
			UTIL_SetOrigin(pev, m_hObserverTarget->pev->origin + m_hObserverTarget->pev->view_ofs + Vector(0, 0, 14));
			// Fix angles
			pev->angles = m_hObserverTarget->pev->angles;
			pev->angles.x = 30;
			pev->fixangle = TRUE;
		}
		else
		{
			// Move to the target
			UTIL_SetOrigin(pev, m_hObserverTarget->pev->origin);
		}

		// Store the target in pev so the physics DLL can get to it
		if (pev->iuser1 != OBS_ROAMING && pev->iuser1 != OBS_MAP_FREE)
			pev->iuser2 = ENTINDEX(m_hObserverTarget->edict());
		else
			pev->iuser2 = 0;

		//ALERT(at_console, "Now Tracking %s\n", STRING(m_hObserverTarget->pev->netname));
	}
	else
	{
		//ClientPrint(pev, HUD_PRINTCENTER, "#Spec_NoTarget");
		pev->iuser2 = 0;

		//ALERT(at_console, "No observer targets.\n");
	}
}

// Find the next spot and move spectator to it
void CBasePlayer::Observer_FindNextSpot(bool bReverse)
{
	const int classesCount = 4;
	char *classes[] = { "info_intermission", "info_player_coop", "info_player_start", "info_player_deathmatch" };
	vec_t offsets[] = { 0, VEC_VIEW.z, VEC_VIEW.z, VEC_VIEW.z }; // View offset for spots (will looks like we spawn)

	int iStartClass = 0;
	if (m_hObserverTarget)
	{
		// Get current target's class
		const char *name = STRING(m_hObserverTarget->edict()->v.classname);
		// Pick starting class index
		for (int i = 0; i < classesCount; i++)
		{
			if (!strcmp(classes[i], name))
			{
				iStartClass = i;
				break;
			}
		}
	}

	CBaseEntity *pSpot = m_hObserverTarget, *pResultSpot = NULL;
	vec_t iResultSpotOffset;

	for (int i = 0; i < classesCount; i++)
	{
		int current = iStartClass + (bReverse ? -i : i);
		if (current >= classesCount)
			current -= classesCount;
		if (current < 0)
			current += classesCount;

		pSpot = UTIL_FindEntityByClassname(pSpot, classes[current], pSpot == NULL, bReverse);
		if (!pSpot)
			continue;

		// Spot found
		pResultSpot = pSpot;
		iResultSpotOffset = offsets[current];
		break;
	}

	if (!pResultSpot)
	{
		// Overlook a player (this will be never happen actually)
		Observer_FindNextPlayer(bReverse, true);
		return;
	}
	m_hObserverTarget = pResultSpot;

	// Move player there
	UTIL_SetOrigin(pev, m_hObserverTarget->pev->origin + Vector(0, 0, iResultSpotOffset));
	// Find target for intermission
	edict_t *pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(m_hObserverTarget->pev->target));
	if (pTarget && !FNullEnt(pTarget))
	{
		// Calculate angles to look at camera target
		pev->angles = UTIL_VecToAngles(pTarget->v.origin - m_hObserverTarget->pev->origin);
		pev->angles.x = -pev->angles.x;
	}
	else
	{
		pev->angles = m_hObserverTarget->pev->angles;
	}
	pev->fixangle = TRUE;
}

// Handle buttons in observer mode
void CBasePlayer::Observer_HandleButtons()
{
	// Slow down mouse and keyboard clicks
	if (m_flNextObserverInput > gpGlobals->time)
		return;

	// Jump changes view modes
	if (m_afButtonPressed & IN_JUMP)
	{
		int iMode;
		switch (pev->iuser1)
		{
		case OBS_CHASE_LOCKED:
			iMode = OBS_CHASE_FREE;
			break;
		case OBS_CHASE_FREE:
			iMode = OBS_MAP_CHASE;
			break;
		case OBS_ROAMING:
			iMode = OBS_IN_EYE;
			break;
		case OBS_IN_EYE:
			iMode = OBS_CHASE_LOCKED;
			break;
		case OBS_MAP_FREE:
			iMode = OBS_ROAMING;
			break;
		case OBS_MAP_CHASE:
			iMode = OBS_MAP_FREE;
			break;
		default:
			iMode = OBS_ROAMING;
		}
		Observer_SetMode(iMode);

		m_flNextObserverInput = gpGlobals->time + NEXT_OBSERVER_INPUT_DELAY;
	}

	// No switching of view point in Free Overview
	if (pev->iuser1 == OBS_MAP_FREE)
		return;

	// Attack moves to the next player/spot
	if (m_afButtonPressed & IN_ATTACK)
	{
		if (pev->iuser1 == OBS_ROAMING)
			Observer_FindNextSpot(false);
		else
			Observer_FindNextPlayer(false, false);

		m_flNextObserverInput = gpGlobals->time + NEXT_OBSERVER_INPUT_DELAY;
	}
	// Attack2 moves to the prev player/spot
	else if (m_afButtonPressed & IN_ATTACK2)
	{
		if (pev->iuser1 == OBS_ROAMING)
			Observer_FindNextSpot(true);
		else
			Observer_FindNextPlayer(true, false);

		m_flNextObserverInput = gpGlobals->time + NEXT_OBSERVER_INPUT_DELAY;
	}
}

void CBasePlayer::Observer_CheckTarget()
{
	if (pev->iuser1 != OBS_ROAMING && pev->iuser1 != OBS_MAP_FREE)
	{
		// Player tracking mode
		if (m_hObserverTarget)
		{
			// Don't spectate self, other spectators or not connected players. Also will be cleared if target is not a player (could be a spot)
			if (m_hObserverTarget == this || m_hObserverTarget->pev->iuser1 || !UTIL_PlayerByIndex(ENTINDEX(m_hObserverTarget->edict())))
			{
				// Set view point at same place where we may be looked in First Person mode
				pev->angles = m_hObserverTarget->pev->v_angle;
				pev->fixangle = TRUE;
				// Compensate view offset
				UTIL_SetOrigin(pev, m_hObserverTarget->pev->origin + m_hObserverTarget->pev->view_ofs);

				m_hObserverTarget = NULL;
			}
		}

		// if we are not roaming or free overview, we need a valid target to track
		if (m_hObserverTarget == NULL)
			Observer_FindNextPlayer(false, false);

		// if we didn't find a valid target switch to roaming
		if (m_hObserverTarget == NULL)
			Observer_SetMode(OBS_ROAMING);
		else
			// Store the target in pev so the physics DLL can get to it
			pev->iuser2 = ENTINDEX(m_hObserverTarget->edict());
	}
	else
	{
		// Clear target in pev if roaming or free overview
		pev->iuser2 = 0;
	}
}
