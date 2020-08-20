#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "player_info.h"
#include "com_model.h"
#include "engine_patches.h"

CPlayerInfo CPlayerInfo::m_sPlayerInfo[MAX_PLAYERS + 1];

/**
 * Parses string into a SteamID64. Returns 0 if failed.
 * Credits to voogru
 * https://forums.alliedmods.net/showthread.php?t=60899?t=60899
 */
static uint64 ParseSteamID(const char *pszAuthID)
{
	if (!pszAuthID)
		return 0;

	int iServer = 0;
	int iAuthID = 0;

	char szAuthID[64];
	strncpy(szAuthID, pszAuthID, sizeof(szAuthID) - 1);
	szAuthID[sizeof(szAuthID) - 1] = '\0';

	char *szTmp = strtok(szAuthID, ":");
	while (szTmp = strtok(NULL, ":"))
	{
		char *szTmp2 = strtok(NULL, ":");
		if (szTmp2)
		{
			iServer = atoi(szTmp);
			iAuthID = atoi(szTmp2);
		}
	}

	if (iAuthID == 0)
		return 0;

	uint64 i64friendID = (long long)iAuthID * 2;

	//Friend ID's with even numbers are the 0 auth server.
	//Friend ID's with odd numbers are the 1 auth server.
	i64friendID += 76561197960265728 + iServer;

	return i64friendID;
}

bool CPlayerInfo::IsConnected()
{
	return m_bIsConnected;
}

const char *CPlayerInfo::GetName()
{
	Assert(m_bIsConnected);
	return m_EngineInfo.name;
}

int CPlayerInfo::GetPing()
{
	Assert(m_bIsConnected);
	return m_EngineInfo.ping;
}

int CPlayerInfo::GetPacketLoss()
{
	Assert(m_bIsConnected);
	return m_EngineInfo.packetloss;
}

bool CPlayerInfo::IsThisPlayer()
{
	Assert(m_bIsConnected);
	return m_EngineInfo.thisplayer;
}

const char *CPlayerInfo::GetModel()
{
	Assert(m_bIsConnected);
	return m_EngineInfo.model;
}

int CPlayerInfo::GetTopColor()
{
	Assert(m_bIsConnected);
	return m_EngineInfo.topcolor;
}

int CPlayerInfo::GetBottomColor()
{
	Assert(m_bIsConnected);
	return m_EngineInfo.bottomcolor;
}

uint64 CPlayerInfo::GetSteamID64()
{
	Assert(m_bIsConnected);

	if (CEnginePatches::Get().IsSDLEngine())
	{
		player_info_t *info = GetEnginePlayerInfo();

		// Check whether first digit is 7
		if (info->m_nSteamID / 10000000000000000LL == 7)
			return info->m_nSteamID;
	}

	return ParseSteamID(m_szSteamID);
}

int CPlayerInfo::GetFrags()
{
	Assert(m_bIsConnected);
	return m_ExtraInfo.frags;
}

int CPlayerInfo::GetDeaths()
{
	Assert(m_bIsConnected);
	return m_ExtraInfo.deaths;
}

int CPlayerInfo::GetPlayerClass()
{
	Assert(m_bIsConnected);
	return m_ExtraInfo.playerclass;
}

int CPlayerInfo::GetTeamNumber()
{
	Assert(m_bIsConnected);
	return m_ExtraInfo.teamnumber;
}

const char *CPlayerInfo::GetTeamName()
{
	Assert(m_bIsConnected);
	return m_ExtraInfo.teamname;
}

bool CPlayerInfo::IsSpectator()
{
	return m_bIsSpectator || m_EngineInfo.spectator;
}

const char *CPlayerInfo::GetDisplayName(bool bNoColorCodes)
{
	// This method is a great place to add AG realnames.
	if (bNoColorCodes && gHUD.GetColorCodeAction() != ColorCodeAction::Ignore)
	{
		// Select a buffer
		static char buffers[8][MAX_PLAYER_NAME];
		static unsigned bufferIndex = 0;
		char *buf = buffers[bufferIndex & 0b111];
		bufferIndex++;

		// Strip color codes
		RemoveColorCodes(GetName(), buf, MAX_PLAYER_NAME);

		return buf;
	}
	else
	{
		// Return name with color codes
		return GetName();
	}
}

const char *CPlayerInfo::GetSteamID()
{
	return m_szSteamID;
}

CPlayerInfo *CPlayerInfo::Update()
{
	gEngfuncs.pfnGetPlayerInfo(m_iIndex, &m_EngineInfo);
	bool bIsConnected = m_EngineInfo.name != nullptr;

	if (bIsConnected && !m_bIsConnected)
	{
		// Player connected, update SteamID
		m_szSteamID[0] = '\0';
		CSvcMessages::Get().SendStatusRequest();
	}
	else if (!bIsConnected && m_bIsConnected)
	{
		// Player disconnected, erase SteamID.
		m_szSteamID[0] = '\0';
	}

	if (bIsConnected)
	{
		if (!m_szSteamID[0])
		{
			// Player has no SteamID, update it
			CSvcMessages::Get().SendStatusRequest();
		}
	}

	m_bIsConnected = bIsConnected;

	return this;
}

player_info_t *CPlayerInfo::GetEnginePlayerInfo()
{
	Assert(m_bIsConnected);
	if (!m_EngineInfo.name)
		return nullptr;
	player_info_t *ptr = reinterpret_cast<player_info_t *>(m_EngineInfo.name - offsetof(player_info_t, name));
	return ptr;
}

//-----------------------------------------------------

CTeamInfo CTeamInfo::m_sTeamInfo[MAX_TEAMS + 1];

int CTeamInfo::GetNumber()
{
	return m_iNumber;
}

const char *CTeamInfo::GetName()
{
	return m_Name;
}

const char *CTeamInfo::GetDisplayName()
{
	return m_DisplayName[0] != '\0' ? m_DisplayName : m_Name;
}

bool CTeamInfo::IsScoreOverriden()
{
	return m_bScoreOverriden;
}

int CTeamInfo::GetFrags()
{
	Assert(m_bScoreOverriden);
	return m_iFrags;
}

int CTeamInfo::GetDeaths()
{
	Assert(m_bScoreOverriden);
	return m_iDeaths;
}

void CTeamInfo::Reset(int number)
{
	m_iNumber = number;
	Q_strncpy(m_Name, "< undefined >", sizeof(m_Name));
	Q_strncpy(m_DisplayName, "", sizeof(m_DisplayName));
	m_bScoreOverriden = false;
	m_iFrags = 0;
	m_iDeaths = 0;
}

void CTeamInfo::UpdateAllTeams()
{
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i);

		if (!pi->IsConnected())
			continue;

		if (pi->GetTeamNumber() < 0 || pi->GetTeamNumber() > MAX_TEAMS)
			continue;

		CTeamInfo *ti = GetTeamInfo(pi->GetTeamNumber());
		Q_strncpy(ti->m_Name, pi->GetTeamName(), sizeof(ti->m_Name));
	}
}
