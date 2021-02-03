#include <cassert>
#include "bhl_api.h"

static bhl::IBugfixedServer *g_pServerApi;
static int g_iServerMajor = 0, g_iServerMinor = 0;

static CSysModule *g_pServerModule = nullptr;
static CreateInterfaceFn g_fnServerFactory = nullptr;

bhl::IBugfixedServer *serverapi()
{
	return g_pServerApi;
}

bool IsServerApiReady()
{
	return (g_pServerApi != nullptr);
}

E_ApiInitResult InitServerApi()
{
	assert(!IsServerApiReady());

	// Load module
#if defined(_WIN32)
	const char *pModNames[] = {
		"hl.dll"
	};
#elif defined(LINUX)
	const char *pModNames[] = {
		"hl.so", "hl_i386.so"
	};
#else
#error Platform not supported: no module name
#endif
	int constexpr MOD_NAME_COUNT = sizeof(pModNames) / sizeof(pModNames[0]);

	//g_pServerModule = Sys_LoadModule(pszModName);
	for (int i = 0; i < MOD_NAME_COUNT; i++)
	{
		g_pServerModule = Sys_LoadModule(pModNames[i]);
		if (g_pServerModule)
			break;
	}

	if (!g_pServerModule)
		return E_ApiInitResult::ModuleNotFound;

	// Get factory
	g_fnServerFactory = Sys_GetFactory(g_pServerModule);
	if (!g_fnServerFactory)
		return E_ApiInitResult::FactoryNotFound;

	// Get interface
	bhl::IBugfixedServer *pIface = static_cast<bhl::IBugfixedServer *>(g_fnServerFactory(IBUGFIXEDSERVER_NAME, nullptr));
	if (!pIface)
		return E_ApiInitResult::InterfaceNotFound;

	// Check version
	pIface->GetInterfaceVersion(g_iServerMajor, g_iServerMinor);
	if (!(g_iServerMajor == IBUGFIXEDSERVER_MAJOR && g_iServerMinor >= IBUGFIXEDSERVER_MINOR))
		return E_ApiInitResult::VersionMismatch;

	g_pServerApi = pIface;
	return E_ApiInitResult::OK;
}

void DeinitServerApi()
{
	g_pServerApi = nullptr;
	g_iServerMajor = 0;
	g_iServerMinor = 0;
	g_pServerModule = nullptr;
	g_fnServerFactory = nullptr;
}

void GetServerApiVersion(int &major, int &minor)
{
	major = g_iServerMajor;
	minor = g_iServerMinor;
}
