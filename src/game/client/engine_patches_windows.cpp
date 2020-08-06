#include <winsani_in.h>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <Winternl.h>
#include <winsani_out.h>

#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "engine_patches.h"
#include "parsemsg.h"

class CEnginePatchesWindows : public CEnginePatches
{
public:
	struct Module
	{
		uintptr_t iBase = 0;
		uintptr_t iEnd = 0;

		/**
		 * Loads address of specified module. Returns true on success.
		 */
		bool ReadModule(const char *moduleName);

		/**
		 * Returns true if iBase and iEnd are set.
		 */
		inline bool IsValid() const
		{
			return iBase != 0;
		}
	};

	virtual void RunFrame() override;

protected:
	virtual void PlatformPatchesInit() override;
	virtual void PlatformPatchesShutdown() override;
	virtual void PlatformLatePatching() override;
	virtual void FindMsgBuf() override;
	virtual void FindSvcArray() override;
	virtual void FindUserMessageList() override;

private:
	Module m_EngineModule;
	Module m_ServerBrowserModule;

	// FPS bug
	uintptr_t m_iFpsBugPlace = 0;
	uint8_t m_FpsBugPlaceBackup[16] = { 0 };
	double *m_pflFrameTime = nullptr;
	double m_flFrameTimeReminder = 0;

	// Connectionless Packet
	using ConnectionlessPacketFunc = void (*)();
	ConnectionlessPacketFunc m_fnEngineCLPHandler = nullptr;
	uintptr_t m_iEngineCLPPlace = 0;
	uint32_t m_iEngineCLPOffset = 0;

	// Console command hooks
	CmdFunction m_fnEngineMotdWrite = nullptr;

	/**
	 * Loads address of engine module.
	 */
	bool GetEngineModule();

	/**
	 * Removes physics dependence on FPS.
	 */
	void PatchFPSBug(bool enable);

	/**
	 * Function that eliminates FPS bug. Injected into the engine.
	 * It is using stdcall convention so it will clear the stack.
	 */
	static void __stdcall FpsBugFix(int a1, int64_t *a2);

	/**
	 * Connectionless packet sanitizing
	 */
	void PatchConnectionlessPacketHandler(bool enable);

	/**
	 * Our ConnectionlessPacket handler which sanitizes redirect packet
	 */
	static void ConnectionlessPacketHandler();

	/**
	 * Hooks some engine console commands.
	 */
	void PatchCommandList(bool enable);

	/**
	 * Empty motd_write handler to prevent slowhacking.
	 */
	static void MotdWriteHandler();

	/**
	 * Changes process affinity to not use first CPU core.
	 */
	void SetAffinity();

	/**
	 * Stops stale server browser threads to avoid crash on engine reload.
	 */
	void StopServerBrowserThreads();

	//-------------------------------------------------------------------
	// Utility functions
	//-------------------------------------------------------------------
	/**
	 * Replaces double word on specified address with new dword, returns old dword.
	 */
	static uint32_t HookDWord(uintptr_t origAddr, uint32_t newDWord);

	/**
	 * Exchanges bytes between memory address and bytes array
	 */
	static void ExchangeMemoryBytes(uintptr_t *origAddr, uintptr_t *dataAddr, uint32_t size);

	friend BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
};

// NtQueryInformationThread is an internal WinAPI function and is not available in winternl.h.
typedef NTSTATUS NTAPI NtQueryInformationThreadProto(
    HANDLE ThreadHandle,
    THREADINFOCLASS ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength);
#define ThreadQuerySetWin32StartAddress 9

static CEnginePatchesWindows s_EnginePatchesInstance;

static ConVar engine_fix_fpsbug("engine_fix_fpsbug", "1", FCVAR_BHL_ARCHIVE);

//-------------------------------------------------------------------
// Module
//-------------------------------------------------------------------
bool CEnginePatchesWindows::Module::ReadModule(const char *moduleName)
{
	HANDLE hProcess = GetCurrentProcess();
	HMODULE hModuleDll = GetModuleHandle(moduleName);
	if (!hProcess || !hModuleDll)
		return false;
	MODULEINFO moduleInfo;
	GetModuleInformation(hProcess, hModuleDll, &moduleInfo, sizeof(moduleInfo));
	iBase = (uintptr_t)moduleInfo.lpBaseOfDll;
	iEnd = (uintptr_t)moduleInfo.lpBaseOfDll + (size_t)moduleInfo.SizeOfImage - 1;
	return true;
}

