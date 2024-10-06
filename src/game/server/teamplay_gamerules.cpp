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
// teamplay_gamerules.cpp
//
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "game.h"

static char team_names[MAX_TEAMS][MAX_TEAM_NAME];
static int team_scores[MAX_TEAMS];
static int num_teams = 0;

extern DLL_GLOBAL BOOL g_fGameOver;

CHalfLifeTeamplay ::CHalfLifeTeamplay()
{
	m_DisableDeathMessages = FALSE;
	m_DisableDeathPenalty = FALSE;

	memset(team_names, 0, sizeof(team_names));
	memset(team_scores, 0, sizeof(team_scores));
	num_teams = 0;

	// Cache this because the team code doesn't want to deal with changing this in the middle of a game
	strncpy(m_szTeamList, teamlist.string, TEAMPLAY_TEAMLISTLENGTH);
	m_szTeamList[TEAMPLAY_TEAMLISTLENGTH - 1] = 0;

	edict_t *pWorld = INDEXENT(0);
	if (pWorld && pWorld->v.team && teamoverride.value)
	{
		const char *pTeamList = STRING(pWorld->v.team);
		if (pTeamList && !pTeamList[0])
		{
			strncpy(m_szTeamList, pTeamList, TEAMPLAY_TEAMLISTLENGTH);
			m_szTeamList[TEAMPLAY_TEAMLISTLENGTH - 1] = 0;
		}
	}

	char *pName;
	char temp[TEAMPLAY_TEAMLISTLENGTH];

	// Copy all of the teams from the teamlist
	// make a copy because strtok is destructive
	UTIL_strcpy(temp, m_szTeamList);
	// loop through all teams
	num_teams = 0;
	pName = strtok(temp, ";");
	while (pName != NULL && *pName && num_teams < MAX_TEAMS)
	{
		if (GetTeamIndex(pName) < 0)
		{
			strncpy(team_names[num_teams], pName, MAX_TEAM_NAME);
			team_names[num_teams][MAX_TEAM_NAME - 1];
			num_teams++;
		}
		pName = strtok(NULL, ";");
	}

	if (num_teams < 2)
	{
		num_teams = 0;
		m_teamLimit = FALSE;
	}
	else
		m_teamLimit = TRUE;

	RecountTeams();
}

extern cvar_t timeleft, fragsleft;

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

