#include <cassert>
#include "bhl_api.h"
#include "amxxmodule.h"

namespace
{

bhl::IBugfixedServer *g_pServerApi;
int g_iServerMajor = 0, g_iServerMinor = 0;

CSysModule *g_pServerModule = nullptr;
CreateInterfaceFn g_fnServerFactory = nullptr;

}

bhl::IBugfixedServer *bhl::serverapi()
{
	return g_pServerApi;
}

bool bhl::IsServerApiReady()
{
	return (g_pServerApi != nullptr);
}

bhl::E_ApiInitResult bhl::InitServerApi()
{
	assert(!IsServerApiReady());

	const char *pszModule = gpMetaUtilFuncs->pfnGetGameInfo(PLID, GINFO_DLL_FULLPATH);
	LOG_DEVELOPER(PLID, "Server DLL path: %s", pszModule);
	
	g_pServerModule = Sys_LoadModule(pszModule);

	if (!g_pServerModule)
		return E_ApiInitResult::ModuleNotFound;

	// Decrease reference count - module should still be loaded by the engine
	Sys_UnloadModule(g_pServerModule);

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

void bhl::ShutdownServerApi()
{
	g_pServerApi = nullptr;
	g_iServerMajor = 0;
	g_iServerMinor = 0;
	g_pServerModule = nullptr;
	g_fnServerFactory = nullptr;
}

void bhl::GetServerApiVersion(int &major, int &minor)
{
	major = g_iServerMajor;
	minor = g_iServerMinor;
}
