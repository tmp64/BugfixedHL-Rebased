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

#include "com_model.h"

CSysModule *g_hParticleManModule = NULL;
IParticleMan *g_pParticleMan = NULL;

extern ConVar cl_show_server_triggers;
extern ConVar cl_show_server_triggers_alpha;

void DivideRGBABy255(float &r, float &g, float &b, float &a)
{
	r /= 255.0f;
	g /= 255.0f;
	b /= 255.0f;
	a /= 255.0f;
}

void DrawAACuboid(triangleapi_s *pTriAPI, Vector corner1, Vector corner2)
{
	pTriAPI->Begin(TRI_QUADS);

	pTriAPI->Vertex3f(corner1.x, corner1.y, corner1.z);
	pTriAPI->Vertex3f(corner1.x, corner2.y, corner1.z);
	pTriAPI->Vertex3f(corner2.x, corner2.y, corner1.z);
	pTriAPI->Vertex3f(corner2.x, corner1.y, corner1.z);

	pTriAPI->Vertex3f(corner1.x, corner1.y, corner1.z);
	pTriAPI->Vertex3f(corner1.x, corner1.y, corner2.z);
	pTriAPI->Vertex3f(corner1.x, corner2.y, corner2.z);
	pTriAPI->Vertex3f(corner1.x, corner2.y, corner1.z);

	pTriAPI->Vertex3f(corner1.x, corner1.y, corner1.z);
	pTriAPI->Vertex3f(corner2.x, corner1.y, corner1.z);
	pTriAPI->Vertex3f(corner2.x, corner1.y, corner2.z);
	pTriAPI->Vertex3f(corner1.x, corner1.y, corner2.z);

	pTriAPI->Vertex3f(corner2.x, corner2.y, corner2.z);
	pTriAPI->Vertex3f(corner1.x, corner2.y, corner2.z);
	pTriAPI->Vertex3f(corner1.x, corner1.y, corner2.z);
	pTriAPI->Vertex3f(corner2.x, corner1.y, corner2.z);

	pTriAPI->Vertex3f(corner2.x, corner2.y, corner2.z);
	pTriAPI->Vertex3f(corner2.x, corner1.y, corner2.z);
	pTriAPI->Vertex3f(corner2.x, corner1.y, corner1.z);
	pTriAPI->Vertex3f(corner2.x, corner2.y, corner1.z);

	pTriAPI->Vertex3f(corner2.x, corner2.y, corner2.z);
	pTriAPI->Vertex3f(corner2.x, corner2.y, corner1.z);
	pTriAPI->Vertex3f(corner1.x, corner2.y, corner1.z);
	pTriAPI->Vertex3f(corner1.x, corner2.y, corner2.z);

	pTriAPI->End();
}

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

void DrawServerTriggers()
{
	if ((cl_show_server_triggers.GetBool()) && (cl_show_server_triggers.GetInt() != 2))
	{
		for (int e = 0; e < MAX_EDICTS; ++e)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(e);
			if (ent)
			{
				if (ent->model)
				{
					if ((ent->curstate.rendermode == kRenderTransColor) && (ent->curstate.renderfx == kRenderFxTrigger))
					{
						color24 colors = ent->curstate.rendercolor;
						if (!gHUD.IsTriggerForSinglePlayer(colors))
						{
							gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
							gEngfuncs.pTriAPI->CullFace(TRI_NONE);

							float r = colors.r, g = colors.g, b = colors.b, a = std::clamp(cl_show_server_triggers_alpha.GetFloat(), 0.0f, 255.0f);
							DivideRGBABy255(r, g, b, a);
							gEngfuncs.pTriAPI->Color4f(r, g, b, a);

							Vector mins = ent->curstate.mins;
							Vector maxs = ent->curstate.maxs;
							Vector origin = ent->curstate.origin;
							Vector absmin = origin + mins;
							Vector absmax = origin + maxs;

							DrawAACuboid(gEngfuncs.pTriAPI, absmin, absmax);
						}
					}
				}
			}
		}
	}
}

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

	if (gHUD.white_sprite == 0)
		return;

	if (gEngfuncs.pTriAPI->SpriteTexture(const_cast<model_s*>(gEngfuncs.GetSpritePointer(gHUD.white_sprite)), 0))
	{
		DrawServerTriggers();

		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	}
}
