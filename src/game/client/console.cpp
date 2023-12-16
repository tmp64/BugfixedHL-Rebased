#define TIER2_GAMEUI_INTERNALS
#include <cstdarg>
#include <queue>
#include <FileSystem.h>
#include <tier0/dbg.h>
#include <tier1/interface.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/IScheme.h>
#include <IGameConsole.h>
#include <convar.h>

#include "hud.h"
#include "console.h"

namespace console
{

// Ambiguity between ::Color (Source SDK) and vgui::Color (VGUI1)
using ::Color;

struct ColorOffset
{
	size_t printOffset;
	size_t dprintOffset;
};

constexpr ColorOffset COLOR_OFFSETS[] = {
#ifdef PLATFORM_WINDOWS
	ColorOffset { 0x128, 0x12C }, // Pre-9884
	ColorOffset { 0x12C, 0x130 }, // 9884
	ColorOffset { 0x130, 0x134 }, // 9891
#else
	ColorOffset { 0x124, 0x128 }, // Pre-9884
	ColorOffset { 0x128, 0x12C }, // 9884
	ColorOffset { 0x12C, 0x130 }, // 9891
#endif
};

class CGameConsolePrototype : public IGameConsole
{
public:
	bool m_bInitialized;
	void *m_pConsole;
};

//-----------------------------------------------------
// GameUI module
// Engine unloads GameUI before HUD_Shutdown.
// If a tier0 spew occurs between CBaseUI::Shutdown and HUD_Shutdown
// SetColor will access invalid memory (pointing to unloaded module).
// We prevent this by keeping it loaded until Hud_Shutdown.
//-----------------------------------------------------
CSysModule *s_hGameUI = nullptr;

static void LoadGameUI();
static void UnloadGameUI();

//-----------------------------------------------------
// Console color hook
//-----------------------------------------------------
// Default color if scheme fails to load
static const Color s_DefaultColor = Color(216, 222, 211, 255);
static const Color s_DefaultDColor = Color(196, 181, 80, 255);

// Color for checking that offset is correct
static Color s_ExpectedColor;
static Color s_ExpectedDColor;

// Used when failed to find color in GameUI
static Color s_StubColor = s_DefaultColor;
static Color s_StubDColor = s_DefaultDColor;

// Pointers to console color in GameUI
static Color *s_ConColor = &s_StubColor;
static Color *s_ConDColor = &s_StubDColor;

static void ReadSchemeColors();
static void HookConsoleColor();

//-----------------------------------------------------
// Early console
// Console is not yet initialized when client's Initialize()
// and IClientVGUI::Initialize() are called.
//
// EarlyConPrintf saves the messages and DumpEarlyCon prints
// them when the console is ready.
//-----------------------------------------------------
constexpr size_t EARLY_CON_BUFFER_SIZE = 512;

struct EarlyConItem
{
	char *buf;
	Color color;
};

using ConPrintfFn = void (*)(const char *fmt, ...);

static ConPrintfFn s_fnEnginePrintf = nullptr;
static ConPrintfFn s_fnEngineDPrintf = nullptr;
static std::queue<EarlyConItem> s_EarlyConQueue;

static void EnableEarlyCon();
static void EarlyConPrintf(const char *fmt, ...);
static void EarlyConDPrintf(const char *fmt, ...);
static void DisableEarlyCon();
static void DumpEarlyCon();

//-----------------------------------------------------
// Console redirection
// Con_Printf doesn't work until after HUD_Init finishes.
// EnableRedirection redirects it to Con_DPrintf, which
// does work.
//-----------------------------------------------------
static void RedirectedConPrintf(const char *pszFormat, ...);
static void EnableRedirection();
static void DisableRedirection();

//-----------------------------------------------------
// tier0 spew output
// By default tier0 spew output goes in stdout but
// stdout is closed in GoldSrc.
//-----------------------------------------------------
static SpewOutputFunc_t s_fnDefaultSpewOutput = nullptr;
static void EnableSpewOutputFunc();
static void DisableSpewOutputFunc();

// True if launched with -dev (Con_DPrintf works).
static bool s_bIsDev = false;
}

