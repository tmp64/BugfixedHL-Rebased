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

//-----------------------------------------------------

class CPlayerInfo;
CPlayerInfo *GetPlayerInfo(int idx);
CPlayerInfo *GetThisPlayerInfo();

class CPlayerInfo
{
public:
	int GetIndex();
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

	/**
	 * Returns display name. It should be used in text displayed on HUD.
	 * @param	bNoColorCodes	If true and ColorCodeAction != Ignore, color codes will be removed.
	 * @return	Display name stored in internal buffer. It can handle up to 8 calls before overwriting.
	 */
	const char *GetDisplayName(bool bNoColorCodes = false);

	/**
	 * Returns SteamID string. Requires SVC hook.
	 */
	const char *GetSteamID();

	// Should be called before reading engine info.
	// Returns this
	CPlayerInfo *Update();

private:
	int m_iIndex = -1;
	hud_player_info_t m_EngineInfo;
	extra_player_info_t m_ExtraInfo;
	bool m_bIsConnected;
	bool m_bIsSpectator;
	char m_szSteamID[MAX_STEAMID + 1];

	player_info_t *GetEnginePlayerInfo();
	void Reset();

	static CPlayerInfo m_sPlayerInfo[MAX_PLAYERS + 1];
	friend CPlayerInfo *GetPlayerInfo(int idx);
	friend class CHud;
	friend class CClientViewport;
	friend class CSvcMessages;
};

inline CPlayerInfo *GetPlayerInfo(int idx)
{
	Assert(idx >= 1 && idx <= MAX_PLAYERS);
	return &CPlayerInfo::m_sPlayerInfo[idx];
}

//-----------------------------------------------------

class CTeamInfo;
CTeamInfo *GetTeamInfo(int number);

class CTeamInfo
{
public:
	/**
	 * Returns team number.
	 */
	int GetNumber();

	/**
	 * Returns name of the team. This is the one returned by CPlayerInfo::GetTeamName().
	 */
	const char *GetName();

	/**
	 * Returns display name. It should be used in text displayed to the player.
	 * It can be overriden by TeamNames message.
	 */
	const char *GetDisplayName();

	/**
	 * Returns whether TeamScore message was used to override the scores.
	 * If true, use GetFrags and GetDeaths instead of calculated values.
	 */
	bool IsScoreOverriden();

	/**
	 * Returns frag count.
	 * Only valid if IsScoreOverriden() == true.
	 */
	int GetFrags();

	/**
	 * Returns death count.
	 * Only valid if IsScoreOverriden() == true.
	 */
	int GetDeaths();

private:
	int m_iNumber = -1;
	char m_Name[MAX_TEAM_NAME] = "< undefined >";
	char m_DisplayName[MAX_TEAM_DISPLAY_NAME] = "< undefined >";
	bool m_bScoreOverriden = false;
	int m_iFrags = 0;
	int m_iDeaths = 0;

	/**
	 * Called during VidInit to reset internal data.
	 * @param	number	Number of the team (used to set m_iNumber).
	 */
	void Reset(int number);

	/**
	 * Updates state of all teams.
	 */
	static void UpdateAllTeams();

	static CTeamInfo m_sTeamInfo[MAX_TEAMS + 1];
	friend CTeamInfo *GetTeamInfo(int number);
	friend class CHud;
	friend class CClientViewport;
};

inline CTeamInfo *GetTeamInfo(int number)
{
	Assert(number >= 0 && number <= MAX_TEAMS);
	return &CTeamInfo::m_sTeamInfo[number];
}

#endif