//-------------------------------------------------------------------
// CEnginePatchesWindows
//-------------------------------------------------------------------
void CEnginePatchesWindows::RunFrame()
{
	CEnginePatches::RunFrame();

	if (!m_ServerBrowserModule.IsValid())
	{
		m_ServerBrowserModule.ReadModule("ServerBrowser.dll");
	}
}

void CEnginePatchesWindows::PlatformPatchesInit()
{
	if (!GetEngineModule())
	{
		ConPrintf(ConColor::Red, "Engine patch: Failed to get engine base address.\n");
		return;
	}

	// Check for SDL2.dll
	{
		HMODULE hSDL = nullptr;
		GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "SDL2.dll", &hSDL);
		if (hSDL)
		{
			m_bIsSDLEngine = true;
			gEngfuncs.Con_DPrintf("Engine patch: SteamPipe (SDL2) engine\n");
		}
		else
		{
			m_bIsSDLEngine = false;
			gEngfuncs.Con_DPrintf("Engine patch: pre-SteamPipe engine\n");
		}
	}

	PatchFPSBug(true);
	PatchConnectionlessPacketHandler(true);
	SetAffinity();
}

void CEnginePatchesWindows::PlatformPatchesShutdown()
{
	PatchFPSBug(false);
	PatchConnectionlessPacketHandler(false);
	PatchCommandList(false);
}

void CEnginePatchesWindows::PlatformLatePatching()
{
	PatchCommandList(true);
}

void CEnginePatchesWindows::FindMsgBuf()
{
	// Find and get engine messages buffer variables in READ_CHAR function
	const char data1[] = "A1283DCD02 8B1530E67602 8D4801 3BCA 7E0E C7052C3DCD0201000000 83C8FFC3 8B1528E67602 890D283DCD02";
	const char mask1[] = "FF00000000 FFFF00000000 FFFFFF FFFF FF00 FFFF0000000000000000 FFFFFFFF FFFF00000000 FFFF00000000";
	size_t addr1 = MemoryFindForward(m_EngineModule.iBase, m_EngineModule.iEnd, data1, mask1);
	if (!addr1)
	{
		ConPrintf(ConColor::Red, "Engine patch: offsets of EngineBuffer variables not found.\n");
		return;
	}

	// We have two references of EngineReadPos, compare them
	if ((int *)*(size_t *)(addr1 + 1) != (int *)*(size_t *)(addr1 + 40))
	{
		ConPrintf(ConColor::Red, "Engine patch: offsets of EngineReadPos didn't match.\n");
		return;
	}

	// Pointers to buffer, its size and current buffer read position
	m_MsgBuf.m_EngineBuf = (void **)*(size_t *)(addr1 + 34);
	m_MsgBuf.m_EngineBufSize = (int *)*(size_t *)(addr1 + 7);
	m_MsgBuf.m_EngineReadPos = (int *)*(size_t *)(addr1 + 1);
}