void CHalfLifeTeamplay ::Think(void)
{
	CGameRules::Think();

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	if (g_fGameOver) // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	float flTimeLimit = CVAR_GET_FLOAT("mp_timelimit") * 60;

	time_remaining = (int)(flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);

	if (flTimeLimit != 0 && gpGlobals->time >= flTimeLimit)
	{
		GoToIntermission();
		return;
	}

	float flFragLimit = fraglimit.value;
	if (flFragLimit)
	{
		int bestfrags = 9999;
		int remain;

		// check if any team is over the frag limit
		for (int i = 0; i < num_teams; i++)
		{
			if (team_scores[i] >= flFragLimit)
			{
				GoToIntermission();
				return;
			}

			remain = flFragLimit - team_scores[i];
			if (remain < bestfrags)
			{
				bestfrags = remain;
			}
		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if (frags_remaining != last_frags)
	{
		g_engfuncs.pfnCvar_DirectSet(&fragsleft, UTIL_VarArgs("%i", frags_remaining));
	}

	// Updates once per second
	if (timeleft.value != last_time)
	{
		g_engfuncs.pfnCvar_DirectSet(&timeleft, UTIL_VarArgs("%i", time_remaining));
	}

	last_frags = frags_remaining;
	last_time = time_remaining;
}

extern int gmsgGameMode;
extern int gmsgSayText;
extern int gmsgTeamInfo;
extern int gmsgTeamNames;
extern int gmsgScoreInfo;

void CHalfLifeTeamplay ::UpdateGameMode(CBasePlayer *pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgGameMode, NULL, pPlayer->edict());
	WRITE_BYTE(1); // game mode teamplay
	MESSAGE_END();
}

const char *CHalfLifeTeamplay::SetDefaultPlayerTeam(CBasePlayer *pPlayer)
{
	// copy out the team name from the model
	char *mdls = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model");
	strncpy(pPlayer->m_szTeamName, mdls, MAX_TEAM_NAME);
	pPlayer->m_szTeamName[MAX_TEAM_NAME - 1] = 0;

	// update the current player of the team he is joining
	if (defaultteam.value || pPlayer->m_szTeamName[0] == '\0' || !IsValidTeam(pPlayer->m_szTeamName))
	{
		const char *pTeamName;
		if (defaultteam.value)
		{
			pTeamName = team_names[0];
		}
		else
		{
			pTeamName = TeamWithFewestPlayers();
		}
		strncpy(pPlayer->m_szTeamName, pTeamName, MAX_TEAM_NAME);
	}

	return pPlayer->m_szTeamName;
}

//=========================================================
// InitHUD
//=========================================================
void CHalfLifeTeamplay::InitHUD(CBasePlayer *pPlayer)
{
	CHalfLifeMultiplay::InitHUD(pPlayer);

	// update the current player of the team he is joining
	char text[256];
	char *mdls = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model");
	if (!strcmp(mdls, pPlayer->m_szTeamName))
	{
		sprintf(text, "* you are on team \'%s\'\n", pPlayer->m_szTeamName);
	}
	else
	{
		sprintf(text, "* assigned to team %s\n", pPlayer->m_szTeamName);
	}
	UTIL_SayText(text, pPlayer);

	// Send down the team names
	MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
	WRITE_BYTE(num_teams);
	for (int i = 0; i < num_teams; i++)
	{
		WRITE_STRING(team_names[i]);
	}
	MESSAGE_END();

	ChangePlayerTeam(pPlayer, pPlayer->m_szTeamName, FALSE, FALSE);

	// loop through all active players and send their team info to the new client
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity *plr = UTIL_PlayerByIndex(i);
		if (plr && IsValidTeam(plr->TeamID()))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict());
			WRITE_BYTE(plr->entindex());
			WRITE_STRING(plr->pev->iuser1 ? "" : plr->TeamID());
			MESSAGE_END();
		}
	}
}

void CHalfLifeTeamplay::ChangePlayerTeam(CBasePlayer *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib)
{
	int clientIndex = pPlayer->entindex();

	if (bKill)
	{
		// kill the player,  remove a death,  and let them start on the new team
		m_DisableDeathMessages = TRUE;
		m_DisableDeathPenalty = TRUE;

		int damageFlags = DMG_GENERIC | (bGib ? DMG_ALWAYSGIB : DMG_NEVERGIB);
		entvars_t *pevWorld = VARS(INDEXENT(0));
		pPlayer->TakeDamage(pevWorld, pevWorld, 10000, damageFlags);

		m_DisableDeathMessages = FALSE;
		m_DisableDeathPenalty = FALSE;
	}

	// Set team to player
	strncpy(pPlayer->m_szTeamName, pTeamName, MAX_TEAM_NAME);
	g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);

	RecountTeams();

	// notify everyone's HUD of the team change
	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
	WRITE_BYTE(clientIndex);
	WRITE_STRING(pPlayer->pev->iuser1 ? "" : pPlayer->TeamID());
	MESSAGE_END();

	pPlayer->SendScoreInfo();
}

