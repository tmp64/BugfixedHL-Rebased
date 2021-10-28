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
}

bool CBugfixedServer::IsClientSupportsReceived(int index)
{
	return m_pClientInfo[index].isSupportsReceived;
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
		bool isAuthed = false;
		bool allowQuery = false;
		const char *authId = GETPLAYERAUTHID(pEntity);

		if (authId)
		{
			if (strcmp(authId, "STEAM_ID_PENDING") != 0)
			{
				isAuthed = true;
				clientInfo.isAuthed = true;

				if (!strcmp(authId, "STEAM_ID_LAN") || !strncmp(authId, "VALVE_", 6))
				{
					// p47 clients don't support QueryClientCvarValue2
					allowQuery = false;
				}
				else
				{
					allowQuery = true;
				}
			}
		}

		if (isAuthed)
		{
			if (allowQuery)
				QueryClientCvars(pEntity);
			else
				QueryUnsupportedClientCvars(pEntity);
		}
	}
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