void CEnginePatchesWindows::FindSvcArray()
{
	// Search for "svc_bad" and futher engine messages strings
	uintptr_t svc_bad, svc_nop, svc_disconnect;

	const unsigned char data1[] = "svc_bad";
	svc_bad = MemoryFindForward(m_EngineModule.iBase, m_EngineModule.iEnd, data1, NULL, sizeof(data1) - 1);
	if (!svc_bad)
	{
		ConPrintf(ConColor::Red, "Engine patch: offset of svc_bad not found.\n");
		return;
	}

	const unsigned char data2[] = "svc_nop";
	svc_nop = MemoryFindForward(svc_bad, m_EngineModule.iEnd, data2, NULL, sizeof(data2) - 1);
	if (!svc_nop)
	{
		ConPrintf(ConColor::Red, "Engine patch: offset of svc_nop not found.\n");
		return;
	}

	const unsigned char data3[] = "svc_disconnect";
	svc_disconnect = MemoryFindForward(svc_nop, m_EngineModule.iEnd, data3, NULL, sizeof(data3) - 1);
	if (!svc_disconnect)
	{
		ConPrintf(ConColor::Red, "Engine patch: offset of svc_disconnect not found.\n");
		return;
	}

	// Form a pattern to search for engine messages functions table
	unsigned char data4[12 * 3 + 4];
	memset(data4, 0, sizeof(data4));
	*((uint32_t *)data4 + 0) = 0;
	*((uint32_t *)data4 + 1) = svc_bad;
	*((uint32_t *)data4 + 3) = 1;
	*((uint32_t *)data4 + 4) = svc_nop;
	*((uint32_t *)data4 + 6) = 2;
	*((uint32_t *)data4 + 7) = svc_disconnect;
	*((uint32_t *)data4 + 9) = 3;
	const char mask4[] = "FFFFFFFFFFFFFFFF00000000 FFFFFFFFFFFFFFFF00000000 FFFFFFFFFFFFFFFF00000000 FFFFFFFF";
	unsigned char m[MAX_PATTERN];
	ConvertHexString(mask4, m, sizeof(m));

	// We search backward first - table should be there and near
	uintptr_t tableAddr = MemoryFindBackward(svc_bad, m_EngineModule.iBase, data4, m, sizeof(data4) - 1);

	if (!tableAddr)
		tableAddr = MemoryFindForward(svc_bad, m_EngineModule.iEnd, data4, m, sizeof(data4) - 1);

	if (!tableAddr)
	{
		ConPrintf(ConColor::Red, "Engine patch: offset of SvcMessagesTable not found.\n");
		return;
	}

	m_pSvcArray = reinterpret_cast<SvcHandler *>(tableAddr);

	// Check first 3 elements just to be sure
	auto fnCheck = [&](size_t idx, const char *name) {
		return strcmp(m_pSvcArray[idx].pszName, name) == 0;
	};

	if (!fnCheck(0, "svc_bad") || !fnCheck(1, "svc_nop") || !fnCheck(2, "svc_disconnect"))
	{
		m_pSvcArray = nullptr;
		ConPrintf(ConColor::Red, "Engine patch: SvcMessagesTable check has failed.\n");
	}
}

void CEnginePatchesWindows::FindUserMessageList()
{
	// Search for registered user messages chain entry
	const char data1[] = "81FB00010000 0F8D1B010000 8B3574FF6C03 85F6740B";
	const char mask1[] = "FFFFFFFFFFFF FFFF0000FFFF FFFF00000000 FFFFFF00";
	size_t addr1 = MemoryFindForward(m_EngineModule.iBase, m_EngineModule.iEnd, data1, mask1);
	if (addr1)
	{
		m_pUserMsgList = (UserMessage **)*(size_t *)(addr1 + 14);
	}
	else
	{
		ConPrintf(ConColor::Red, "Engine patch: offset of UserMessages entry not found.\n");
	}
}

bool CEnginePatchesWindows::GetEngineModule()
{
	// Try Hardware engine
	if (m_EngineModule.ReadModule("hw.dll"))
		return true;

	// Try Software engine
	if (m_EngineModule.ReadModule("sw.dll"))
		return true;

	// Try Encrypted engine
	if (m_EngineModule.ReadModule("hl.exe"))
		return true;

	// Get process base module name in case it differs from hl.exe
	char moduleName[256];

	if (!GetModuleFileName(nullptr, moduleName, ARRAYSIZE(moduleName)))
		return false;

	char *baseName = strrchr(moduleName, '\\');
	if (baseName == nullptr)
		return false;
	baseName++;

	return m_EngineModule.ReadModule(baseName);
}

void CEnginePatchesWindows::PatchFPSBug(bool enable)
{
	if (enable)
	{
		// Fixed in SteamPipe engine
		if (IsSDLEngine() || m_iFpsBugPlace)
			return;

		// Find a place where FPS bug happens
		const char data1[] = "DD052834FA03 DC0DE8986603 83C408 E8D87A1000 89442424DB442424 DD5C242C DD05";
		const char mask1[] = "FFFF00000000 FFFF00000000 FFFFFF FF00000000 FFFFFFFFFFFFFFFF FFFFFFFF FFFF";
		uintptr_t addr1 = MemoryFindForward(m_EngineModule.iBase, m_EngineModule.iEnd, data1, mask1);

		if (addr1)
		{
			m_iFpsBugPlace = addr1;
			m_pflFrameTime = (double *)*(size_t *)(addr1 + 2);

			// Patch FPS bug: inject correction function
			const char data2[] = "8D542424 52 50 E8FFFFFFFF 90";
			ConvertHexString(data2, m_FpsBugPlaceBackup, sizeof(m_FpsBugPlaceBackup));
			size_t offset = (size_t)FpsBugFix - (m_iFpsBugPlace + 20 + 11);
			*(size_t *)(&(m_FpsBugPlaceBackup[7])) = offset;
			ExchangeMemoryBytes((size_t *)(m_iFpsBugPlace + 20), (size_t *)m_FpsBugPlaceBackup, 12);
		}
		else
		{
			ConPrintf(ConColor::Red, "Engine patch: offset of FPS bug place not found.\n");
		}
	}
	else
	{
		if (!m_iFpsBugPlace)
			return;

		// Restore FPS engine block
		ExchangeMemoryBytes((size_t *)(m_iFpsBugPlace + 20), (size_t *)m_FpsBugPlaceBackup, 12);
		m_iFpsBugPlace = 0;
	}
}

