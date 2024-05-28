//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"
#include "hud/spectator.h"

#include "particleman.h"
#include "tri.h"
#include "fog.h"

CSysModule *g_hParticleManModule = NULL;
IParticleMan *g_pParticleMan = NULL;

//---------------------------------------------------
// Particle Manager
//---------------------------------------------------
void CL_LoadParticleMan()
{
	char szPDir[512];

	if (gEngfuncs.COM_ExpandFilename(PARTICLEMAN_DLLNAME, szPDir, sizeof(szPDir)) == FALSE)
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_hParticleManModule = Sys_LoadModule(szPDir);
	CreateInterfaceFn particleManFactory = Sys_GetFactory(g_hParticleManModule);

	if (particleManFactory == NULL)
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_pParticleMan = (IParticleMan *)particleManFactory(PARTICLEMAN_INTERFACE, NULL);

	if (g_pParticleMan)
	{
		g_pParticleMan->SetUp(&gEngfuncs);

		// Add custom particle classes here BEFORE calling anything else or you will die.
		g_pParticleMan->AddCustomParticleClassSize(sizeof(CBaseParticle));
	}
}

void CL_UnloadParticleMan()
{
	Sys_UnloadModule(g_hParticleManModule);

	g_pParticleMan = NULL;
	g_hParticleManModule = NULL;
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void CL_DLLEXPORT HUD_DrawNormalTriangles(void)
{
	//	RecClDrawNormalTriangles();

	CHudSpectator::Get()->DrawOverview();
}

#if defined(_TFC)
void RunEventList(void);
#endif

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void CL_DLLEXPORT HUD_DrawTransparentTriangles(void)
{
	//	RecClDrawTransparentTriangles();

#if defined(_TFC)
	RunEventList();
#endif

	gFog.RenderFog();

	if (g_pParticleMan)
		g_pParticleMan->Update();
}
