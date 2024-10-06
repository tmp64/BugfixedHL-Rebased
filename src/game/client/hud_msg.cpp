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
//  hud_msg.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "hud/ammo.h"
#include "hud/status_icons.h"
#include "hud/ammo.h"
#include "engine_builds.h"

#include "particleman.h"
extern IParticleMan *g_pParticleMan;

#include "fog.h"

#define MAX_CLIENTS 32

#if !defined(_TFC)
extern BEAM *pBeam;
extern BEAM *pBeam2;
#endif

#if defined(_TFC)
void ClearEventList(void);
#endif

extern ConVar zoom_sensitivity_ratio;
extern float g_lastFOV;

cvar_t *cl_lw = nullptr;

void CAM_ToFirstPerson(void);
float IN_GetMouseSensitivity();

/// USER-DEFINED SERVER MESSAGE HANDLERS

int CHud::MsgFunc_InitHUD(const char *pszName, int iSize, void *pbuf)
{
	// prepare all hud data
	for (CHudElem *i : m_HudList)
		i->InitHudData();

#if defined(_TFC)
	ClearEventList();

	// catch up on any building events that are going on
	gEngfuncs.pfnServerCmd("sendevents");
#endif

	if (g_pParticleMan)
		g_pParticleMan->ResetParticles();

#if !defined(_TFC)
	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;
#endif

	return 1;
}

int CHud::MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf)
{
	// clear all hud data
	for (CHudElem *i : m_HudList)
		i->Reset();

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	return 1;
}

int CHud::MsgFunc_Logo(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// update Train data
	m_iLogo = READ_BYTE();

	return 1;
}

int CHud::MsgFunc_Damage(const char *pszName, int iSize, void *pbuf)
{
	int armor, blood;
	Vector from;
	int i;
	float count;

	BEGIN_READ(pbuf, iSize);
	armor = READ_BYTE();
	blood = READ_BYTE();

	for (i = 0; i < 3; i++)
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);

	if (count < 10)
		count = 10;

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud::MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_Teamplay = READ_BYTE();

	if (GetEngineBuild() >= ENGINE_BUILD_ANNIVERSARY_FIRST)
	{
		if (m_Teamplay)
			gEngfuncs.pfnClientCmd("richpresence_gamemode Teamplay\n");
		else
			gEngfuncs.pfnClientCmd("richpresence_gamemode\n"); // reset

		gEngfuncs.pfnClientCmd("richpresence_update\n");
	}

	return 1;
}

int CHud::MsgFunc_ViewMode(const char *pszName, int iSize, void *pbuf)
{
	CAM_ToFirstPerson();
	return 1;
}

int CHud::MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int newfov = READ_BYTE();
	int def_fov = default_fov.GetInt();

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	if (cl_lw && cl_lw->value)
		return 1;

	g_lastFOV = newfov;

	if (newfov == 0)
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if (m_iFOV == def_fov)
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = IN_GetMouseSensitivity() * ((float)newfov / (float)def_fov) * zoom_sensitivity_ratio.GetFloat();
	}

	// Update crosshair after zoom change
	CHudAmmo::Get()->UpdateCrosshair();

	return 1;
}

int CHud::MsgFunc_Concuss(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iConcussionEffect = READ_BYTE();
	if (m_iConcussionEffect)
		CHudStatusIcons::Get()->EnableIcon("dmg_concuss", 255, 160, 0);
	else
		CHudStatusIcons::Get()->DisableIcon("dmg_concuss");
	return 1;
}

int CHud::MsgFunc_Fog(const char *pszName, int iSize, void *pbuf)
{
	FogParams fogParams;
	float density;

	// Reset fog parameters
	gFog.ClearFog();

	BEGIN_READ(pbuf, iSize);

	fogParams.color[0] = (float)READ_BYTE(); // r
	fogParams.color[1] = (float)READ_BYTE(); // g
	fogParams.color[2] = (float)READ_BYTE(); // b

	density = READ_FLOAT();

	if (READ_OK())
	{
		fogParams.density = density;
		fogParams.fogSkybox = true;
		gFog.SetFogParameters(fogParams);
	}
	else
	{
		gFog.ClearFog();
	}

	return 1;
}
