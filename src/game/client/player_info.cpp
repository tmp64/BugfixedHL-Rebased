#include "hud.h"
#include "player_info.h"

team_info_t g_TeamInfo[MAX_TEAMS + 1];

CPlayerInfo CPlayerInfo::m_sPlayerInfo[MAX_PLAYERS + 1];

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

CPlayerInfo *CPlayerInfo::Update()
{
	gEngfuncs.pfnGetPlayerInfo(m_iIndex, &m_EngineInfo);
	m_bIsConnected = m_EngineInfo.name != nullptr;

	return this;
}
