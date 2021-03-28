#include "amxxmodule.h"
#include "bhl_api.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

// native bhl_is_api_ready()
static cell AMX_NATIVE_CALL bhl_is_api_ready(AMX *amx, cell *params)
{
	return bhl::IsServerApiReady();
}

// native bhl_get_server_version(&major, &minor, &patch);
static cell AMX_NATIVE_CALL bhl_get_server_version(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;

	cell *addr[3];
	addr[0] = MF_GetAmxAddr(amx, params[1]);
	addr[1] = MF_GetAmxAddr(amx, params[2]);
	addr[2] = MF_GetAmxAddr(amx, params[3]);

	const IGameVersion *ver = bhl::serverapi()->GetServerVersion();
	ver->GetVersion(*addr[0], *addr[1], *addr[2]);
	return 1;
}

// native bhl_get_client_supports(idx)
static cell AMX_NATIVE_CALL bhl_get_client_supports(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	return (cell)bhl::serverapi()->GetClientSupports(idx);
}

// native bhl_get_color_support(idx)
static cell AMX_NATIVE_CALL bhl_get_color_support(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	return (cell)bhl::serverapi()->GetColorSupport(idx);
}

// native bhl_get_client_version(idx, &major, &minor, &patch)
static cell AMX_NATIVE_CALL bhl_get_client_version(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;

	cell *major = MF_GetAmxAddr(amx, params[2]);
	cell *minor = MF_GetAmxAddr(amx, params[3]);
	cell *patch = MF_GetAmxAddr(amx, params[4]);

	const IGameVersion *ver = bhl::serverapi()->GetClientVersion(idx);
	if (!ver)
		return 0;
	ver->GetVersion(*major, *minor, *patch);
	return 1;
}

// native bhl_is_client_modified(idx)
static cell AMX_NATIVE_CALL bhl_is_client_dirty(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	const IGameVersion *ver = bhl::serverapi()->GetClientVersion(idx);
	if (!ver)
		return 0;
	return ver->IsDirtyBuild();
}

// native bhl_get_client_version_commit(idx, buf[], size)
static cell AMX_NATIVE_CALL bhl_get_client_version_commit(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	int size = params[3];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	const IGameVersion *ver = bhl::serverapi()->GetClientVersion(idx);
	if (!ver)
		return 0;
	char buf[128];
	ver->GetCommitHash(buf, sizeof(buf));
	MF_SetAmxString(amx, params[2], buf, size);
	return 1;
}

// native bhl_is_client_version_valid(idx)
static cell AMX_NATIVE_CALL bhl_is_client_version_valid(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	return (cell)bhl::serverapi()->IsClientVersionValid(idx);
}

// native bhl_get_automatic_motd(E_MotdType:type)
static cell AMX_NATIVE_CALL bhl_get_automatic_motd(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	return (cell)bhl::serverapi()->GetAutomaticMotd(type);
}

// native bhl_set_automatic_motd(E_MotdType:type, state)
static cell AMX_NATIVE_CALL bhl_set_automatic_motd(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	bool state = params[2];
	bhl::serverapi()->SetAutomaticMotd(type, state);
	return 1;
}

// native bhl_show_motd_from_string(E_MotdType:type, idx, const str[])
static cell AMX_NATIVE_CALL bhl_show_motd_from_string(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	int idx = params[2];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;

	int length;
	const char *str = MF_GetAmxString(amx, params[3], 0, &length);
	bhl::serverapi()->ShowMotdFromString(type, idx, str);
	return 1;
}

// native bhl_show_motd_from_file(E_MotdType:type, idx, const file[])
static cell AMX_NATIVE_CALL bhl_show_motd_from_file(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	int idx = params[2];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;

	int length;
	const char *str = MF_GetAmxString(amx, params[3], 0, &length);
	bhl::serverapi()->ShowMotdFromFile(type, idx, str);
	return 1;
}

// native bhl_set_player_score(idx, frags, deaths);
static cell AMX_NATIVE_CALL bhl_set_player_score(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	bhl::serverapi()->SetPlayerScore(idx, params[2], params[3]);
	return 1;
}

AMX_NATIVE_INFO bugfixedapi_Exports[] = {
	{ "bhl_is_api_ready", bhl_is_api_ready },
	{ "bhl_get_server_version", bhl_get_server_version },
	{ "bhl_get_client_supports", bhl_get_client_supports },
	{ "bhl_get_color_support", bhl_get_color_support },
	{ "bhl_get_client_version", bhl_get_client_version },
	{ "bhl_is_client_dirty", bhl_is_client_dirty },
	{ "bhl_get_client_version_commit", bhl_get_client_version_commit },
	{ "bhl_is_client_version_valid", bhl_is_client_version_valid },
	{ "bhl_get_automatic_motd", bhl_get_automatic_motd },
	{ "bhl_set_automatic_motd", bhl_set_automatic_motd },
	{ "bhl_show_motd_from_string", bhl_show_motd_from_string },
	{ "bhl_show_motd_from_file", bhl_show_motd_from_file },
	{ "bhl_set_player_score", bhl_set_player_score },
	{ nullptr, nullptr }
};

void OnAmxxAttach()
{
	MF_AddNatives(bugfixedapi_Exports);

	bhl::E_ApiInitResult res = bhl::InitServerApi();

	if (res != bhl::E_ApiInitResult::OK)
	{
		LOG_ERROR(PLID, "BugfixedAPI errors:");
		// Print error details
		if (res == bhl::E_ApiInitResult::ModuleNotFound)
		{
			LOG_ERROR(PLID, "Failed to load server library (Sys_LoadModule returned nullptr).");
#ifndef _WIN32
			LOG_ERROR(PLID, "Last dlerror(): %s", dlerror());
#endif
		}
		else if (res == bhl::E_ApiInitResult::FactoryNotFound)
		{
			LOG_ERROR(PLID, "No factory in server library (Sys_GetFactory returned nullptr).");
		}
		else if (res == bhl::E_ApiInitResult::InterfaceNotFound)
		{
			LOG_ERROR(PLID, "Server factory doesn't provide interface %s.", IBUGFIXEDSERVER_NAME);
		}
		else if (res == bhl::E_ApiInitResult::VersionMismatch)
		{
			int major = 0, minor = 0;
			bhl::GetServerApiVersion(major, minor);
			LOG_ERROR(PLID, "Interface version mismatch.");
			LOG_ERROR(PLID, "\tGot:      %d.%d", major, minor);
			LOG_ERROR(PLID, "\tRequired: %d.%d", IBUGFIXEDSERVER_MAJOR, IBUGFIXEDSERVER_MINOR);
		}

		// Print final message
		LOG_ERROR(PLID, "********************************************************");
		LOG_ERROR(PLID, "Failed to get IBugfixedServer from server library.");
		LOG_ERROR(PLID, "Check log above for details.");
		LOG_ERROR(PLID, "All bhl_ AMXX natives will not function.");
		LOG_ERROR(PLID, "********************************************************");
		return;
	}

	LOG_MESSAGE(PLID, "Got IBugfixedServer from server library.");
	LOG_MESSAGE(PLID, "BugfixedAPI AMXX module loaded successfully.");
}

void OnAmxxDetach()
{
	bhl::ShutdownServerApi();
}
