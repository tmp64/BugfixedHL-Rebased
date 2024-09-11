//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "Exports.h"

#include "hud/spectator.h"

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

ConVar cl_forceenemymodels("cl_forceenemymodels", "", FCVAR_BHL_ARCHIVE, "List of allowed enemy models (e.g. \"red;zombie\")");
ConVar cl_forceenemycolors("cl_forceenemycolors", "", FCVAR_BHL_ARCHIVE, "Forced enemy colors (e.g. \"90 30\")");
ConVar cl_forceteammatesmodels("cl_forceteammatesmodels", "", FCVAR_BHL_ARCHIVE, "Forced teammates model (e.g. \"blue\")");
ConVar cl_forceteammatescolors("cl_forceteammatescolors", "", FCVAR_BHL_ARCHIVE, "Forced teammates color (e.g. \"90 30\")");

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;

// The renderer object, created on the stack.
CGameStudioModelRenderer g_StudioRenderer;

CON_COMMAND(forcemodel, "Force changes a model of a player")
{
	g_StudioRenderer.ForceModelCommand();
}

CON_COMMAND(forcecolor, "Force changes colors of a player")
{
	g_StudioRenderer.ForceColorsCommand();
}

CON_COMMAND(dev_model_debug, "Prints debug info for model enforcement")
{
	g_StudioRenderer.PrintDebugInfo();
}

/*
====================
CGameStudioModelRenderer

====================
*/
CGameStudioModelRenderer::CGameStudioModelRenderer(void)
{
}

/*
====================
Init

====================
*/
void CGameStudioModelRenderer::Init(void)
{
	// Call base Init function
	CStudioModelRenderer::Init();
}

///
/// InitOnConnect
///
void CGameStudioModelRenderer::InitOnConnect(void)
{
	// Reset internal data
	m_iLocalPlayerIndex = 0;
	m_iEnemyModelsCount = 0;
	m_iLastUsedEnemyModel = 0;
	m_szEnemyModelsList[0] = 0;
	memset(m_szEnemyModels, 0, sizeof(m_szEnemyModels));
	memset(m_szPlayerActualModel, 0, sizeof(m_szPlayerActualModel));
	memset(m_szPlayerRemapModel, 0, sizeof(m_szPlayerRemapModel));
	memset(m_rgbPlayerRemapModelForced, 0, sizeof(m_rgbPlayerRemapModelForced));

	m_iEnemyTopColor = 0;
	m_iEnemyBottomColor = 0;
	m_iTeammatesTopColor = 0;
	m_iTeammatesBottomColor = 0;
	memset(m_szEnemyColor, 0, sizeof(m_szEnemyColor));
	memset(m_szTeammatesColor, 0, sizeof(m_szTeammatesColor));
	memset(m_rgiPlayerRemapColors, 0, sizeof(m_rgiPlayerRemapColors));
	memset(m_rgbPlayerRemapColorsForced, 0, sizeof(m_rgbPlayerRemapColorsForced));
}

