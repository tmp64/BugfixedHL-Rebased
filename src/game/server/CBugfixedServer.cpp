#include <cassert>
#include "extdll.h"
#include "util.h"
#include "edict.h"
#include "cbase.h"
#include "gamerules.h"
#include "convar.h"
#include "CBugfixedServer.h"
#include <appversion.h>
#include "player.h"

static CBugfixedServer g_staticserver;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CBugfixedServer, IBugfixedServer, IBUGFIXEDSERVER_NAME, g_staticserver);

ConVar sv_bhl_query_vars("sv_bhl_query_vars", "0", FCVAR_SERVER, "Enable querying of client capability and version cvars");
ConVar sv_bhl_query_wait_for_id("sv_bhl_query_wait_for_id", "0", FCVAR_SERVER, "Wait for a valid SteamID before querying");
ConVar sv_bhl_defer_motd("sv_bhl_defer_motd", "0", FCVAR_SERVER, "Wait for client cvars before sending the MOTD");

CBugfixedServer *serverapi()
{
	return &g_staticserver;
}

CON_COMMAND(bhl_caps, "Prints capabilities of connected clients")
{
	serverapi()->PrintClientCaps();
}

//! Parses "_bhl": 1 to 8 hex digits without a sign, spaces or prefix, anything else is 0. Unknown bits are dropped.
static bhl::E_ClientCaps ParseClientCaps(const char *value)
{
	if (!value)
		return bhl::E_ClientCaps::None;

	size_t len = strlen(value);

	if (len < 1 || len > 8 || strspn(value, "0123456789abcdefABCDEF") != len)
		return bhl::E_ClientCaps::None;

	return (bhl::E_ClientCaps)(strtoul(value, nullptr, 16) & (unsigned long)bhl::E_ClientCaps::All);
}

static bool IsProxy(edict_t *pEntity)
{
	const char *authId = GETPLAYERAUTHID(pEntity);
	return (pEntity->v.flags & FL_PROXY) || (authId && !strcmp(authId, "HLTV"));
}

//----------------------------------------------------------------------------------------------------
// CBugfixedServer
//----------------------------------------------------------------------------------------------------
CBugfixedServer::CBugfixedServer()
{
	m_ServerVersion = CGameVersion(APP_VERSION);
	assert(m_ServerVersion.IsValid());
}

void CBugfixedServer::Init()
{
}

void CBugfixedServer::ClientConnect(edict_t *pEntity)
{
	int idx = ENTINDEX(pEntity);

	ResetPlayerData(idx);
	const char *authId = GETPLAYERAUTHID(pEntity);

	m_pClientInfo[idx].isConnected = true;
	m_pClientInfo[idx].isBot = (pEntity->v.flags & FL_FAKECLIENT) || (authId && !strcmp(authId, "BOT"));
	m_pClientInfo[idx].isProxy = IsProxy(pEntity);
	m_Scoreboard.ClearPlayer(idx);
	ReadClientCaps(pEntity, g_engfuncs.pfnGetInfoKeyBuffer(pEntity));

	if ((m_pClientInfo[idx].isBot || m_pClientInfo[idx].isProxy) && m_pClientInfo[idx].capsSource == CAPS_SOURCE_UNKNOWN)
		m_pClientInfo[idx].capsSource = CAPS_SOURCE_NONE;

	// Bots can't receive network messages
	if (pEntity->v.flags & FL_FAKECLIENT)
	{
		m_pClientInfo[idx].isAuthed = true;
	}

	if (!sv_bhl_query_wait_for_id.GetBool())
	{
		// SteamID is not used when sv_bhl_query_wait_for_id is 0.
		// Assume the player is authed.
		m_pClientInfo[idx].isAuthed = true;
	}

	if (sv_bhl_query_vars.GetBool())
	{
		if (sv_bhl_query_wait_for_id.GetBool())
			CheckClientSteamID(pEntity);
		else
			QueryClientCvars(pEntity);
	}
	else
	{
		QueryUnsupportedClientCvars(pEntity);
	}
}

