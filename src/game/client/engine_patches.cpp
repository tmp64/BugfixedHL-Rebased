#include "hud.h"
#include "cl_util.h"
#include "engine_patches.h"

static CEnginePatches *s_pInstance = nullptr;

CEnginePatches &CEnginePatches::Get()
{
	Assert(s_pInstance);
	return *s_pInstance;
}

CEnginePatches::CEnginePatches()
{
	Assert(!s_pInstance);
	s_pInstance = this;
	memset(&m_EngineSvcHandlers.funcs, 0, sizeof(m_EngineSvcHandlers.funcs));
}

CEnginePatches::~CEnginePatches()
{
	Assert(s_pInstance);
	s_pInstance = nullptr;
}

void CEnginePatches::Init()
{
	if (gEngfuncs.CheckParm("-nopatch", nullptr))
	{
		ConPrintf("Engine patching disabled with -nopatch.\n");
		return;
	}

	m_bLatePatchesDone = false;

	PlatformPatchesInit();
	FindMsgBuf();
	FindSvcArray();
	FindUserMessageList();
}

void CEnginePatches::Shutdown()
{
	PlatformPatchesShutdown();

	m_MsgBuf = EngineMsgBuf();
	m_pSvcArray = nullptr;
	m_pUserMsgList = nullptr;
}

void CEnginePatches::RunFrame()
{
	if (!m_bLatePatchesDone)
	{
		PlatformLatePatching();
		m_bLatePatchesDone = true;
	}
}

void CEnginePatches::HookSvcHandlers(SvcParseFunc array[SVC_MSG_COUNT])
{
	Assert(false);
}

void CEnginePatches::DisableExitCommands()
{
	CmdFunction fn = []() {
		ConPrintf("Quiting has been temporarily disabled.\n");
	};

	for (cmd_function_t *pItem = gEngfuncs.GetFirstCmdFunctionHandle(); pItem; pItem = pItem->next)
	{
		if (!strcmp(pItem->name, "quit"))
		{
			if (!m_fnEngineQuitCmd)
			{
				m_fnEngineQuitCmd = pItem->function;
				pItem->function = fn;
			}
		}
		else if (!strcmp(pItem->name, "_restart"))
		{
			if (!m_fnEngineRestartCmd)
			{
				m_fnEngineRestartCmd = pItem->function;
				pItem->function = fn;
			}
		}
	}
}

void CEnginePatches::EnableExitCommands()
{
	for (cmd_function_t *pItem = gEngfuncs.GetFirstCmdFunctionHandle(); pItem; pItem = pItem->next)
	{
		if (!strcmp(pItem->name, "quit"))
		{
			if (m_fnEngineQuitCmd)
			{
				pItem->function = m_fnEngineQuitCmd;
				m_fnEngineQuitCmd = nullptr;
			}
		}
		else if (!strcmp(pItem->name, "_restart"))
		{
			if (!m_fnEngineRestartCmd)
			{
				pItem->function = m_fnEngineRestartCmd;
				m_fnEngineRestartCmd = nullptr;
			}
		}
	}
}

void CEnginePatches::PlatformPatchesInit()
{
}

void CEnginePatches::PlatformPatchesShutdown()
{
}

void CEnginePatches::PlatformLatePatching()
{
}

void CEnginePatches::FindMsgBuf()
{
}

void CEnginePatches::FindSvcArray()
{
}

void CEnginePatches::FindUserMessageList()
{
}

//-------------------------------------------------------------------
// Utility functions
//-------------------------------------------------------------------