void console::Initialize()
{
	s_fnEnginePrintf = gEngfuncs.Con_Printf;
	s_fnEngineDPrintf = gEngfuncs.Con_DPrintf;
	EnableEarlyCon();
	EnableSpewOutputFunc();
}

void console::HudInit()
{
	if (gEngfuncs.CheckParm("-dev", nullptr))
		s_bIsDev = true;

	if (s_bIsDev)
	{
		// Only enable redirection in developer mode.
		// Otherwise, keep storing messages in a buffer.
		DisableEarlyCon();
		EnableRedirection();
		HookConsoleColor();
		DumpEarlyCon();
	}
	else
	{
		HookConsoleColor();
	}
}

void console::HudPostInit()
{
	if (s_bIsDev)
	{
		DisableRedirection();
	}
	else
	{
		// Console will be available on first frame.
		gHUD.CallOnNextFrame([]() {
			DisableEarlyCon();
			DumpEarlyCon();
		});
	}
}

void console::HudShutdown()
{
	// Pointers to GameUI will no longer be valid after GameUI is unloaded
	s_ConColor = &s_StubColor;
	s_ConDColor = &s_StubDColor;

	UnloadGameUI();
}

void console::HudPostShutdown()
{
	DisableSpewOutputFunc();
}

::Color console::GetColor()
{
	return *s_ConColor;
}

void console::SetColor(::Color c)
{
	*s_ConColor = c;
	*s_ConDColor = c;
}

void console::ResetColor()
{
	*s_ConColor = s_ExpectedColor;
	*s_ConDColor = s_ExpectedDColor;
}

//-----------------------------------------------------
// GameUI
//-----------------------------------------------------
void console::LoadGameUI()
{
#ifdef PLATFORM_WINDOWS
	// Windows loads GameUI from cl_dlls/GameUI.dll using IFileSystem
	char path[MAX_PATH];
	g_pFullFileSystem->GetLocalPath("cl_dlls/GameUI.dll", path, sizeof(path));
#else
	// Linux always loads GameUI from valve/cl_dlls/gameui.so
	const char *path = "valve/cl_dlls/gameui" DLL_EXT_STRING;
#endif

	s_hGameUI = Sys_LoadModule(path);
}

void console::UnloadGameUI()
{
	if (s_hGameUI)
	{
		Sys_UnloadModule(s_hGameUI);
	}
}

//-----------------------------------------------------
// Console color hook
//-----------------------------------------------------
void console::ReadSchemeColors()
{
	// Set the default color (in case scheme fails to load)
	s_ExpectedColor = s_DefaultColor;
	s_ExpectedDColor = s_DefaultDColor;

	constexpr char SCHEME_NAME[] = "BaseUI";

	vgui2::HScheme hScheme = g_pVGuiSchemeManager->GetScheme(SCHEME_NAME);
	if (!hScheme)
	{
		ConPrintf(ConColor::Red, "Console Color: Failed to open scheme %s\n", SCHEME_NAME);
		return;
	}

	vgui2::IScheme* pScheme = g_pVGuiSchemeManager->GetIScheme(hScheme);
	if (!pScheme)
	{
		ConPrintf(ConColor::Red, "Console Color: Failed to get pointer to scheme %s [%lu]\n", SCHEME_NAME, hScheme);
		return;
	}

	s_ExpectedColor = pScheme->GetColor("FgColor", s_DefaultColor);
	s_ExpectedDColor = pScheme->GetColor("BrightControlText", s_DefaultDColor);
}

