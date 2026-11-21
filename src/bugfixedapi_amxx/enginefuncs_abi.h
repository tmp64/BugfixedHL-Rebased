#pragma once

#include <stddef.h>
#include <string.h>

namespace metamod_abi
{
constexpr size_t EngineFuncsPrefixSize =
    offsetof(enginefuncs_t, pfnPEntityOfEntIndexAllEntities);

inline void CopyEngineFuncsPrefix(void *destination, const void *source)
{
	memcpy(destination, source, EngineFuncsPrefixSize);
}

inline void InitializeEngineFuncsFromHost(enginefuncs_t &destination, const void *source)
{
	destination = {};
	CopyEngineFuncsPrefix(&destination, source);
}
}
