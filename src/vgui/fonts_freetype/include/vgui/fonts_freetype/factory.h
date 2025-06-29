#pragma once
#include <tier1/interface.h>
#include <bhl/logging/ILogger.h>

void ConnectVGuiFreeTypeLibraries(CreateInterfaceFn *pFactories, int iNumFactories, ILogger* pLogger);
CreateInterfaceFn GetFreeTypeFactory();