void console::HookConsoleColor()
{
	LoadGameUI();

	if (!s_hGameUI)
	{
		ConPrintf(ConColor::Red, "HookConsoleColor: GameUI not loaded, color hooking is not safe.\n");
		return;
	}

	CGameConsolePrototype *pGameConsole = (CGameConsolePrototype *)g_pGameConsole;

	if (!pGameConsole)
	{
		ConPrintf(ConColor::Red, "HookConsoleColor: g_pGameConsole is invalid\n");
		return;
	}

	if (!pGameConsole->m_pConsole)
	{
		ConPrintf(ConColor::Red, "HookConsoleColor: console not initialized\n");
		return;
	}

	// Read console colors from the GameUI scheme
	ReadSchemeColors();
	s_StubColor = s_ExpectedColor;
	s_StubDColor = s_ExpectedDColor;

	auto fnTryHookSpecificColor = [&](size_t offset, Color compare, Color **target) -> bool
	{
		size_t iColorPtr = reinterpret_cast<size_t>(pGameConsole->m_pConsole) + offset;
		Color *pColor = reinterpret_cast<Color *>(iColorPtr);

		if (*pColor != compare)
		{
			if (!target)
			{
				ConPrintf(ConColor::Red,
				    "  Offset: 0x%X\n"
				    "    Expected: %d %d %d %d\n"
				    "    Got: %d %d %d %d.\n",
					offset,
				    compare.r(), compare.g(), compare.b(), compare.a(),
				    pColor->r(), pColor->g(), pColor->b(), pColor->a());
			}
			return false;
		}

		if (target)
			*target = pColor;

		return true;
	};

	Color *pPrintColor = nullptr;
	Color *pDPrintColor = nullptr;

	// Try different offsets
	for (const ColorOffset& offset : COLOR_OFFSETS)
	{
		if (fnTryHookSpecificColor(offset.printOffset, s_ExpectedColor, &pPrintColor) &&
			fnTryHookSpecificColor(offset.dprintOffset, s_ExpectedDColor, &pDPrintColor))
		{
			// Found the offset
			break;
		}
	}

	if (pPrintColor && pDPrintColor)
	{
		s_ConColor = pPrintColor;
		s_ConDColor = pDPrintColor;
#ifdef _DEBUG
		ConPrintf(ConColor::Cyan, "HookConsoleColor: Success!\n");
#endif
	}
	else
	{
		ConPrintf(ConColor::Red, "Failed to hook console color.\n");

		// Print offsets and values
		for (const ColorOffset &offset : COLOR_OFFSETS)
		{
			fnTryHookSpecificColor(offset.printOffset, s_ExpectedColor, nullptr);
			fnTryHookSpecificColor(offset.dprintOffset, s_ExpectedDColor, nullptr);
		}
	}
}

//-----------------------------------------------------
// Early console
//-----------------------------------------------------
void console::EnableEarlyCon()
{
	gEngfuncs.Con_Printf = EarlyConPrintf;
	gEngfuncs.Con_DPrintf = EarlyConDPrintf;
}

void console::EarlyConPrintf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char *buf = new char[EARLY_CON_BUFFER_SIZE];
	vsnprintf(buf, EARLY_CON_BUFFER_SIZE, fmt, args);
	s_EarlyConQueue.push({ buf, GetColor() });

	va_end(args);
}

void console::EarlyConDPrintf(const char *fmt, ...)
{
	if (!s_bIsDev)
		return;

	va_list args;
	va_start(args, fmt);

	char *buf = new char[EARLY_CON_BUFFER_SIZE];
	vsnprintf(buf, EARLY_CON_BUFFER_SIZE, fmt, args);
	s_EarlyConQueue.push({ buf, GetColor() });

	va_end(args);
}

void console::DisableEarlyCon()
{
	gEngfuncs.Con_Printf = s_fnEnginePrintf;
	gEngfuncs.Con_DPrintf = s_fnEngineDPrintf;
}

void console::DumpEarlyCon()
{
	while (s_EarlyConQueue.size() > 0)
	{
		EarlyConItem &item = s_EarlyConQueue.front();
		SetColor(item.color);
		gEngfuncs.Con_Printf("%s", item.buf);
		delete[] item.buf;
		s_EarlyConQueue.pop();
	}
	ResetColor();
}

//-----------------------------------------------------
// Console redirection
//-----------------------------------------------------
static void console::RedirectedConPrintf(const char *pszFormat, ...)
{
	// Print redirected messages with Con_Printf color instead of Con_DPrintf
	// But only if color wasn't changed.
	if (*s_ConColor == s_ExpectedColor)
		*s_ConDColor = s_ExpectedColor;

	va_list args;
	va_start(args, pszFormat);

	static char buf[1024];
	vsnprintf(buf, sizeof(buf), pszFormat, args);
	s_fnEngineDPrintf("%s", buf);

	va_end(args);

	if (*s_ConColor == s_ExpectedColor)
		*s_ConDColor = s_ExpectedDColor;
}

