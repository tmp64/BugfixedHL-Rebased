//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GAMESTUDIOMODELRENDERER_H
#define GAMESTUDIOMODELRENDERER_H
#include "StudioModelRenderer.h"

/*
====================
CGameStudioModelRenderer

====================
*/
class CGameStudioModelRenderer : public CStudioModelRenderer
{
public:
	CGameStudioModelRenderer(void);

	// Initialization
	virtual void Init(void);
	void InitOnConnect(void);

	// Prints debug info to the console
	void PrintDebugInfo();

	// Returns player model for rendering
	virtual model_t *GetPlayerModel(int playerIndex);

	// Sets remap colors for current player
	virtual void SetPlayerRemapColors(int playerIndex);

	// Forces remap model for given player/slot
	virtual void ForceModelCommand(void);

	// Forces remap colors for given player/slot
	virtual void ForceColorsCommand(void);

private:
	// local player index 1-based
	int m_iLocalPlayerIndex;
	char m_szTeammatesModel[MAX_TEAM_NAME];
	model_t *m_pTeammatesModel;
	int m_iEnemyModelsCount;
	int m_iLastUsedEnemyModel;
	char m_szEnemyModelsList[MAX_TEAMS * MAX_TEAM_NAME];
	char m_szEnemyModels[MAX_TEAMS][MAX_TEAM_NAME];
	char m_szPlayerActualModel[MAX_PLAYERS][MAX_TEAM_NAME];
	char m_szPlayerRemapModel[MAX_PLAYERS][MAX_TEAM_NAME];
	model_t *m_rgpPlayerRemapModel[MAX_PLAYERS];
	bool m_rgbPlayerRemapModelForced[MAX_PLAYERS];

	int m_iEnemyTopColor;
	int m_iEnemyBottomColor;
	int m_iTeammatesTopColor;
	int m_iTeammatesBottomColor;
	char m_szEnemyColor[12];
	char m_szTeammatesColor[12];
	int m_rgiPlayerRemapColors[MAX_PLAYERS][2];
	bool m_rgbPlayerRemapColorsForced[MAX_PLAYERS];

	// Parses enemy models list cvar if it changed.
	int ParseModels(void);

	// Parses enemy and teammates color cvars if they are changed.
	void ParseColors(void);

	// Return true if players are teammates
	bool AreTeammates(int playerIndex1, int playerIndex2);

	// Returns next enemy model from the list
	char *GetNextEnemyModel(void);
};

extern CGameStudioModelRenderer g_StudioRenderer;

#endif // GAMESTUDIOMODELRENDERER_H
