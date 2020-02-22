#define TIER2_GAMEUI_INTERNALS
#include <cstdarg>
#include <tier2/tier2.h>
#include <IGameConsole.h>

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
}

void console::Initialize()
{
}

void console::HudInit()
{
	HookConsoleColor();
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