void console::EnableRedirection()
{
	gEngfuncs.Con_Printf = RedirectedConPrintf;
}

void console::DisableRedirection()
{
	gEngfuncs.Con_Printf = s_fnEnginePrintf;
}

//-----------------------------------------------------
// tier0 spew output
//-----------------------------------------------------
void console::EnableSpewOutputFunc()
{
	s_fnDefaultSpewOutput = GetSpewOutputFunc();

	SpewOutputFunc([](SpewType_t spewType, tchar const *pMsg) -> SpewRetval_t {
		if (spewType == SPEW_ASSERT)
		{
			ConPrintf(ConColor::Red, "%s", pMsg);
			return SPEW_DEBUGGER;
		}
		else if (spewType == SPEW_ERROR)
			ConPrintf(ConColor::Red, "%s", pMsg);
		else if (spewType == SPEW_WARNING)
			ConPrintf(ConColor::Yellow, "%s", pMsg);
		else
			ConPrintf("%s", pMsg);
		return SPEW_CONTINUE;
	});
}

void console::DisableSpewOutputFunc()
{
	SpewOutputFunc(s_fnDefaultSpewOutput);
}

//-----------------------------------------------------
// Colors for ConPrintf
//-----------------------------------------------------
const Color ConColor::Red = Color(249, 54, 54, 255);
const Color ConColor::Green = Color(77, 219, 83, 255);
const Color ConColor::Yellow = Color(240, 205, 65, 255);
const Color ConColor::Cyan = Color(111, 234, 247, 255);

//-----------------------------------------------------
// Console print
//-----------------------------------------------------
void ConPrintf(const char *fmt, ...)
{
	static char buf[1024];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buf, sizeof(buf), fmt, args);
	gEngfuncs.Con_Printf("%s", buf);

	va_end(args);
}

void ConPrintf(::Color color, const char *fmt, ...)
{
	static char buf[1024];
	va_list args;
	va_start(args, fmt);

	console::SetColor(color);
	vsnprintf(buf, sizeof(buf), fmt, args);
	gEngfuncs.Con_Printf("%s", buf);
	console::ResetColor();

	va_end(args);
}