//=========================================================
// ClientUserInfoChanged
//=========================================================
void CHalfLifeTeamplay::ClientUserInfoChanged(CBasePlayer *pPlayer, char *infobuffer)
{
	char text[1024];

	// prevent skin/color/model changes
	char *mdls = g_engfuncs.pfnInfoKeyValue(infobuffer, "model");

	if (!_stricmp(mdls, pPlayer->m_szTeamName))
		return;

	if (defaultteam.value)
	{
		int clientIndex = pPlayer->entindex();
		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
		sprintf(text, "* Not allowed to change teams in this game!\n");
		UTIL_SayText(text, pPlayer);
		return;
	}

	if (!IsValidTeam(mdls))
	{
		int clientIndex = pPlayer->entindex();
		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
		snprintf(text, sizeof(text), "* Can't change team to \'%s\'\n", mdls);
		UTIL_SayText(text, pPlayer);
		snprintf(text, sizeof(text), "* Server limits teams to \'%s\'\n", m_szTeamList);
		UTIL_SayText(text, pPlayer);
		return;
	}

	// notify everyone of the team change
	sprintf(text, "* %s has changed to team \'%s\'\n", STRING(pPlayer->pev->netname), mdls);
	UTIL_SayTextAll(text, pPlayer);

	UTIL_LogPrintf("\"%s<%i><%s><%s>\" joined team \"%s\"\n",
	    STRING(pPlayer->pev->netname),
	    GETPLAYERUSERID(pPlayer->edict()),
	    GETPLAYERAUTHID(pPlayer->edict()),
	    pPlayer->m_szTeamName,
	    mdls);

	ChangePlayerTeam(pPlayer, mdls, TRUE, TRUE);
}

extern int gmsgDeathMsg;

//=========================================================
// Deathnotice.
//=========================================================
void CHalfLifeTeamplay::DeathNotice(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor)
{
	if (m_DisableDeathMessages)
		return;

	if (pVictim && pKiller && pKiller->flags & FL_CLIENT)
	{
		CBasePlayer *pk = (CBasePlayer *)CBaseEntity::Instance(pKiller);

		if (pk)
		{
			if ((pk != pVictim) && (PlayerRelationship(pVictim, pk) == GR_TEAMMATE))
			{
				MESSAGE_BEGIN(MSG_ALL, gmsgDeathMsg);
				WRITE_BYTE(ENTINDEX(ENT(pKiller))); // the killer
				WRITE_BYTE(ENTINDEX(pVictim->edict())); // the victim
				WRITE_STRING("teammate"); // flag this as a teammate kill
				MESSAGE_END();
				return;
			}
		}
	}

	CHalfLifeMultiplay::DeathNotice(pVictim, pKiller, pevInflictor);
}

//=========================================================
//=========================================================
void CHalfLifeTeamplay ::PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor)
{
	if (!m_DisableDeathPenalty)
	{
		CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
		RecountTeams();
	}
}

//=========================================================
// IsTeamplay
//=========================================================
BOOL CHalfLifeTeamplay::IsTeamplay(void)
{
	return TRUE;
}

BOOL CHalfLifeTeamplay::FPlayerCanTakeDamage(CBasePlayer *pPlayer, CBaseEntity *pAttacker)
{
	if (pAttacker && PlayerRelationship(pPlayer, pAttacker) == GR_TEAMMATE)
	{
		// my teammate hit me.
		if ((friendlyfire.value == 0) && (pAttacker != pPlayer))
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return FALSE;
		}
	}

	return CHalfLifeMultiplay::FPlayerCanTakeDamage(pPlayer, pAttacker);
}

