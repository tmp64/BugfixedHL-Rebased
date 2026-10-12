#ifndef CBUGFIXEDSERVER_H
#define CBUGFIXEDSERVER_H

#include <CGameVersion.h>
#include <IBugfixedServer.h>
#include "eiface.h"
#include "progdefs.h"
#include "CScoreboardExt.h"

#ifndef BUGFIXEDAPI_REQUEST_BASE
#define BUGFIXEDAPI_REQUEST_BASE 1
#endif

/**
 * Implementation of IBugfixedServer
 * @see IBugfixedServer
 */
class CBugfixedServer : public bhl::IBugfixedServer
{
public:
	CBugfixedServer();
	void Init();
	void ClientConnect(edict_t *pEntity);
	void PlayerPostThink(edict_t *pEntity);
	void CvarValueCallback(const edict_t *pEnt, int requestID, const char *cvarName, const char *value);
	bool IsClientSupportsReceived(int index);
	void ClientUserInfoChanged(edict_t *pEntity, char *infobuffer);
	void ClientDisconnect(edict_t *pEntity);
	void InitHUD(edict_t *pEntity);
	void StartFrame();
	void ServerDeactivate();

	bool IsClientConnected(int idx);

	//! Whether the client gets "BhlScore": passed InitHUD, has ScoreboardV1, not a bot or HLTV
	bool IsScoreboardClient(int idx);

	//! Queries "_bhl" from clients that passed InitHUD and haven't reported it
	void QueryUnknownClientCaps();

	void PrintClientCaps();

private:
	static constexpr float CAPS_REQUERY_INTERVAL = 10.0f; //!< Seconds between "_bhl" queries of a client that answers them

	enum E_RequestType
	{
		REQUEST_VERSION = 0,
		REQUEST_SUPPORTS,
		REQUEST_COLOR
	};

	enum E_RequestId
	{
		REQUESTID_VERSION = 0,
		REQUESTID_SUPPORTS,
		REQUESTID_COLOR,
		REQUESTID_CAPS,
		REQUESTID_END
	};

	enum E_CvarQuery
	{
		CVARQUERY_ALLOWED = 0,
		CVARQUERY_DENIED, //!< p47 or LAN client
		CVARQUERY_PENDING //!< No SteamID yet
	};

	enum E_CapsSource
	{
		CAPS_SOURCE_UNKNOWN = 0,
		CAPS_SOURCE_USERINFO,
		CAPS_SOURCE_QUERY,
		CAPS_SOURCE_NONE
	};

	struct bhl_client_info_t
	{
		CGameVersion version;
		bhl::E_ClientSupports supports = bhl::E_ClientSupports::None;
		bool isColorEnabled = false;

		bool isAuthed = false; //!< Whether the player has received a SteamID
		float nextAuthCheck = 0; //!< Next time to check for SteamID
		bool isSupportsReceived = false;

		bool isConnected = false;
		bool isReady = false; //!< InitHUD was sent
		bool isBot = false; //!< Cached on connect because Spawn clears FL_FAKECLIENT
		bool isProxy = false; //!< HLTV
		bhl::E_ClientCaps caps = bhl::E_ClientCaps::None; //!< Known bits of "_bhl", GetClientCaps normalizes them
		E_CapsSource capsSource = CAPS_SOURCE_UNKNOWN;
		bool isCapsQueried = false;
		bool isCapsQueryDeferred = false; //!< Waits for a SteamID
		float nextCapsQuery = 0; //!< Next try of a deferred query
		float nextCapsRequery = 0; //!< Next query while the source is CAPS_SOURCE_QUERY
	};

	bhl_client_info_t m_pClientInfo[MAX_PLAYERS + 1];
	CGameVersion m_ServerVersion;
	bhl::E_MotdType m_nMotdType = bhl::E_MotdType::All;
	CScoreboardExt m_Scoreboard;

	void ResetPlayerData(int idx);
	void CheckClientSteamID(edict_t *pEntity);
	E_CvarQuery GetCvarQueryState(edict_t *pEntity);
	void QueryClientCvars(edict_t *pEntity);
	void QueryUnsupportedClientCvars(edict_t *pEntity);
	void OnClientSupportsReceived(edict_t *pEntity);
	void ReadClientCaps(edict_t *pEntity, char *infobuffer);
	void QueryClientCaps(edict_t *pEntity);
	void SetClientCaps(int idx, bhl::E_ClientCaps caps, E_CapsSource source);

	//! Caps as plugins see them (3.1): HudV1 only with ScoreboardV1, None for bots, HLTV and empty slots
	bhl::E_ClientCaps GetNormalizedCaps(int idx);

public:
	//----------------------------------------------------------------------------------------------------
	// IBugfixedServer methods
	//----------------------------------------------------------------------------------------------------
	virtual void GetInterfaceVersion(int &major, int &minor);
	virtual CGameRules **GetGameRulesPtr();
	virtual const IGameVersion *GetServerVersion();
	virtual bhl::E_ClientSupports GetClientSupports(int idx);
	virtual bool GetColorSupport(int idx);
	virtual bool IsClientVersionValid(int idx);
	virtual const IGameVersion *GetClientVersion(int idx);
	virtual bool GetAutomaticMotd(bhl::E_MotdType type);
	virtual void SetAutomaticMotd(bhl::E_MotdType type, bool state);
	virtual void ShowMotdFromString(bhl::E_MotdType type, int idx, const char *str);
	virtual void ShowMotdFromFile(bhl::E_MotdType type, int idx, const char *file);
	virtual void SetPlayerScore(int idx, int frags, int deaths);
	virtual bhl::E_ClientCaps GetClientCaps(int idx);
	virtual bhl::IScoreboardExt *GetScoreboard();
};

CBugfixedServer *serverapi();

#endif