void CBugfixedServer::PlayerPostThink(edict_t *pEntity)
{
	int idx = ENTINDEX(pEntity);

	if (sv_bhl_query_vars.GetBool()
	    && !m_pClientInfo[idx].isAuthed
	    && gpGlobals->time >= m_pClientInfo[idx].nextAuthCheck)
	{
		CheckClientSteamID(pEntity);
		m_pClientInfo[idx].nextAuthCheck = gpGlobals->time + 0.1;
	}

	if (m_pClientInfo[idx].isCapsQueryDeferred && gpGlobals->time >= m_pClientInfo[idx].nextCapsQuery)
	{
		QueryClientCaps(pEntity);
		m_pClientInfo[idx].nextCapsQuery = gpGlobals->time + 0.1;
	}

	// A changed client setting reaches the server only with a new answer
	if (m_pClientInfo[idx].capsSource == CAPS_SOURCE_QUERY
	    && m_pClientInfo[idx].isReady
	    && m_Scoreboard.IsActive()
	    && gpGlobals->time >= m_pClientInfo[idx].nextCapsRequery)
	{
		m_pClientInfo[idx].nextCapsRequery = gpGlobals->time + CAPS_REQUERY_INTERVAL;
		g_engfuncs.pfnQueryClientCvarValue2(pEntity, bhl::sb::CAPS_KEY, REQUESTID_CAPS);
	}
}

void CBugfixedServer::CvarValueCallback(const edict_t *pEnt, int requestID, const char *cvarName, const char *value)
{
	int idx = ENTINDEX(pEnt);
	if (idx < 1 || idx > gpGlobals->maxClients)
		return; // Invalid pEnt

	if (requestID == REQUESTID_VERSION)
	{
		// aghl_version
		m_pClientInfo[idx].version.TryParse(value);
	}
	else if (requestID == REQUESTID_SUPPORTS)
	{
		// aghl_supports
		unsigned int iValue = strtoul(value, nullptr, 10);
		if (iValue == ULONG_MAX)
			iValue = 0;
		m_pClientInfo[idx].supports = (bhl::E_ClientSupports)iValue;
		OnClientSupportsReceived(const_cast<edict_t *>(pEnt));
	}
	else if (requestID == REQUESTID_COLOR)
	{
		// hud_colortext
		int iValue = atoi(value);
		m_pClientInfo[idx].isColorEnabled = (iValue == 1);
	}
	else if (requestID == REQUESTID_CAPS && !strcmp(cvarName, bhl::sb::CAPS_KEY))
	{
		// Other plugins get their answers here too, hence the name check.
		// Userinfo wins over a late answer.
		if (m_pClientInfo[idx].isCapsQueried && m_pClientInfo[idx].capsSource != CAPS_SOURCE_USERINFO)
			SetClientCaps(idx, ParseClientCaps(value), CAPS_SOURCE_QUERY);
	}
}

bool CBugfixedServer::IsClientSupportsReceived(int index)
{
	return m_pClientInfo[index].isSupportsReceived;
}

void CBugfixedServer::ClientUserInfoChanged(edict_t *pEntity, char *infobuffer)
{
	int idx = ENTINDEX(pEntity);
	if (idx < 1 || idx > gpGlobals->maxClients)
		return;

	ReadClientCaps(pEntity, infobuffer);
}

void CBugfixedServer::ClientDisconnect(edict_t *pEntity)
{
	int idx = ENTINDEX(pEntity);
	if (idx < 1 || idx > gpGlobals->maxClients)
		return;

	m_Scoreboard.ClearPlayer(idx);
	ResetPlayerData(idx);
}

void CBugfixedServer::InitHUD(edict_t *pEntity)
{
	int idx = ENTINDEX(pEntity);
	bhl_client_info_t &clientInfo = m_pClientInfo[idx];

	clientInfo.isReady = true;
	clientInfo.isProxy = clientInfo.isProxy || IsProxy(pEntity);

	// Also without caps and when inactive: caps that arrive later and an extension
	// turned on later must still wait for the InitHUD burst
	m_Scoreboard.StartFullSync(idx, true);

	QueryClientCaps(pEntity);
}

void CBugfixedServer::StartFrame()
{
	// The engine calls ClientDisconnect only for spawned clients. A client dropped while it loads
	// the map or downloads resources is seen only by its edict having no userid any more.
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (m_pClientInfo[i].isConnected && GETPLAYERUSERID(INDEXENT(i)) == -1)
		{
			m_Scoreboard.ClearPlayer(i);
			ResetPlayerData(i);
		}
	}

	m_Scoreboard.StartFrame();
}