void __stdcall CEnginePatchesWindows::FpsBugFix(int a1, int64_t *a2)
{
	if (engine_fix_fpsbug.GetBool())
	{
		// Collect the reminder and use it when it is over 1
		s_EnginePatchesInstance.m_flFrameTimeReminder += *s_EnginePatchesInstance.m_pflFrameTime * 1000 - a1;
		if (s_EnginePatchesInstance.m_flFrameTimeReminder > 1.0)
		{
			s_EnginePatchesInstance.m_flFrameTimeReminder--;
			a1++;
		}
	}

	// Place fixed value on a stack and do actions that our patch had overwritten in original function
	*a2 = a1;
	*((double *)(a2 + 1)) = a1;
}

void CEnginePatchesWindows::PatchConnectionlessPacketHandler(bool enable)
{
	if (enable)
	{
		// Fixed in SteamPipe engine
		if (IsSDLEngine() || m_fnEngineCLPHandler)
			return;

		// Find CL_ConnectionlessPacket call
		const char data6[] = "833AFF 750A E80DF3FFFF E9";
		const char mask6[] = "FFFFFF FFFF FF00000000 FF";
		uintptr_t addr6 = MemoryFindForward(m_EngineModule.iBase, m_EngineModule.iEnd, data6, mask6);

		if (addr6)
		{
			m_iEngineCLPPlace = addr6 + 6;
			size_t offset1 = (size_t)ConnectionlessPacketHandler - (m_iEngineCLPPlace + 4);
			m_iEngineCLPOffset = HookDWord(m_iEngineCLPPlace, offset1);
			m_fnEngineCLPHandler = (ConnectionlessPacketFunc)((m_iEngineCLPPlace + 4) + m_iEngineCLPOffset);
		}
		else
		{
			ConPrintf(ConColor::Red, "Engine patch: offset of CL_ConnectionlessPacket not found.\n");
		}
	}
	else
	{
		if (!m_fnEngineCLPHandler)
			return;

		// Restore CL_ConnectionlessPacket call
		HookDWord(m_iEngineCLPPlace, m_iEngineCLPOffset);
		m_fnEngineCLPHandler = 0;
		m_iEngineCLPPlace = 0;
		m_iEngineCLPOffset = 0;
	}
}

void CEnginePatchesWindows::ConnectionlessPacketHandler()
{
	Assert(s_EnginePatchesInstance.m_fnEngineCLPHandler);
	const CEnginePatches::EngineMsgBuf &msgBuf = s_EnginePatchesInstance.GetMsgBuf();

	if (msgBuf.IsValid())
	{
		BEGIN_READ(msgBuf.GetBuf(), msgBuf.GetSize(), 0); // Zero to emulate RESET_READ
		long ffffffff = READ_LONG();
		char *line = READ_LINE();
		char *s = line;
		// Simulate command parsing
		while (*s && *s <= ' ' && *s != '\n')
			s++;
		if (*s == 'L')
		{
			// Redirect packet received. Sanitize it
			s = (char *)msgBuf.GetBuf();
			s += 5; // skip FFFFFFFF65
			char *end = (char *)msgBuf.GetBuf() + msgBuf.GetSize();
			while (*s && *s != -1 && *s != ';' && *s != '\n' && s < end)
			{
				s++;
			}
			if (*s == ';')
			{
				s++;
			}
			// Blank out any commands after ';'
			while (*s && *s != -1 && *s != '\n' && s < end)
			{
				*s = ' ';
				s++;
			}
		}
	}

	// Call engine handler
	s_EnginePatchesInstance.m_fnEngineCLPHandler();
}

