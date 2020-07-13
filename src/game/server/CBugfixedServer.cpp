#include <cassert>
#include "extdll.h"
#include "util.h"
#include "edict.h"
#include "cbase.h"
#include "gamerules.h"
#include "CBugfixedServer.h"
#include <appversion.h>
#include "player.h"

static CBugfixedServer g_staticserver;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CBugfixedServer, IBugfixedServer, IBUGFIXEDSERVER_NAME, g_staticserver);

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
	m_pLanCvar = CVAR_GET_POINTER("sv_lan");
	CVAR_REGISTER(&m_NoQueryCvar);
}

void CBugfixedServer::ClientConnect(edict_t *pEntity)
{
	int idx = ENTINDEX(pEntity);

	ResetPlayerData(idx);

	// Bots can't receive network messages
	if (pEntity->v.flags & FL_FAKECLIENT)
	{
		m_pClientInfo[idx].isAuthed = true;
	}
}

void CBugfixedServer::PlayerPostThink(edict_t *pEntity)
{
	int idx = ENTINDEX(pEntity);

	if (!m_pClientInfo[idx].isAuthed && gpGlobals->time >= m_pClientInfo[idx].nextAuthCheck)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pEntity);
		if (pPlayer->m_bIsBot || m_NoQueryCvar.value)
		{
			// Bots can't receive network messages
			// If sv_disable_cvar_query = 1, don't query
			m_pClientInfo[idx].isAuthed = true;
		}
		else
		{
			bool allowQuery = false;
			const char *authId = GETPLAYERAUTHID(pEntity);
			if (m_pLanCvar->value)
			{
				// If sv_lan = 1, all SteamID are STEAM_ID_LAN
				m_pClientInfo[idx].isAuthed = true;
				allowQuery = true;
			}
			else if (authId)
			{
				if (strcmp(authId, "STEAM_ID_PENDING") != 0)
				{
					m_pClientInfo[idx].isAuthed = true;

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

			if (allowQuery)
			{
				QueryClientCvars(pEntity);
			}
		}

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
	}
	else if (requestID == REQUESTID_COLOR)
	{
		// hud_colortext
		int iValue = atoi(value);
		m_pClientInfo[idx].isColorEnabled = (iValue == 1);
	}
}

void CBugfixedServer::ResetPlayerData(int idx)
{
	m_pClientInfo[idx] = bhl_client_info_t();
}

void CBugfixedServer::QueryClientCvars(edict_t *pEntity)
{
	g_engfuncs.pfnQueryClientCvarValue2(pEntity, "aghl_version", REQUESTID_VERSION);
	g_engfuncs.pfnQueryClientCvarValue2(pEntity, "aghl_supports", REQUESTID_SUPPORTS);
	g_engfuncs.pfnQueryClientCvarValue2(pEntity, "hud_colortext", REQUESTID_COLOR);
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