void CBugfixedServer::ServerDeactivate()
{
	// Clients that stay connect again on the new map. A slot dropped before spawning on this map
	// gets an edict again there, so StartFrame could no longer see that it is empty.
	for (bhl_client_info_t &clientInfo : m_pClientInfo)
	{
		clientInfo.isConnected = false;
		clientInfo.isReady = false;
	}

	m_Scoreboard.ClearAll();
}

bool CBugfixedServer::IsClientConnected(int idx)
{
	return m_pClientInfo[idx].isConnected;
}

bool CBugfixedServer::IsScoreboardClient(int idx)
{
	return m_pClientInfo[idx].isReady && IsEnumFlagSet(GetNormalizedCaps(idx), bhl::E_ClientCaps::ScoreboardV1);
}

void CBugfixedServer::QueryUnknownClientCaps()
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (m_pClientInfo[i].isReady)
			QueryClientCaps(INDEXENT(i));
	}
}

void CBugfixedServer::PrintClientCaps()
{
	static const char *sourceNames[] = { "unknown", "userinfo", "query", "none" };

	g_engfuncs.pfnServerPrint(UTIL_VarArgs("%4s %-32s %8s %s\n", "slot", "name", "caps", "source"));

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		const bhl_client_info_t &clientInfo = m_pClientInfo[i];

		if (!clientInfo.isConnected)
			continue;

		g_engfuncs.pfnServerPrint(UTIL_VarArgs("%4d %-32s %8x %s\n", i, STRING(INDEXENT(i)->v.netname), (unsigned int)GetNormalizedCaps(i), sourceNames[clientInfo.capsSource]));
	}
}

void CBugfixedServer::ResetPlayerData(int idx)
{
	m_pClientInfo[idx] = bhl_client_info_t();
}

void CBugfixedServer::CheckClientSteamID(edict_t *pEntity)
{
	bhl_client_info_t &clientInfo = m_pClientInfo[ENTINDEX(pEntity)];

	if (pEntity->v.flags & FL_FAKECLIENT)
	{
		// Bots can't receive network messages
		clientInfo.isAuthed = true;
	}
	else
	{
		E_CvarQuery query = GetCvarQueryState(pEntity);

		if (query != CVARQUERY_PENDING)
		{
			clientInfo.isAuthed = true;

			if (query == CVARQUERY_ALLOWED)
				QueryClientCvars(pEntity);
			else
				QueryUnsupportedClientCvars(pEntity);
		}
	}
}

CBugfixedServer::E_CvarQuery CBugfixedServer::GetCvarQueryState(edict_t *pEntity)
{
	const char *authId = GETPLAYERAUTHID(pEntity);

	if (!authId || !strcmp(authId, "STEAM_ID_PENDING"))
		return CVARQUERY_PENDING;

	// p47 clients don't support QueryClientCvarValue2
	if (!strcmp(authId, "STEAM_ID_LAN") || !strncmp(authId, "VALVE_", 6))
		return CVARQUERY_DENIED;

	return CVARQUERY_ALLOWED;
}

void CBugfixedServer::QueryClientCvars(edict_t *pEntity)
{
	g_engfuncs.pfnQueryClientCvarValue2(pEntity, "aghl_supports", REQUESTID_SUPPORTS);
	g_engfuncs.pfnQueryClientCvarValue2(pEntity, "aghl_version", REQUESTID_VERSION);
	g_engfuncs.pfnQueryClientCvarValue2(pEntity, "hud_colortext", REQUESTID_COLOR);
}

void CBugfixedServer::QueryUnsupportedClientCvars(edict_t *pEntity)
{
	// Pretend we got something to show the MOTD
	OnClientSupportsReceived(pEntity);
}

void CBugfixedServer::OnClientSupportsReceived(edict_t *pEntity)
{
	CBasePlayer *pPlayer = static_cast<CBasePlayer *>(CBaseEntity::Instance(pEntity));

	if (pPlayer && pPlayer->m_fGameHUDInitialized
	    && g_pGameRules->IsMultiplayer()
	    && sv_bhl_defer_motd.GetBool())
		static_cast<CHalfLifeMultiplay *>(g_pGameRules)->SendDefaultMOTDToClient(pEntity);

	m_pClientInfo[ENTINDEX(pEntity)].isSupportsReceived = true;
}

