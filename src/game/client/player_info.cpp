#include <map>
#include <string>
#include <FileSystem.h>
#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "player_info.h"
#include "com_model.h"
#include "engine_patches.h"
#include "vgui/client_viewport.h"
#include "vgui/score_panel.h"

CPlayerInfo CPlayerInfo::m_sPlayerInfo[MAX_PLAYERS + 1];
static CPlayerInfo *s_ThisPlayerInfo = nullptr;

namespace
{

std::map<uint64_t, std::string> s_RealNames;

// UTF-friendly version instead of platform-specific ones
bool IsSpace(char c)
{
	// Everything before space is either whitespace or invalid.
	// UTF-8 chars all have bit 7 set to high
	return static_cast<unsigned char>(c) <= ' ';
}

/**
 * Parses string into a SteamID64. Returns 0 if failed.
 * Credits to voogru
 * https://forums.alliedmods.net/showthread.php?t=60899?t=60899
 */
uint64 ParseSteamID(const char *pszAuthID)
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

void UnloadAuthID()
{
	s_RealNames.clear();

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		GetPlayerInfo(i)->ClearRealName();
	}
}

void LoadAuthID()
{
	UnloadAuthID();
	FileHandle_t hFile = g_pFullFileSystem->Open("realnames.txt", "r");

	if (hFile == FILESYSTEM_INVALID_HANDLE)
	{
		ConPrintf(ConColor::Red, "Failed to open realnames.txt\n");
		return;
	}

	char linebuf[512];

	for (int linenum = 1; g_pFullFileSystem->ReadLine(linebuf, sizeof(linebuf), hFile); linenum++)
	{
		char steamid[64];
		char name[64];
		int idx = 0;

		if (!linebuf[idx])
			continue; // Empty line

		// Skip spaces
		while (linebuf[idx] && IsSpace(linebuf[idx]))
			idx++;

		if (!linebuf[idx])
			continue; // Empty line

		// Find next space
		int steamidbegin = idx;
		while (linebuf[idx] && !IsSpace(linebuf[idx]))
			idx++;

		if (!linebuf[idx])
		{
			ConPrintf("Line %d: unexpected end of line [1]\n", linenum);
			continue;
		}

		// Copy SteamID
		int steamidend = idx;
		int steamidlen = steamidend - steamidbegin;

		if (steamidlen >= 2)
		{
			if (linebuf[steamidbegin + 0] == '/' && linebuf[steamidbegin + 1] == '/')
			{
				// Skip comment
				continue;
			}
		}

		if (steamidlen >= sizeof(steamid))
		{
			ConPrintf("Line %d: SteamID too long (%d > %d)\n", linenum, steamidlen, sizeof(steamid) - 1);
			continue;
		}

		memcpy(steamid, linebuf + steamidbegin, steamidlen);
		steamid[steamidlen] = '\0';

		// Skip spaces
		while (linebuf[idx] && IsSpace(linebuf[idx]))
			idx++;

		if (!linebuf[idx])
		{
			ConPrintf("Line %d: unexpected end of line [2]\n", linenum);
			continue;
		}

		int namebegin = idx;

		// Find end of the string
		while (linebuf[idx])
			idx++;
		int nameend = idx;
		int namelen = nameend - namebegin;

		if (namelen >= sizeof(name))
		{
			ConPrintf("Line %d: name too long (%d > %d)\n", linenum, namelen, sizeof(name) - 1);
			continue;
		}

		memcpy(name, linebuf + namebegin, namelen);
		name[namelen] = '\0';

		if (name[namelen - 1] == '\n')
			name[namelen - 1] = '\0';

		// Parse SteamID
		uint64_t steamid64 = ParseSteamID(steamid);

		if (steamid64 == 0)
		{
			ConPrintf("Line %d: failed to parse SteamID\n", linenum);
			continue;
		}

		s_RealNames.insert({ steamid64, name });
	}

	ConPrintf("Loaded %d realnames\n", s_RealNames.size());
}

}

CON_COMMAND(loadauthid, "Loads real names from realnames.txt")
{
	LoadAuthID();
}

CON_COMMAND(unloadauthid, "Clears loaded realnames")
{
	UnloadAuthID();
}

CPlayerInfo *GetThisPlayerInfo()
{
	return s_ThisPlayerInfo;
}

CPlayerInfo *GetPlayerInfoSafe(int idx)
{
	if (!(idx >= 1 && idx <= MAX_PLAYERS))
		return nullptr;

	CPlayerInfo *pInfo = GetPlayerInfo(idx);
	if (!pInfo->IsConnected())
		return nullptr;

	return pInfo;
}