///
/// Parses enemy models list cvar if it changed.
///
int CGameStudioModelRenderer::ParseModels(void)
{
	if (!Q_stricmp(m_szEnemyModelsList, cl_forceenemymodels.GetString()) && !Q_stricmp(m_szTeammatesModel, cl_forceteammatesmodels.GetString()))
		return m_iEnemyModelsCount;
	strncpy(m_szEnemyModelsList, cl_forceenemymodels.GetString(), sizeof(m_szEnemyModelsList));
	m_szEnemyModelsList[sizeof(m_szEnemyModelsList) - 1] = 0;
	strncpy(m_szTeammatesModel, cl_forceteammatesmodels.GetString(), sizeof(m_szTeammatesModel));
	m_szTeammatesModel[sizeof(m_szTeammatesModel) - 1] = 0;

	m_iEnemyModelsCount = 0;
	m_iLastUsedEnemyModel = 0;
	memset(m_szEnemyModels, 0, sizeof(m_szEnemyModels));
	memset(m_szPlayerActualModel, 0, sizeof(m_szPlayerActualModel));
	memset(m_szPlayerRemapModel, 0, sizeof(m_szPlayerRemapModel));

	// Check for teammates replacement model
	char path[256];
	if (m_szTeammatesModel[0])
	{
		sprintf(path, "models/player/%s/%s.mdl", m_szTeammatesModel, m_szTeammatesModel);
		m_pTeammatesModel = IEngineStudio.Mod_ForName(path, 0);
	}
	else
	{
		m_pTeammatesModel = NULL;
	}

	// Parse enemy models list
	char buffer[MAX_TEAM_NAME];
	char *buffer_end = buffer + sizeof(buffer) - 1;
	for (char *src = m_szEnemyModelsList, *dst = buffer; *src != 0;)
	{
		while (*src != ';' && *src != 0)
		{
			if (dst < buffer_end)
			{
				*dst = *src;
				dst++;
			}
			src++;
		}
		*dst = 0;
		dst = buffer;
		if (*src == ';')
			src++;
		if (buffer[0] == 0)
			continue;

		// Don't add teammates model to a list
		if (!Q_stricmp(m_szTeammatesModel, buffer))
		{
			ConsolePrint("\"");
			ConsolePrint(buffer);
			ConsolePrint("\" is excluded from cl_forceenemymodels because it is in cl_forceteammatesmodel\n");
			continue;
		}

		bool found = false;
		for (int j = 0; j < m_iEnemyModelsCount; j++)
		{
			if (!Q_stricmp(m_szEnemyModels[j], buffer))
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			// Check that model exists
			sprintf(path, "models/player/%s/%s.mdl", buffer, buffer);
			model_t *model = IEngineStudio.Mod_ForName(path, 0);
			if (!model)
				continue;
			// Add to list
			V_strcpy_safe(m_szEnemyModels[m_iEnemyModelsCount], buffer);
			m_iEnemyModelsCount++;
			if (m_iEnemyModelsCount >= MAX_TEAMS)
				break;
		}
	}

	return m_iEnemyModelsCount;
}

///
/// Parses enemy and teammates color cvars if they are changed.
///
void CGameStudioModelRenderer::ParseColors(void)
{
	if (!Q_stricmp(m_szEnemyColor, cl_forceenemycolors.GetString()) && !Q_stricmp(m_szTeammatesColor, cl_forceteammatescolors.GetString()))
		return;

	strncpy(m_szEnemyColor, cl_forceenemycolors.GetString(), sizeof(m_szEnemyColor));
	m_szEnemyColor[sizeof(m_szEnemyColor) - 1] = 0;
	strncpy(m_szTeammatesColor, cl_forceteammatescolors.GetString(), sizeof(m_szTeammatesColor));
	m_szTeammatesColor[sizeof(m_szTeammatesColor) - 1] = 0;

	char *enemyBottom = strchr(m_szEnemyColor, ' ');
	char *teammatesBottom = strchr(m_szTeammatesColor, ' ');

	m_iEnemyTopColor = clamp(atoi(m_szEnemyColor), 0, 254);
	m_iEnemyBottomColor = enemyBottom != NULL ? clamp(atoi(enemyBottom), 0, 254) : 0;
	m_iTeammatesTopColor = clamp(atoi(m_szTeammatesColor), 0, 254);
	m_iTeammatesBottomColor = teammatesBottom != NULL ? clamp(atoi(teammatesBottom), 0, 254) : 0;
}

///
/// Return true if players are teammates
///
bool CGameStudioModelRenderer::AreTeammates(int playerIndex1, int playerIndex2)
{
	CPlayerInfo *pl1 = GetPlayerInfo(playerIndex1);
	CPlayerInfo *pl2 = GetPlayerInfo(playerIndex2);
	return pl1->IsConnected() && pl2->IsConnected() && pl1->GetTeamNumber() > 0 && pl1->GetTeamNumber() == pl2->GetTeamNumber();
}

