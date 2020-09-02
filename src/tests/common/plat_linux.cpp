#include <dlfcn.h>
#include <tier1/strtools.h>
#include "plat.h"

plat::Module plat::LoadModuleOrDie(const char *pszName)
{
	Module ret;
	ret.moduleName = V_GetFileName(pszName);

	void *hModule = dlopen(pszName, RTLD_NOW);

	if (!hModule)
	{
		fprintf(stderr, "Failed to load library %s from path %s\n", ret.moduleName.c_str(), pszName);
		fprintf(stderr, "%s\n", dlerror());
		PLAT_FatalError("LoadModuleOrDie died.");
	}

	ret.pHandle = reinterpret_cast<CSysModule *>(hModule);
	return ret;
}

void *plat::GetProcAddr(const Module &mod, const char *name, bool dieOnError)
{
	void *addr = dlsym(reinterpret_cast<void *>(mod.pHandle), name);

	if (!addr && dieOnError)
	{
		fprintf(stderr, "Failed to load proc %s from module %s\n", name, mod.moduleName.c_str());
		PLAT_FatalError("plat::GetProcAddress failed.");
	}

	return addr;
}