void CBugfixedServer::ReadClientCaps(edict_t *pEntity, char *infobuffer)
{
	int idx = ENTINDEX(pEntity);
	bhl_client_info_t &clientInfo = m_pClientInfo[idx];
	const char *value = g_engfuncs.pfnInfoKeyValue(infobuffer, bhl::sb::CAPS_KEY);

	if (value[0])
	{
		SetClientCaps(idx, ParseClientCaps(value), CAPS_SOURCE_USERINFO);
	}
	else if (clientInfo.capsSource == CAPS_SOURCE_USERINFO)
	{
		clientInfo.capsSource = CAPS_SOURCE_UNKNOWN;
		clientInfo.isCapsQueried = false;

		if (clientInfo.isReady)
			QueryClientCaps(pEntity);
	}
}

void CBugfixedServer::QueryClientCaps(edict_t *pEntity)
{
	int idx = ENTINDEX(pEntity);
	bhl_client_info_t &clientInfo = m_pClientInfo[idx];

	clientInfo.isCapsQueryDeferred = false;

	if (clientInfo.capsSource != CAPS_SOURCE_UNKNOWN || clientInfo.isCapsQueried)
		return;

	if (!m_Scoreboard.IsActive() && !sv_bhl_query_vars.GetBool())
		return;

	E_CvarQuery query = (clientInfo.isBot || clientInfo.isProxy) ? CVARQUERY_DENIED : GetCvarQueryState(pEntity);

	if (query == CVARQUERY_ALLOWED)
	{
		clientInfo.isCapsQueried = true;
		clientInfo.nextCapsRequery = gpGlobals->time + CAPS_REQUERY_INTERVAL;
		g_engfuncs.pfnQueryClientCvarValue2(pEntity, bhl::sb::CAPS_KEY, REQUESTID_CAPS);
	}
	else if (query == CVARQUERY_DENIED)
	{
		SetClientCaps(idx, bhl::E_ClientCaps::None, CAPS_SOURCE_NONE);
	}
	else
	{
		clientInfo.isCapsQueryDeferred = true;
	}
}

void CBugfixedServer::SetClientCaps(int idx, bhl::E_ClientCaps caps, E_CapsSource source)
{
	bool hadScoreboard = IsScoreboardClient(idx);

	m_pClientInfo[idx].caps = caps;
	m_pClientInfo[idx].capsSource = source;

	// A client that lost the cap is skipped by the scoreboard without a Reset
	if (!hadScoreboard && IsScoreboardClient(idx))
		m_Scoreboard.StartFullSync(idx, false);
}

bhl::E_ClientCaps CBugfixedServer::GetNormalizedCaps(int idx)
{
	const bhl_client_info_t &clientInfo = m_pClientInfo[idx];

	// HudV1 alone would make a plugin drop its HUD texts for a client that gets no data
	if (!clientInfo.isConnected || clientInfo.isBot || clientInfo.isProxy || !IsEnumFlagSet(clientInfo.caps, bhl::E_ClientCaps::ScoreboardV1))
		return bhl::E_ClientCaps::None;

	return clientInfo.caps;
}

//----------------------------------------------------------------------------------------------------
// IBugfixedServer methods
//----------------------------------------------------------------------------------------------------
void CBugfixedServer::GetInterfaceVersion(int &major, int &minor)
{
	major = IBUGFIXEDSERVER_MAJOR;
	minor = IBUGFIXEDSERVER_MINOR;
}

CGameRules **CBugfixedServer::GetGameRulesPtr()
{
	return &g_pGameRules;
}

const IGameVersion *CBugfixedServer::GetServerVersion()
{
	return &m_ServerVersion;
}

bhl::E_ClientSupports CBugfixedServer::GetClientSupports(int idx)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::GetClientSupports(): invalid player id (%d)\n", idx);
		return bhl::E_ClientSupports::None;
	}
	return m_pClientInfo[idx].supports;
}

bool CBugfixedServer::GetColorSupport(int idx)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::GetColorSupport(): invalid player id (%d)\n", idx);
		return false;
	}
	return m_pClientInfo[idx].isColorEnabled;
}