///
/// Returns next enemy model from the list.
///
char *CGameStudioModelRenderer::GetNextEnemyModel(void)
{
	for (int j = 0; j < m_iEnemyModelsCount; j++)
	{
		// Check if model is unused
		bool used = false;
		int maxClients = gEngfuncs.GetMaxClients();
		for (int i = 0; i < maxClients; i++)
		{
			if (!CHudSpectator::Get()->IsActivePlayer(gEngfuncs.GetEntityByIndex(i + 1)))
				continue;
			if (!Q_stricmp(m_szEnemyModels[j], m_szPlayerRemapModel[i]))
			{
				used = true;
				break;
			}
		}
		if (!used)
		{
			m_iLastUsedEnemyModel = j;
			return m_szEnemyModels[j];
		}
	}
	m_iLastUsedEnemyModel++;
	if (m_iLastUsedEnemyModel >= m_iEnemyModelsCount)
		m_iLastUsedEnemyModel = 0;
	return m_szEnemyModels[m_iLastUsedEnemyModel];
}

void CGameStudioModelRenderer::PrintDebugInfo()
{
	CPlayerInfo *actuallocalpi = GetThisPlayerInfo();
	int localplayer = (actuallocalpi->IsSpectator() && g_iUser2 > 0) ? g_iUser2 : m_iLocalPlayerIndex;

	if (localplayer == 0)
	{
		ConPrintf("Local player unknown.\n");
		return;
	}

	CPlayerInfo *localpi = GetPlayerInfo(localplayer);
	ConPrintf("Local player: %s [%d]\n", localpi->GetName(), localpi->GetIndex());
	ConPrintf("Local team: %d %s\n", localpi->GetTeamNumber(), localpi->GetTeamName());

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i + 1);

		if (!pi->IsConnected())
			continue;

		ConPrintf("%s [%d]\n", pi->GetName(), pi->GetIndex());
		ConPrintf("    Real Model: %s\n", pi->GetModel());
		ConPrintf("    Is teammate: %d\n", (int)AreTeammates(i + 1, localplayer));
		ConPrintf("    Team: %d %s\n", pi->GetTeamNumber(), pi->GetTeamName());
		ConPrintf("    Replaced model: %s\n", GetPlayerModel(i)->name);
	}
}

