#include <Windows.h>
#include <tier1/strtools.h>
#include "plat.h"

plat::Module plat::LoadModuleOrDie(const char *pszName)
{
	Module ret;
	ret.moduleName = V_GetFileName(pszName);

	HMODULE hModule = LoadLibrary(pszName);

	if (!hModule)
	{
		char *lpMsgBuf;

		FormatMessage(
		    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		    NULL,
		    GetLastError(),
		    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		    (LPTSTR)&lpMsgBuf,
		    0,
		    NULL);

		fprintf(stderr, "Failed to load library %s from path %s\n", ret.moduleName.c_str(), pszName);
		fprintf(stderr, "%s\n", lpMsgBuf);

		LocalFree((HLOCAL)lpMsgBuf);

		PLAT_FatalError("LoadModuleOrDie died.");
	}

	ret.pHandle = reinterpret_cast<CSysModule *>(hModule);
	return ret;
}

void *plat::GetProcAddr(const Module &mod, const char *name, bool dieOnError)
{
	void *addr = ::GetProcAddress(reinterpret_cast<HMODULE>(mod.pHandle), name);

	if (!addr && dieOnError)
	{
		fprintf(stderr, "Failed to load proc %s from module %s\n", name, mod.moduleName.c_str());
		PLAT_FatalError("plat::GetProcAddr failed.");
	}

	return addr;
}