bool CBugfixedServer::IsClientVersionValid(int idx)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::IsClientVersionValid(): invalid player id (%d)\n", idx);
		return false;
	}
	return m_pClientInfo[idx].version.IsValid();
}

const IGameVersion *CBugfixedServer::GetClientVersion(int idx)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::IsClientVersionValid(): invalid player id (%d)\n", idx);
		return nullptr;
	}
	if (!m_pClientInfo[idx].version.IsValid())
		return nullptr;
	return &m_pClientInfo[idx].version;
}

bool CBugfixedServer::GetAutomaticMotd(bhl::E_MotdType type)
{
	return IsEnumFlagSet(m_nMotdType, type);
}

void CBugfixedServer::SetAutomaticMotd(bhl::E_MotdType type, bool state)
{
	if (state)
		SetEnumFlag(m_nMotdType, type);
	else
		ClearEnumFlag(m_nMotdType, type);
}

void CBugfixedServer::ShowMotdFromString(bhl::E_MotdType type, int idx, const char *cstr)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::ShowMotdFromString(): invalid player id (%d)\n", idx);
		return;
	}

	CHalfLifeMultiplay *pRules = dynamic_cast<CHalfLifeMultiplay *>(g_pGameRules);
	assert(pRules);
	if (!pRules)
	{
		UTIL_LogPrintf("CBugfixedServer::ShowMotdFromString(): g_pGameRules is not CHalfLifeMultiplay\n");
		return;
	}

	char *str = const_cast<char *>(cstr);
	edict_t *pEdict = INDEXENT(idx);

	if (type == bhl::E_MotdType::Plain)
	{
		pRules->SendMOTDToClient(pEdict, str);
	}
	else if (type == bhl::E_MotdType::Unicode)
	{
		pRules->SendUnicodeMOTDToClient(pEdict, str);
	}
	else if (type == bhl::E_MotdType::Html)
	{
		pRules->SendHtmlMOTDToClient(pEdict, str);
	}
}

void CBugfixedServer::ShowMotdFromFile(bhl::E_MotdType type, int idx, const char *file)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::ShowMotdFromFile(): invalid player id (%d)\n", idx);
		return;
	}

	CHalfLifeMultiplay *pRules = dynamic_cast<CHalfLifeMultiplay *>(g_pGameRules);
	assert(pRules);
	if (!pRules)
	{
		UTIL_LogPrintf("CBugfixedServer::ShowMotdFromFile(): g_pGameRules is not CHalfLifeMultiplay\n");
		return;
	}

	edict_t *pEdict = INDEXENT(idx);

	bool result = false;

	if (type == bhl::E_MotdType::Plain)
	{
		result = pRules->SendMOTDFileToClient(pEdict, file);
	}
	else if (type == bhl::E_MotdType::Unicode)
	{
		result = pRules->SendUnicodeMOTDFileToClient(pEdict, file);
	}
	else if (type == bhl::E_MotdType::Html)
	{
		result = pRules->SendHtmlMOTDFileToClient(pEdict, file);
	}

	if (!result)
		UTIL_LogPrintf("CBugfixedServer::ShowMotdFromFile(): file not found: %s\n", file);
}

void CBugfixedServer::SetPlayerScore(int idx, int frags, int deaths)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::SetPlayerScore(): invalid player id (%d)\n", idx);
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(idx);

	if (!pPlayer)
	{
		UTIL_LogPrintf("CBugfixedServer::SetPlayerScore(): player %d not yet ready\n", idx);
		return;
	}

	pPlayer->pev->frags = frags;
	pPlayer->m_iDeaths = deaths;
	pPlayer->SendScoreInfo();
}

bhl::E_ClientCaps CBugfixedServer::GetClientCaps(int idx)
{
	if (idx < 1 || idx > gpGlobals->maxClients)
	{
		UTIL_LogPrintf("CBugfixedServer::GetClientCaps(): invalid player id (%d)\n", idx);
		return bhl::E_ClientCaps::None;
	}
	return GetNormalizedCaps(idx);
}

bhl::IScoreboardExt *CBugfixedServer::GetScoreboard()
{
	return &m_Scoreboard;
}
