/***
*
*	Copyright (c) 2003', Valve LLC. All rights reserved.
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
#ifndef PLAYER_INFO_H
#define PLAYER_INFO_H
#include <tier0/dbg.h>

typedef struct player_info_s player_info_t;

struct extra_player_info_t
{
	short frags;
	short deaths;
	short playerclass;
	short health; // UNUSED currently, spectator UI would like this
	bool dead; // UNUSED currently, spectator UI would like this
	short teamnumber;
	char teamname[MAX_TEAM_NAME];
};

struct team_info_t
{
	char name[MAX_TEAM_NAME];
	short frags;
	short deaths;
	short ping;
	short packetloss;
	short ownteam;
	short players;
	int already_drawn;
	int scores_overriden;
	int teamnumber;
};

extern team_info_t g_TeamInfo[MAX_TEAMS + 1];

class CPlayerInfo;
inline CPlayerInfo *GetPlayerInfo(int idx);

class CPlayerInfo
{
public:
	bool IsConnected();

	// Engine info
	const char *GetName();
	int GetPing();
	int GetPacketLoss();
	bool IsThisPlayer();
	const char *GetModel();
	int GetTopColor();
	int GetBottomColor();
	uint64 GetSteamID64();

	// Extra info (from HUD messages)
	int GetFrags();
	int GetDeaths();
	int GetPlayerClass();
	int GetTeamNumber();
	const char *GetTeamName();
	bool IsSpectator();

	// Should be called before reading engine info.
	// Returns this
	CPlayerInfo *Update();

private:
	int m_iIndex = -1;
	hud_player_info_t m_EngineInfo;
	extra_player_info_t m_ExtraInfo;
	bool m_bIsConnected;
	bool m_bIsSpectator;

	player_info_t *GetEnginePlayerInfo();

	static CPlayerInfo m_sPlayerInfo[MAX_PLAYERS + 1];
	friend CPlayerInfo *GetPlayerInfo(int idx);
	friend class CHud;
	friend class CClientViewport;
};

inline CPlayerInfo *GetPlayerInfo(int idx)
{
	Assert(idx >= 1 && idx <= MAX_PLAYERS);
	return &CPlayerInfo::m_sPlayerInfo[idx];
}

#endif
