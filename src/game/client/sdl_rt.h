#ifndef SDL_RT_H
#define SDL_RT_H
#include "SDL2/SDL.h"

/**
 * Class for dynamic linking with SDL2 library in runtime.
 * Used to support both new and pre-SteamPipe engine.
 */
class CSDLRuntime
{
public:
	decltype(SDL_GetRelativeMouseState) *GetRelativeMouseState = nullptr;
	decltype(SDL_SetRelativeMouseMode) *SetRelativeMouseMode = nullptr;
	decltype(SDL_NumJoysticks) *NumJoysticks = nullptr;
	decltype(SDL_IsGameController) *IsGameController = nullptr;
	decltype(SDL_GameControllerOpen) *GameControllerOpen = nullptr;
	decltype(SDL_GameControllerClose) *GameControllerClose = nullptr;
	decltype(SDL_GameControllerName) *GameControllerName = nullptr;
	decltype(SDL_GameControllerGetAxis) *GameControllerGetAxis = nullptr;
	decltype(SDL_GameControllerGetButton) *GameControllerGetButton = nullptr;
	decltype(SDL_JoystickUpdate) *JoystickUpdate = nullptr;
	decltype(SDL_GL_GetProcAddress) *GL_GetProcAddress = nullptr;

	/**
	 * Custom ShowSimpleMessageBox, works on Windows without SDL.
	 * @param	flags	An SDL_MessageBoxFlag that selects dialog type
	 * @param	title	Title of the dialog, UTF-8 formatted
	 * @param	message	Message text, UTF-8 formatted
	 */
	void ShowSimpleMessageBox(Uint32 flags, const char *title, const char *message);

	/**
	 * Sets SDL_ function pointers.
	 */
	void Init();

	/**
	 * Returns true if all function pointers are valid.
	 */
	bool IsGood();

private:
	bool m_bIsGood = false;

	/**
	 * Sets pointers in runtime if SDL2.dll is loaded.
	 */
	void InitWindows();

	/**
	 * Sets pointers with SDL_ functions.
	 * On non-windows platforms client is linked with libSDL2.
	 */
	void InitOther();
};

CSDLRuntime *GetSDL();

#endif