size_t CEnginePatches::ConvertHexString(const char *srcHexString, unsigned char *outBuffer, size_t bufferSize)
{
	unsigned char *in = (unsigned char *)srcHexString;
	unsigned char *out = outBuffer;
	unsigned char *end = outBuffer + bufferSize;
	bool low = false;
	uint8_t byte = 0;
	while (*in && out < end)
	{
		if (*in >= '0' && *in <= '9')
		{
			byte |= *in - '0';
		}
		else if (*in >= 'A' && *in <= 'F')
		{
			byte |= *in - 'A' + 10;
		}
		else if (*in >= 'a' && *in <= 'f')
		{
			byte |= *in - 'a' + 10;
		}
		else if (*in == ' ')
		{
			in++;
			continue;
		}

		if (!low)
		{
			byte = byte << 4;
			in++;
			low = true;
			continue;
		}
		low = false;

		*out = byte;
		byte = 0;

		in++;
		out++;
	}
	return out - outBuffer;
}

uintptr_t CEnginePatches::MemoryFindForward(uintptr_t start, uintptr_t end, const uint8_t *pattern, const uint8_t *mask, size_t pattern_len)
{
	// Ensure start is lower than the end
	if (start > end)
	{
		size_t reverse = end;
		end = start;
		start = reverse;
	}

	unsigned char *cend = (unsigned char *)(end - pattern_len + 1);
	unsigned char *current = (unsigned char *)(start);

	// Just linear search for sequence of bytes from the start till the end minus pattern length
	size_t i;
	if (mask)
	{
		// honoring mask
		while (current < cend)
		{
			for (i = 0; i < pattern_len; i++)
			{
				if ((current[i] & mask[i]) != (pattern[i] & mask[i]))
					break;
			}

			if (i == pattern_len)
				return (size_t)(void *)current;

			current++;
		}
	}
	else
	{
		// without mask
		while (current < cend)
		{
			for (i = 0; i < pattern_len; i++)
			{
				if (current[i] != pattern[i])
					break;
			}

			if (i == pattern_len)
				return (size_t)(void *)current;

			current++;
		}
	}

	return 0;
}

uintptr_t CEnginePatches::MemoryFindForward(uintptr_t start, uintptr_t end, const char *pattern, const char *mask)
{
	uint8_t p[MAX_PATTERN];
	uint8_t m[MAX_PATTERN];
	size_t pl = ConvertHexString(pattern, p, sizeof(p));
	size_t ml = mask != nullptr ? ConvertHexString(mask, m, sizeof(m)) : 0;
	return MemoryFindForward(start, end, p, mask != nullptr ? m : nullptr, pl >= ml ? pl : ml);
}

uintptr_t CEnginePatches::MemoryFindBackward(uintptr_t start, uintptr_t end, const uint8_t *pattern, const uint8_t *mask, size_t pattern_len)
{
	// Ensure start is higher than the end
	if (start < end)
	{
		size_t reverse = end;
		end = start;
		start = reverse;
	}

	unsigned char *cend = (unsigned char *)(end);
	unsigned char *current = (unsigned char *)(start - pattern_len);

	// Just linear search backward for sequence of bytes from the start minus pattern length till the end
	size_t i;
	if (mask)
	{
		// honoring mask
		while (current >= cend)
		{
			for (i = 0; i < pattern_len; i++)
			{
				if ((current[i] & mask[i]) != (pattern[i] & mask[i]))
					break;
			}

			if (i == pattern_len)
				return (size_t)(void *)current;

			current--;
		}
	}
	else
	{
		// without mask
		while (current >= cend)
		{
			for (i = 0; i < pattern_len; i++)
			{
				if (current[i] != pattern[i])
					break;
			}

			if (i == pattern_len)
				return (size_t)(void *)current;

			current--;
		}
	}

	return 0;
}

uintptr_t CEnginePatches::MemoryFindBackward(uintptr_t start, uintptr_t end, const char *pattern, const char *mask)
{
	uint8_t p[MAX_PATTERN];
	uint8_t m[MAX_PATTERN];
	size_t pl = ConvertHexString(pattern, p, sizeof(p));
	size_t ml = mask != nullptr ? ConvertHexString(mask, m, sizeof(m)) : 0;
	return MemoryFindBackward(start, end, p, mask != nullptr ? m : nullptr, pl >= ml ? pl : ml);
}
