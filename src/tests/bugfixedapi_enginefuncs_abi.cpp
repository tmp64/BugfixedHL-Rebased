#include <string.h>

#include "extdll.h"
#include "enginefuncs_abi.h"

static edict_t *StubEntityOfEntIndex(int)
{
	return nullptr;
}

int main()
{
	enginefuncs_t source{};
	source.pfnPEntityOfEntIndexAllEntities = StubEntityOfEntIndex;

	struct GuardedDestination
	{
		unsigned char table[metamod_abi::EngineFuncsPrefixSize];
		unsigned char guard[16];
	} guarded;
	memset(&guarded, 0xA5, sizeof(guarded));

	metamod_abi::CopyEngineFuncsPrefix(guarded.table, &source);
	if (memcmp(guarded.table, &source, sizeof(guarded.table)) != 0)
		return 1;

	for (unsigned char value : guarded.guard)
	{
		if (value != 0xA5)
			return 2;
	}

	enginefuncs_t imported{};
	imported.pfnPEntityOfEntIndexAllEntities = StubEntityOfEntIndex;
	metamod_abi::InitializeEngineFuncsFromHost(imported, &source);
	if (memcmp(&imported, &source, metamod_abi::EngineFuncsPrefixSize) != 0)
		return 3;
	if (imported.pfnPEntityOfEntIndexAllEntities != nullptr)
		return 4;

	return 0;
}