//=========================================================
//=========================================================
int CHalfLifeTeamplay::PlayerRelationship(CBaseEntity *pPlayer, CBaseEntity *pTarget)
{
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if (!pPlayer || !pTarget || !pPlayer->IsPlayer() || !pTarget->IsPlayer())
		return GR_NOTTEAMMATE;
	// Spectators are teammates, but not players in welcomecam mode
	if (((CBasePlayer *)pPlayer)->IsObserver() && !((CBasePlayer *)pPlayer)->m_bInWelcomeCam && ((CBasePlayer *)pTarget)->IsObserver() && !((CBasePlayer *)pTarget)->m_bInWelcomeCam)
		return GR_TEAMMATE;

	if ((*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !_stricmp(GetTeamID(pPlayer), GetTeamID(pTarget)))
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeTeamplay::ShouldAutoAim(CBasePlayer *pPlayer, edict_t *target)
{
	// always autoaim, unless target is a teammate
	CBaseEntity *pTgt = CBaseEntity::Instance(target);
	if (pTgt && pTgt->IsPlayer())
	{
		if (PlayerRelationship(pPlayer, pTgt) == GR_TEAMMATE)
			return FALSE; // don't autoaim at teammates
	}

	return CHalfLifeMultiplay::ShouldAutoAim(pPlayer, target);
}

//=========================================================
//=========================================================
int CHalfLifeTeamplay::IPointsForKill(CBasePlayer *pAttacker, CBasePlayer *pKilled)
{
	if (!pKilled)
		return 0;

	if (!pAttacker)
		return 1;

	if (pAttacker != pKilled && PlayerRelationship(pAttacker, pKilled) == GR_TEAMMATE)
		return -1;

	return 1;
}

//=========================================================
//=========================================================
const char *CHalfLifeTeamplay::GetTeamID(CBaseEntity *pEntity)
{
	if (pEntity == NULL || pEntity->pev == NULL)
		return "";

	// return their team name
	return pEntity->TeamID();
}

int CHalfLifeTeamplay::GetTeamIndex(const char *pTeamName)
{
	if (pTeamName && *pTeamName != 0)
	{
		// try to find existing team
		for (int tm = 0; tm < num_teams; tm++)
		{
			if (!_stricmp(team_names[tm], pTeamName))
				return tm;
		}
	}

	return -1; // No match
}

const char *CHalfLifeTeamplay::GetIndexedTeamName(int teamIndex)
{
	if (teamIndex < 0 || teamIndex >= num_teams)
		return "";

	return team_names[teamIndex];
}

BOOL CHalfLifeTeamplay::IsValidTeam(const char *pTeamName)
{
	if (!m_teamLimit) // Any team is valid if the teamlist isn't set
		return TRUE;

	return (GetTeamIndex(pTeamName) != -1) ? TRUE : FALSE;
}

const char *CHalfLifeTeamplay::TeamWithFewestPlayers(void)
{
	int i;
	int minPlayers = gpGlobals->maxClients;
	int teamCount[MAX_TEAMS];
	char *pTeamName = NULL;

	memset(teamCount, 0, sizeof(teamCount));

	// loop through all clients, count number of players on each team
	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity *plr = UTIL_PlayerByIndex(i);
		if (!plr)
			continue;
		int team = GetTeamIndex(plr->TeamID());
		if (team >= 0)
			teamCount[team]++;
	}

	// Find team with least players
	for (i = 0; i < num_teams; i++)
	{
		if (teamCount[i] < minPlayers)
		{
			minPlayers = teamCount[i];
			pTeamName = team_names[i];
		}
	}

	return pTeamName;
}

//=========================================================
//=========================================================
void CHalfLifeTeamplay::RecountTeams(void)
{
	// We will use this to resend team names to clients
	int num_teams_old = num_teams;
	bool teamListChanged = false;

	// Reset scores
	memset(team_scores, 0, sizeof(team_scores));
	// Reset teams if server doesn't limit them so we can fill the list with current player teams
	if (!m_teamLimit)
		num_teams = 0;

	// loop through all clients
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (!plr)
			continue;

		const char *pTeamName = plr->TeamID();
		// Search in existing teams
		int tm = GetTeamIndex(pTeamName);
		if (tm < 0) // No team match found
		{
			if (!m_teamLimit) // Server doesn't limit teams
			{
				// Add new team
				tm = num_teams;
				num_teams++;
				team_scores[tm] = 0;
				if (_stricmp(team_names[tm], pTeamName))
					teamListChanged = true;
				strncpy(team_names[tm], pTeamName, MAX_TEAM_NAME);
				team_names[tm][MAX_TEAM_NAME - 1] = 0;
			}
		}
		if (tm >= 0)
		{
			team_scores[tm] += plr->pev->frags;
		}
	}

	if (!m_teamLimit)
	{
		if (teamListChanged || num_teams_old != num_teams)
		{
			// Send down the team names
			MESSAGE_BEGIN(MSG_ALL, gmsgTeamNames);
			WRITE_BYTE(num_teams);
			for (int i = 0; i < num_teams; i++)
			{
				WRITE_STRING(team_names[i]);
			}
			MESSAGE_END();

			// loop through all clients and resend team index info
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (!pPlayer)
					continue;

				pPlayer->SendScoreInfo();
			}
		}
	}
}