///
/// Returns player model to render that is remapped if needed.
///  :playerIndex is zero based
///
model_t *CGameStudioModelRenderer::GetPlayerModel(int playerIndex)
{
	char buffer[256];
	char actualModelName[MAX_TEAM_NAME];
	model_t *actualModel = IEngineStudio.SetupPlayerModel(playerIndex);

	// Return actual model for local player
	if (!m_iLocalPlayerIndex)
		m_iLocalPlayerIndex = gEngfuncs.GetLocalPlayer()->index;
	if (m_iLocalPlayerIndex - 1 == playerIndex)
		return actualModel;

	// Return forced model
	if (m_rgbPlayerRemapModelForced[playerIndex])
		return m_rgpPlayerRemapModel[playerIndex];

	// Check if local player changed the model
	CPlayerInfo *pLocalPlayerInfo = GetPlayerInfo(m_iLocalPlayerIndex)->Update();

	if (pLocalPlayerInfo->IsConnected() && pLocalPlayerInfo->GetModel() != NULL && Q_stricmp(m_szPlayerActualModel[m_iLocalPlayerIndex - 1], pLocalPlayerInfo->GetModel()))
	{
		// Clear out stale mappings
		int maxClients = gEngfuncs.GetMaxClients();
		for (int i = 0; i < maxClients; i++)
		{
			if (!Q_stricmp(m_szPlayerActualModel[i], pLocalPlayerInfo->GetModel()))
				m_szPlayerRemapModel[i][0] = 0;
		}

		// Store actual model name for future comparisions
		strncpy(m_szPlayerActualModel[m_iLocalPlayerIndex - 1], pLocalPlayerInfo->GetModel(), MAX_TEAM_NAME - 1);
		m_szPlayerActualModel[m_iLocalPlayerIndex - 1][MAX_TEAM_NAME - 1] = 0;
	}

	// Ensure we have actual models info
	ParseModels();

	// Return teammates override model or actual model for local player teammates if this is a teamplay
	// We will not check for gHUD.m_Teamplay because customized sever dll send us "teamplay 1" to enable teams in scoreboard
	// note: if we are spectating someone, then use his team to show forcemodels.
	if (AreTeammates(playerIndex + 1, (pLocalPlayerInfo->IsSpectator() && g_iUser2 > 0) ? g_iUser2 : m_iLocalPlayerIndex))
	{
		if (m_pTeammatesModel)
			return m_pTeammatesModel;
		return actualModel;
	}

	// Return actual model if enemy models list isn't set
	if (m_iEnemyModelsCount == 0)
		return actualModel;

	// Get player model name
	CPlayerInfo *pPlayerInfo = GetPlayerInfo(playerIndex + 1)->Update();
	if (!pPlayerInfo->IsConnected() || pPlayerInfo->GetModel() == NULL)
		return actualModel; // Fallback
	strncpy(actualModelName, pPlayerInfo->GetModel(), MAX_TEAM_NAME - 1);
	actualModelName[MAX_TEAM_NAME - 1] = 0;

	// Check if model was changed by a player or he have no replacement model
	if (Q_stricmp(m_szPlayerActualModel[playerIndex], actualModelName) || m_szPlayerRemapModel[playerIndex][0] == 0)
	{
		// Store actual model name for future comparisions
		V_strcpy_safe(m_szPlayerActualModel[playerIndex], actualModelName);

		// Pickup new mapping
		m_szPlayerRemapModel[playerIndex][0] = 0;

		// Look among existing mappings, so players with equal models will get same replacement model
		int maxClients = gEngfuncs.GetMaxClients();
		for (int i = 0; i < maxClients; i++)
		{
			if (i == playerIndex || !CHudSpectator::Get()->IsActivePlayer(gEngfuncs.GetEntityByIndex(i + 1)) || Q_stricmp(m_szPlayerActualModel[i], actualModelName))
				continue;
			V_strcpy_safe(m_szPlayerRemapModel[playerIndex], m_szPlayerRemapModel[i]);
			break;
		}

		if (m_szPlayerRemapModel[playerIndex][0] == 0)
		{
			// Lookup, may be model is in allow list
			for (int j = 0; j < m_iEnemyModelsCount; j++)
			{
				if (!Q_stricmp(m_szEnemyModels[j], actualModelName))
				{
					V_strcpy_safe(m_szPlayerRemapModel[playerIndex], m_szEnemyModels[j]);
					break;
				}
			}

			// Pickup replacement model name
			if (m_szPlayerRemapModel[playerIndex][0] == 0)
				V_strcpy_safe(m_szPlayerRemapModel[playerIndex], GetNextEnemyModel());
		}

		// Retrive replacement model
		model_t *model = NULL;
		if (m_szPlayerRemapModel[playerIndex][0])
		{
			sprintf(buffer, "models/player/%s/%s.mdl", m_szPlayerRemapModel[playerIndex], m_szPlayerRemapModel[playerIndex]);
			m_rgpPlayerRemapModel[playerIndex] = IEngineStudio.Mod_ForName(buffer, 0);
		}
	}

	if (!m_rgpPlayerRemapModel[playerIndex])
		return actualModel; // Fallback

	return m_rgpPlayerRemapModel[playerIndex];
}