void CEnginePatchesWindows::PatchCommandList(bool enable)
{
	cmd_function_t *pFirst = gEngfuncs.GetFirstCmdFunctionHandle();
	for (cmd_function_t *pItem = pFirst; pItem; pItem = pItem->next)
	{
		if (enable)
		{
			if (!Q_stricmp(pItem->name, "motd_write"))
			{
				m_fnEngineMotdWrite = pItem->function;
				pItem->function = MotdWriteHandler;
			}
		}
		else
		{
			// Function names could be unloaded with modules.
			// Check function pointers instead.
			if (m_fnEngineMotdWrite && pItem->function == MotdWriteHandler)
			{
				pItem->function = m_fnEngineMotdWrite;
				m_fnEngineMotdWrite = nullptr;
			}
		}
	}
}

void CEnginePatchesWindows::MotdWriteHandler()
{
	ConPrintf(ConColor::Red, "Error: motd_write is blocked on the client to prevent slowhacking.\n");
}

void CEnginePatchesWindows::SetAffinity()
{
	HANDLE hProcess = GetCurrentProcess();
	DWORD_PTR processAffinityMask = 0;
	DWORD_PTR systemAffinityMask = 0;

	if (GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask))
	{
		if (processAffinityMask && systemAffinityMask)
		{
			// Find first available CPU on the system
			int i;
			for (i = 0; i < 64; i++)
			{
				if (systemAffinityMask & (1 << i))
					break;
			}

			// Clear first CPU from the mask
			processAffinityMask &= ~(1 << i);

			// Set new mask if there were more than 1 CPU
			if (processAffinityMask)
				SetProcessAffinityMask(hProcess, processAffinityMask);
		}
	}
}

void CEnginePatchesWindows::StopServerBrowserThreads()
{
	if (!m_ServerBrowserModule.IsValid())
		return;

	DWORD dwPID = GetCurrentProcessId();
	HANDLE hTool = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hTool != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 te;
		te.dwSize = sizeof(te);
		if (Thread32First(hTool, &te))
		{
			NtQueryInformationThreadProto *pNtQueryInformationThread;
			HMODULE hNTDLL = LoadLibrary("NTDLL.DLL");
			pNtQueryInformationThread = (NtQueryInformationThreadProto *)GetProcAddress(hNTDLL, "NtQueryInformationThread");

			if (pNtQueryInformationThread)
			{
				do
				{
					if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID) && te.th32OwnerProcessID == dwPID)
					{
						HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
						size_t startAddress = NULL;
						unsigned long size;
						pNtQueryInformationThread(hThread, (THREADINFOCLASS)ThreadQuerySetWin32StartAddress, &startAddress, sizeof(size_t), &size);
						if (startAddress != NULL && startAddress >= m_ServerBrowserModule.iBase && startAddress <= m_ServerBrowserModule.iEnd)
						{
							TerminateThread(hThread, 0);
						}
						CloseHandle(hThread);
					}
					te.dwSize = sizeof(te);
				} while (Thread32Next(hTool, &te));
			}

			FreeLibrary(hNTDLL);
		}
		CloseHandle(hTool);
	}
}

//-------------------------------------------------------------------
// Utilities
//-------------------------------------------------------------------
uint32_t CEnginePatchesWindows::HookDWord(uintptr_t origAddr, uint32_t newDWord)
{
	DWORD oldProtect;
	uint32_t origDWord = *(size_t *)origAddr;
	VirtualProtect((size_t *)origAddr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
	*(size_t *)origAddr = newDWord;
	VirtualProtect((size_t *)origAddr, 4, oldProtect, &oldProtect);
	return origDWord;
}

void CEnginePatchesWindows::ExchangeMemoryBytes(uintptr_t *origAddr, size_t *dataAddr, uint32_t size)
{
	DWORD oldProtect;
	VirtualProtect(origAddr, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	unsigned char data[MAX_PATTERN];
	int32_t iSize = size;
	while (iSize > 0)
	{
		size_t s = iSize <= MAX_PATTERN ? iSize : MAX_PATTERN;
		memcpy(data, origAddr, s);
		memcpy((void *)origAddr, (void *)dataAddr, s);
		memcpy((void *)dataAddr, data, s);
		iSize -= MAX_PATTERN;
	}
	VirtualProtect(origAddr, size, oldProtect, &oldProtect);
}

//-------------------------------------------------------------------
// DLL Entry Point
//-------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		s_EnginePatchesInstance.StopServerBrowserThreads();
	}

	return TRUE;
}