int CPlayerInfo::GetIndex()
{
	return m_iIndex;
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

uint64 CPlayerInfo::GetValidSteamID64()
{
	Assert(m_bIsConnected);

	if (CEnginePatches::Get().IsSDLEngine())
	{
		uint64 sid = GetEnginePlayerInfo()->m_nSteamID;

		// Valid SteamID must have type of account = 1 (Individual)
		// See below for reference
		if (((sid & 0x00F0000000000000ull) >> 52) != 1)
			return 0;

		return sid;
	}
	else
	{
		// Valid SteamID must begin with 0:Y:ZZZZZZZ
		// "The value of X (Universe) is 0 in VALVe's GoldSrc and Source Orange Box Engine games"
		// https://developer.valvesoftware.com/wiki/SteamID
		if (strncmp(m_szSteamID, "0:", 2))
			return 0;

		return ParseSteamID(m_szSteamID);
	}
}

uint64 CPlayerInfo::GetStatusSteamID64()
{
	Assert(m_bIsConnected);
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
	Assert(m_bIsConnected);
	const char *name = nullptr;

	if (m_szRealName[0])
		name = m_szRealName;
	else
		name = GetName();

	if (bNoColorCodes && gHUD.GetColorCodeAction() != ColorCodeAction::Ignore)
	{
		// Select a buffer
		static char buffers[8][MAX_PLAYER_NAME];
		static unsigned bufferIndex = 0;
		char *buf = buffers[bufferIndex & 0b111];
		bufferIndex++;

		// Strip color codes
		RemoveColorCodes(name, buf, MAX_PLAYER_NAME);

		return buf;
	}
	else
	{
		// Return name with color codes
		return name;
	}
}

const char *CPlayerInfo::GetSteamID()
{
	return m_szSteamID;
}

CPlayerInfo *CPlayerInfo::Update()
{
	gEngfuncs.pfnGetPlayerInfo(m_iIndex, &m_EngineInfo);
	bool bWasConnected = m_bIsConnected;
	bool bIsConnected = m_EngineInfo.name != nullptr;
	m_bIsConnected = bIsConnected;

	if (bIsConnected != bWasConnected)
	{
		// Player connected or disconnected
		m_szSteamID[0] = '\0';
		m_szRealName[0] = '\0';
		m_bRealNameChecked = false;
		m_iStatusPenalty = 0;
		m_flLastStatusRequest = 0;
		g_pViewport->GetScoreBoard()->UpdateOnPlayerInfo(GetIndex());
	}

	if (bIsConnected)
	{
		if (!m_szSteamID[0])
		{
			// Player has no SteamID, update it
			float period = (m_iStatusPenalty < STATUS_PENALTY_THRESHOLD) ? STATUS_PERIOD : STATUS_BUGGED_PERIOD;
			if (m_flLastStatusRequest + period < gEngfuncs.GetAbsoluteTime())
			{
				CSvcMessages::Get().SendStatusRequest();
				m_flLastStatusRequest = gEngfuncs.GetAbsoluteTime();
			}
		}

		if (!m_bRealNameChecked && m_szSteamID[0])
		{
			m_bRealNameChecked = true;

			if (!s_RealNames.empty())
			{
				// Find real name
				uint64_t steamid64 = GetStatusSteamID64();
				auto it = s_RealNames.find(steamid64);

				if (it != s_RealNames.end())
				{
					Q_strncpy(m_szRealName, it->second.c_str(), sizeof(m_szRealName));
				}
			}
		}

		if (IsThisPlayer())
			s_ThisPlayerInfo = this;
	}

	return this;
}

bool CPlayerInfo::HasRealName()
{
	return m_szRealName[0] != '\0';
}

void CPlayerInfo::ClearRealName()
{
	m_szRealName[0] = '\0';
	m_bRealNameChecked = false;
}

player_info_t *CPlayerInfo::GetEnginePlayerInfo()
{
	Assert(m_bIsConnected);
	if (!m_EngineInfo.name)
		return nullptr;
	player_info_t *ptr = reinterpret_cast<player_info_t *>(m_EngineInfo.name - offsetof(player_info_t, name));
	return ptr;
}

void CPlayerInfo::Reset()
{
	m_EngineInfo = hud_player_info_t();
	m_ExtraInfo = extra_player_info_t();
	m_bIsConnected = false;
	m_bIsSpectator = false;
	m_szSteamID[0] = '\0';
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
