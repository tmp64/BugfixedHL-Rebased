#include "hud.h"
#include "cl_util.h"
#include "sdl_rt.h"

#ifdef PLATFORM_WINDOWS
#include <winsani_in.h>
#include <Windows.h>
#include <winsani_out.h>
#include <tier1/strtools.h>
#endif

static CSDLRuntime s_SDLRuntime;

void CSDLRuntime::ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message)
{
#ifdef PLATFORM_WINDOWS
	UINT type = MB_OK;

	switch (flags)
	{
	case SDL_MESSAGEBOX_ERROR:
		type |= MB_ICONERROR;
		break;
	case SDL_MESSAGEBOX_WARNING:
		type |= MB_ICONWARNING;
		break;
	case SDL_MESSAGEBOX_INFORMATION:
		type |= MB_ICONINFORMATION;
		break;
	default:
		Assert(false);
		type |= MB_ICONERROR;
		break;
	}

	std::vector<wchar_t> wtitle(256);
	std::vector<wchar_t> wmsg(256);

	Q_UTF8ToWString(title, wtitle.data(), wtitle.size() * sizeof(wchar_t));
	Q_UTF8ToWString(message, wmsg.data(), wmsg.size() * sizeof(wchar_t));

	MessageBoxExW(NULL, wmsg.data(), wtitle.data(), type, 0);
#else
	SDL_ShowSimpleMessageBox(flags, title, message, nullptr);
#endif
}

void CSDLRuntime::Init()
{
	m_bIsGood = false;

#ifdef PLATFORM_WINDOWS
	InitWindows();
#else
	InitOther();
#endif
}

bool CSDLRuntime::IsGood()
{
	return m_bIsGood;
}

void CSDLRuntime::InitWindows()
{
#ifdef PLATFORM_WINDOWS
	HMODULE hSDL = nullptr;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "SDL2.dll", &hSDL);

	if (!hSDL)
		return;

	auto fnLoadSym = [hSDL](auto &var, const char *sym) {
		using T = std::remove_reference<decltype(var)>::type;
		var = reinterpret_cast<T>(GetProcAddress(hSDL, sym));

		if (!var)
		{
			ConPrintf(ConColor::Red, "SDL2: %s not found.\n", sym);
			return false;
		}

		return true;
	};

	m_bIsGood = true;
	m_bIsGood = m_bIsGood && fnLoadSym(GetRelativeMouseState, "SDL_GetRelativeMouseState");
	m_bIsGood = m_bIsGood && fnLoadSym(SetRelativeMouseMode, "SDL_SetRelativeMouseMode");
	m_bIsGood = m_bIsGood && fnLoadSym(NumJoysticks, "SDL_NumJoysticks");
	m_bIsGood = m_bIsGood && fnLoadSym(IsGameController, "SDL_IsGameController");
	m_bIsGood = m_bIsGood && fnLoadSym(GameControllerOpen, "SDL_GameControllerOpen");
	m_bIsGood = m_bIsGood && fnLoadSym(GameControllerClose, "SDL_GameControllerClose");
	m_bIsGood = m_bIsGood && fnLoadSym(GameControllerName, "SDL_GameControllerName");
	m_bIsGood = m_bIsGood && fnLoadSym(GameControllerGetAxis, "SDL_GameControllerGetAxis");
	m_bIsGood = m_bIsGood && fnLoadSym(GameControllerGetButton, "SDL_GameControllerGetButton");
	m_bIsGood = m_bIsGood && fnLoadSym(JoystickUpdate, "SDL_JoystickUpdate");
	m_bIsGood = m_bIsGood && fnLoadSym(GL_GetProcAddress, "SDL_GL_GetProcAddress");

	if (!m_bIsGood)
	{
		ConPrintf(ConColor::Red, "Failed to link with SDL2 in runtime.\n");
	}
#endif
}

void CSDLRuntime::InitOther()
{
#ifndef PLATFORM_WINDOWS
	GetRelativeMouseState = &SDL_GetRelativeMouseState;
	SetRelativeMouseMode = &SDL_SetRelativeMouseMode;
	NumJoysticks = &SDL_NumJoysticks;
	IsGameController = &SDL_IsGameController;
	GameControllerOpen = &SDL_GameControllerOpen;
	GameControllerClose = &SDL_GameControllerClose;
	GameControllerName = &SDL_GameControllerName;
	GameControllerGetAxis = &SDL_GameControllerGetAxis;
	GameControllerGetButton = &SDL_GameControllerGetButton;
	JoystickUpdate = &SDL_JoystickUpdate;
	GL_GetProcAddress = &SDL_GL_GetProcAddress;

	m_bIsGood = true;
#endif
}

CSDLRuntime *GetSDL()
{
	return &s_SDLRuntime;
}
