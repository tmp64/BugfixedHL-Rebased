#ifndef TESTS_COMMON_PLAT_H
#define TESTS_COMMON_PLAT_H
#include <string>
#include <tier1/interface.h>

[[noreturn]] void PLAT_FatalError(const std::string &msg);

namespace plat
{

struct Module
{
	CSysModule *pHandle = nullptr;
	std::string moduleName;
};

Module LoadModuleOrDie(const char *pszName);
void *GetProcAddr(const Module &mod, const char *name, bool dieOnError = false);

}

#endif
