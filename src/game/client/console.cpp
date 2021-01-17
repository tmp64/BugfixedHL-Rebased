#define TIER2_GAMEUI_INTERNALS
#include <cstdarg>
#include <queue>
#include <tier0/dbg.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <IGameConsole.h>
#include <convar.h>

#include "hud.h"
#include "console.h"

namespace console
{

// Ambiguity between ::Color (Source SDK) and vgui::Color (VGUI1)
using ::Color;

#ifdef PLATFORM_WINDOWS
constexpr size_t PRINT_COLOR_OFFSET = 0x128;
constexpr size_t DPRINT_COLOR_OFFSET = 0x12C;
#else
constexpr size_t PRINT_COLOR_OFFSET = 0x124;
constexpr size_t DPRINT_COLOR_OFFSET = 0x128;
#endif

class CGameConsolePrototype : public IGameConsole
{
public:
	bool m_bInitialized;
	void *m_pConsole;
};

//-----------------------------------------------------
// Console color hook
//-----------------------------------------------------
// Default color for checking that offset is correct
static const Color s_DefaultColor = Color(216, 222, 211, 255);
static const Color s_DefaultDColor = Color(196, 181, 80, 255);

// Used when failed to find color in GameUI
static Color s_StubColor = s_DefaultColor;
static Color s_StubDColor = s_DefaultDColor;

// Pointers to console color in GameUI
static Color *s_ConColor = &s_StubColor;
static Color *s_ConDColor = &s_StubDColor;

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
	// Pointers to GameUI are no longer valid since VGUI2 has been shutdown
	s_ConColor = &s_StubColor;
	s_ConDColor = &s_StubDColor;
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
	*s_ConColor = s_DefaultColor;
	*s_ConDColor = s_DefaultDColor;
}

//-----------------------------------------------------
// Console color hook
//-----------------------------------------------------
void console::HookConsoleColor()
{
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

	auto fnHookSpecificColor = [&](size_t offset, Color compare, Color *&target) -> bool {
		size_t iColorPtr = reinterpret_cast<size_t>(pGameConsole->m_pConsole) + offset;
		Color *pColor = reinterpret_cast<Color *>(iColorPtr);

		if (*pColor != compare)
		{
			ConPrintf(ConColor::Red,
			    "HookConsoleColor: check failed.\n"
			    "  Expected: %d %d %d %d\n"
			    "  Got: %d %d %d %d.\n",
			    compare.r(), compare.g(), compare.b(), compare.a(),
			    pColor->r(), pColor->g(), pColor->b(), pColor->a());
			return false;
		}

		target = pColor;
		return true;
	};

	if (fnHookSpecificColor(PRINT_COLOR_OFFSET, s_DefaultColor, s_ConColor) && fnHookSpecificColor(DPRINT_COLOR_OFFSET, s_DefaultDColor, s_ConDColor))
	{
#ifdef _DEBUG
		ConPrintf(ConColor::Cyan, "HookConsoleColor: Success!\n");
#endif
	}
	else
	{
		ConPrintf(ConColor::Red, "Failed to hook console color.\n");
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
	if (*s_ConColor == s_DefaultColor)
		*s_ConDColor = s_DefaultColor;

	va_list args;
	va_start(args, pszFormat);

	static char buf[1024];
	vsnprintf(buf, sizeof(buf), pszFormat, args);
	s_fnEngineDPrintf("%s", buf);

	va_end(args);

	if (*s_ConColor == s_DefaultColor)
		*s_ConDColor = s_DefaultDColor;
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
	struct FindResult
	{
		const char *name;
		cvar_t *cvar;
	};

	if (ConCommand::ArgC() != 2)
	{
		ConPrintf("Searches cvars and commands for a string.\n");
		ConPrintf("Usage: find <string>\n");
		return;
	}

	const char *str_orig = ConCommand::ArgV(1);
	char str[128];
	Q_strncpy(str, str_orig, sizeof(str));
	for (char *c = str; *c; c++)
		*c = tolower(*c);

	std::vector<FindResult> found;

	// Iterate all cvars
	{
		char buf[128];
		cvar_t *item = gEngfuncs.GetFirstCvarPtr();
		for (; item; item = item->next)
		{
			Q_strncpy(buf, item->name, sizeof(buf));
			for (char *c = buf; *c; c++)
				*c = tolower(*c);
			if (strstr(buf, str))
				found.push_back(FindResult { item->name, item });
		}
	}

	// Iterate all commands
	{
		char buf[128];
		cmd_function_t *item = gEngfuncs.GetFirstCmdFunctionHandle();
		for (; item; item = item->next)
		{
			Q_strncpy(buf, item->name, sizeof(buf));
			for (char *c = buf; *c; c++)
				*c = tolower(*c);
			if (strstr(buf, str))
				found.push_back(FindResult { item->name, nullptr });
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
			ConVar *cv = CvarSystem::FindCvar(i.cvar);

			ConPrintf("%s = \"%s\"", i.name, i.cvar->string);

			if (cv)
			{
				ConPrintf(" (def. \"%s\")\n", cv->GetDefaultValue());
				ConPrintf("        %s\n", cv->GetDescription());
			}
			else
			{
				ConPrintf("\n");
			}
		}
		else
		{
			ConCommand *cv = static_cast<ConCommand *>(CvarSystem::FindItem(i.name));

			ConPrintf("%s (command)\n", i.name);

			if (cv)
			{
				ConPrintf("        %s\n", cv->GetDescription());
			}
		}
	}
}