///
/// Sets player remapped colors for render.
///
void CGameStudioModelRenderer::SetPlayerRemapColors(int playerIndex)
{
	// Set forced colors
	if (m_rgbPlayerRemapColorsForced[playerIndex])
	{
		m_nTopColor = m_rgiPlayerRemapColors[playerIndex][0];
		m_nBottomColor = m_rgiPlayerRemapColors[playerIndex][1];
	}
	else
	{
		bool set = false;

		// Pickup replacement colors for all, but local player
		if (m_iLocalPlayerIndex - 1 != playerIndex)
		{
			// Ensure we have actual colors info
			ParseColors();

			// Return teammates override colors or actual colors for local player teammates if this is a teamplay
			// We will not check for gHUD.m_Teamplay because customized sever dll send us "teamplay 1" to enable teams in scoreboard
			if (AreTeammates(playerIndex + 1, m_iLocalPlayerIndex))
			{
				if (cl_forceteammatescolors.GetString()[0] != 0)
				{
					m_nTopColor = m_iTeammatesTopColor;
					m_nBottomColor = m_iTeammatesBottomColor;
					set = true;
				}
			}
			else // Enemies
			{
				if (cl_forceenemycolors.GetString()[0] != 0)
				{
					m_nTopColor = m_iEnemyTopColor;
					m_nBottomColor = m_iEnemyBottomColor;
					set = true;
				}
			}
		}

		// For local player or player with not set ovverrides we will set own colors
		if (!set)
		{
			if (!m_pPlayerInfo)
			{
				m_nTopColor = 0;
				m_nBottomColor = 0;
			}
			else
			{
				m_nTopColor = m_pPlayerInfo->topcolor;
				m_nBottomColor = m_pPlayerInfo->bottomcolor;
				m_nTopColor = clamp(m_nTopColor, 0, 254);
				m_nBottomColor = clamp(m_nBottomColor, 0, 254);
			}
		}
	}

	// Set remap colors
	IEngineStudio.StudioSetRemapColors(m_nTopColor, m_nBottomColor);
}

///
/// Sets player remapped model via command.
///
void CGameStudioModelRenderer::ForceModelCommand(void)
{
	int argc = gEngfuncs.Cmd_Argc();
	if (argc <= 1)
	{
		gEngfuncs.Con_Printf("usage:  forcemodel \"slot number or player name\" [\"model name\"]\n");
		return;
	}

	model_t *m_pModel = NULL;
	if (argc > 1)
	{
		char *modelName = gEngfuncs.Cmd_Argv(2);
		if (modelName && modelName[0])
		{
			char path[256];
			sprintf(path, "models/player/%s/%s.mdl", gEngfuncs.Cmd_Argv(2), gEngfuncs.Cmd_Argv(2));
			m_pModel = IEngineStudio.Mod_ForName(path, 0);
			if (m_pModel == NULL)
			{
				gEngfuncs.Con_Printf("Model \"%s\" not found\n", path);
				return;
			}
		}
	}

	int slot = atoi(gEngfuncs.Cmd_Argv(1));
	if (slot)
	{
		int playerIndex = slot - 1;
		if (playerIndex < 0 || playerIndex >= MAX_PLAYERS)
		{
			gEngfuncs.Con_Printf("Wrong slot number: %i. Slot number should be between 1 and %i\n", slot, MAX_PLAYERS);
			return;
		}
		m_szPlayerRemapModel[playerIndex][0] = 0;
		m_rgpPlayerRemapModel[playerIndex] = m_pModel;
		m_rgbPlayerRemapModelForced[playerIndex] = m_pModel != NULL;
	}
	else
	{
		char *name = gEngfuncs.Cmd_Argv(1);
		Q_strlower(name);
		if (!m_iLocalPlayerIndex)
			m_iLocalPlayerIndex = gEngfuncs.GetLocalPlayer()->index;
		// Find a players by a name
		char plrName[MAX_PLAYER_NAME];
		int maxClients = gEngfuncs.GetMaxClients();
		for (int i = 0; i < maxClients; i++)
		{
			if (i == m_iLocalPlayerIndex - 1)
				continue;

			CPlayerInfo *pi = GetPlayerInfo(i + 1)->Update();

			if (!pi->IsConnected() || !CHudSpectator::Get()->IsActivePlayer(gEngfuncs.GetEntityByIndex(i + 1)))
				continue;

			strncpy(plrName, pi->GetName(), MAX_PLAYER_NAME - 1);
			plrName[MAX_PLAYER_NAME - 1] = 0;
			Q_strlower(plrName);
			if (!strstr(plrName, name))
				continue;

			int playerIndex = i;
			m_szPlayerRemapModel[playerIndex][0] = 0;
			m_rgpPlayerRemapModel[playerIndex] = m_pModel;
			m_rgbPlayerRemapModelForced[playerIndex] = m_pModel != NULL;
		}
	}
}