CON_COMMAND(find, "Searches cvars and commands for a string.")
{
	constexpr const char *CVAR_FLAG_NAMES[32] = {
		"archive", // 0
		"userinfo", // 1
		"server", // 2
		"extdll", // 3
		"client", // 4
		"protected", // 5
		"sponly", // 6
		"printonly", // 7
		"unlogged", // 8
		"noextraws", // 9
		"privileged", // 10
		"filterable", // 11
		nullptr, // 12
		nullptr, // 13
		nullptr, // 14
		nullptr, // 15
		nullptr, // 16
		nullptr, // 17
		nullptr, // 18
		nullptr, // 19
		nullptr, // 20
		nullptr, // 21
		"bhl_archive", // 22
		"devonly", // 23
	};

	constexpr const char *CMD_FLAG_NAMES[32] = {
		"hud",        // 0
		"game",       // 1
		"wrapper",    // 2
		"filtered",   // 3
		"restricted", // 4
	};

	auto fnPrintFlags = [](int flags, const char *const *flagNames)
	{
		bool isFirstPrinted = false;

		for (unsigned i = 0; i < 32; i++)
		{
			unsigned flag = 1 << i;

			if (flags & flag)
			{
				if (isFirstPrinted)
					ConPrintf(" ");

				isFirstPrinted = true;

				if (flagNames[i])
					ConPrintf("%s", flagNames[i]);
				else
					ConPrintf("unknown_%d", i);
			}
		}

		return isFirstPrinted;
	};

	struct FindResult
	{
		const char *name;
		ConItemBase *pItem = nullptr;
		cvar_t *cvar = nullptr;
		cmd_function_t *cmd = nullptr;
	};

	if (ConCommand::ArgC() != 2)
	{
		ConPrintf("Searches cvars and commands for a string.\n");
		ConPrintf("Usage: find <string>\n");
		return;
	}

	const char *str = ConCommand::ArgV(1);
	std::vector<FindResult> found;

	// Iterate all cvars
	{
		cvar_t *item = gEngfuncs.GetFirstCvarPtr();
		for (; item; item = item->next)
		{
			if (!str[0] || Q_stristr(item->name, str))
			{
				// Found in the name of the cvar
				found.push_back(FindResult { item->name, nullptr, item });
			}
			else
			{
				// Check description of the cvar
				ConVar *pCvar = CvarSystem::FindCvar(item);

				if (pCvar && Q_stristr(pCvar->GetDescription(), str))
				{
					found.push_back(FindResult { item->name, pCvar, item });
				}
			}
		}
	}

	// Iterate all commands
	{
		cmd_function_t *item = gEngfuncs.GetFirstCmdFunctionHandle();
		for (; item; item = item->next)
		{
			if (!str[0] || Q_stristr(item->name, str))
			{
				// Found in the name of the command
				found.push_back(FindResult { item->name, nullptr, nullptr, item });
			}
			else
			{
				// Check description of the command
				ConItemBase *pItem = CvarSystem::FindItem(item->name);

				if (pItem && Q_stristr(pItem->GetDescription(), str))
				{
					found.push_back(FindResult { item->name, pItem, nullptr, item });
				}
			}
		}
	}

	// Sort array
	qsort(found.data(), found.size(), sizeof(FindResult), [](const void *i, const void *j) -> int {
		const FindResult *lhs = (const FindResult *)i;
		const FindResult *rhs = (const FindResult *)j;
		return strcmp(lhs->name, rhs->name);
	});

	// Display results
	for (FindResult &i : found)
	{
		if (i.cvar)
		{
			ConVar *cv = i.pItem ? static_cast<ConVar *>(i.pItem) : CvarSystem::FindCvar(i.cvar);

			ConPrintf(ConColor::Yellow, "\"%s\" = \"%s\"", i.name, i.cvar->string);

			if (cv)
				ConPrintf(ConColor::Yellow, " (def. \"%s\")\n", cv->GetDefaultValue());
			else
				ConPrintf("\n");

			if (fnPrintFlags(i.cvar->flags, CVAR_FLAG_NAMES))
				ConPrintf("\n");

			if (cv && cv->GetDescription()[0])
				ConPrintf("- %s\n", cv->GetDescription());
		}
		else
		{
			ConCommand *cv = i.pItem ? static_cast<ConCommand *>(i.pItem) : static_cast<ConCommand *>(CvarSystem::FindItem(i.name));

			ConPrintf(ConColor::Yellow, "\"%s\"\n", i.name);

			if (fnPrintFlags(i.cmd->flags, CMD_FLAG_NAMES))
				ConPrintf("\n");

			if (cv)
				ConPrintf("- %s\n", cv->GetDescription());
		}
	}
}

#ifdef _DEBUG

CON_COMMAND(dump_client_cvars, "Dumps all client convars and concommands to clcvars.csv in Excel format")
{
	FILE *f = fopen("clcvars.csv", "w");

	if (!f)
	{
		ConPrintf("Failed to open clcvars.csv\n");
		return;
	}

	// Iterate all cvars
	{
		cvar_t *item = gEngfuncs.GetFirstCvarPtr();
		for (; item; item = item->next)
		{
			if (!(item->flags & FCVAR_CLIENTDLL))
				continue;

			ConVar *pCvar = CvarSystem::FindCvar(item);

			if (pCvar)
			{
				fprintf(f, "%s;%s;%s\n", pCvar->GetName(), pCvar->GetDefaultValue(), pCvar->GetDescription());
			}
			else
			{
				fprintf(f, "%s;%s;\n", item->name, item->string);
			}
		}
	}

	fprintf(f, ";;\n");

	// Iterate all commands
	{
		cmd_function_t *item = gEngfuncs.GetFirstCmdFunctionHandle();
		for (; item; item = item->next)
		{
			// Client commands have flags = 1
			if (item->flags != 1)
				continue;

			ConCommand *pCvar = static_cast<ConCommand *>(CvarSystem::FindItem(item->name));

			if (pCvar)
			{
				fprintf(f, " %s;%s\n", pCvar->GetName(), pCvar->GetDescription());
			}
			else
			{
				fprintf(f, " %s;\n", item->name);
			}
		}
	}

	fclose(f);
}

#endif