///
/// Sets player remapped colors via command.
///
void CGameStudioModelRenderer::ForceColorsCommand(void)
{
	int argc = gEngfuncs.Cmd_Argc();
	if (argc <= 1)
	{
		gEngfuncs.Con_Printf("usage:  forcecolors \"slot number or player name\" [\"top and bottom colors\"]\n");
		return;
	}

	int topColor = -1, bottomColor = -1;
	if (argc > 1)
	{
		char *colors = gEngfuncs.Cmd_Argv(2);
		if (colors && colors[0])
		{
			while (*colors == ' ')
				colors++;
			topColor = clamp(atoi(colors), -1, 254);
			char *bottom = strchr(m_szEnemyColor, ' ');
			if ((bottom == NULL || bottom[0] == 0) && argc > 2)
			{
				bottom = gEngfuncs.Cmd_Argv(3);
			}
			bottomColor = bottom != NULL ? clamp(atoi(bottom), -1, 254) : 0;
		}
	}

	int slot = atoi(gEngfuncs.Cmd_Argv(1));
	if (slot)
	{
		int playerIndex = slot - 1;
		if (playerIndex < 0 || playerIndex >= MAX_PLAYERS)
		{
			gEngfuncs.Con_Printf("Wrong slot number: %i. Slot number should be between 1 and %i\n", slot, MAX_PLAYERS);
			return;
		}
		m_rgiPlayerRemapColors[playerIndex][0] = topColor;
		m_rgiPlayerRemapColors[playerIndex][1] = bottomColor;
		m_rgbPlayerRemapColorsForced[playerIndex] = topColor >= 0;
	}
	else
	{
		char *name = gEngfuncs.Cmd_Argv(1);
		Q_strlower(name);
		if (!m_iLocalPlayerIndex)
			m_iLocalPlayerIndex = gEngfuncs.GetLocalPlayer()->index;
		// Find a players by a name
		char plrName[MAX_PLAYER_NAME];
		int maxClients = gEngfuncs.GetMaxClients();
		for (int i = 0; i < maxClients; i++)
		{
			if (i == m_iLocalPlayerIndex - 1)
				continue;

			CPlayerInfo *pi = GetPlayerInfo(i + 1)->Update();

			if (!pi->IsConnected() || !CHudSpectator::Get()->IsActivePlayer(gEngfuncs.GetEntityByIndex(i + 1)))
				continue;

			strncpy(plrName, pi->GetName(), MAX_PLAYER_NAME - 1);
			plrName[MAX_PLAYER_NAME - 1] = 0;
			Q_strlower(plrName);
			if (!strstr(plrName, name))
				continue;

			int playerIndex = i;
			m_rgiPlayerRemapColors[playerIndex][0] = topColor;
			m_rgiPlayerRemapColors[playerIndex][1] = bottomColor;
			m_rgbPlayerRemapColorsForced[playerIndex] = topColor >= 0;
		}
	}
}

////////////////////////////////////
// Hooks to class implementation
////////////////////////////////////

/*
====================
R_StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer(int flags, entity_state_t *pplayer)
{
	return g_StudioRenderer.StudioDrawPlayer(flags, pplayer);
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel(int flags)
{
	return g_StudioRenderer.StudioDrawModel(flags);
}

/*
====================
R_StudioInit

====================
*/
void R_StudioInit(void)
{
	g_StudioRenderer.Init();
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio = {
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer,
};

/*
====================
HUD_GetStudioModelInterface

Export this function for the engine to use the studio renderer class to render objects.
====================
*/
extern "C" int CL_DLLEXPORT HUD_GetStudioModelInterface(int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio)
{
	if (version != STUDIO_INTERFACE_VERSION)
		return 0;

	// Point the engine to our callbacks
	*ppinterface = &studio;

	// Copy in engine helper functions
	memcpy(&IEngineStudio, pstudio, sizeof(IEngineStudio));

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}
