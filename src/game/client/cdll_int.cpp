/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include <cstring>

#if HAS_STD_FILESYSTEM
#include <filesystem>
#endif

#include <tier1/interface.h>
#include <cl_dll/IGameClientExports.h>
#include <pm_shared.h>
#include "hud.h"
#include "cl_util.h"
#include "tri.h"
#include "cl_voice_status.h"
#include "hud/spectator.h"
#include "vgui/client_viewport.h"
#include "client_steam_context.h"
#include "vgui/client_viewport.h"
#include "Exports.h"
#include "engine_patches.h"
#include "svc_messages.h"
#include "sdl_rt.h"
#include "GameStudioModelRenderer.h"
#include "results.h"
#include "opengl.h"
#include "engfuncs.h"
#include "engine_builds.h"

CHud gHUD;

void InitInput(void);
void ShutdownInput();
void EV_HookEvents(void);
void IN_Commands(void);

/**
 * Checks that game is launched with working directory set to engine path.
 */
void CheckWorkingDirectory()
{
#if HAS_STD_FILESYSTEM
	namespace fs = std::filesystem;

	try
	{
		char buf[MAX_PATH];
		gEngfuncs.COM_ExpandFilename("liblist.gam", buf, sizeof(buf));
		fs::path liblistPath = fs::u8path(buf);

		if (liblistPath.empty())
			throw std::logic_error("liblist.gam not found: " + liblistPath.u8string());

		if (!liblistPath.has_relative_path())
			throw std::logic_error("liblist.gam has no parent path: " + liblistPath.u8string());

		fs::path enginePath = liblistPath.parent_path().parent_path();
		if (enginePath.empty())
			throw std::logic_error("enginePath is empty");

		enginePath = fs::absolute(enginePath.lexically_normal());
		fs::path workdir = fs::absolute(fs::current_path().lexically_normal());

		if (!fs::equivalent(enginePath, workdir))
		{
			char buf[1024];
			snprintf(buf, sizeof(buf),
			    "Game was launched with incorrect working directory.\n\n"
			    "Engine directory:\n%s\n\n"
			    "Working directory:\n%s\n\n"
			    "Please restart the game with correct working directory.\n",
			    enginePath.u8string().c_str(), workdir.u8string().c_str());

			GetSDL()->ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Invalid launch parameters", buf);
			exit(-1);
		}
	}
	catch (const std::exception &e)
	{
		char buf[1024];
		snprintf(buf, sizeof(buf),
		    "An unexpected error has occured while checking working directory.\n\n"
		    "%s\n\n"
		    "Please report this error on GitHub.\n"
		    "Game will continue launching but may work incorrectly.",
		    e.what());

		GetSDL()->ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Internal error", buf);
	}
#endif
}

//! Checks that the game is not running on an unsupported game build.
//! @returns Whether the build is supported.
static bool CheckForInvalidBuilds(int buildNumber)
{
	constexpr char TITLE[] = "BugfixedHL - Unsupported game";

	if (buildNumber < ENGINE_BUILD_P48)
	{
		GetSDL()->ShowSimpleMessageBox(
		    SDL_MESSAGEBOX_ERROR, TITLE,
			"Your game is too old. Please, update to the latest Steam version.");
		return false;
	}

	if (buildNumber >= ENGINE_BUILD_ANNIVERSARY_FIRST && buildNumber < ENGINE_BUILD_ANNIVERSARY_FIXED_INTERFACES)
	{
		GetSDL()->ShowSimpleMessageBox(
		    SDL_MESSAGEBOX_ERROR, TITLE,
			"You are playing one of the first Half-Life 25th Anniversary Update builds.\n"
		    "This build broke compatibility with most mods and is not supported.\n"
			"Please, update to the latest Steam version.");
		return false;
	}

	return true;
}

/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int CL_DLLEXPORT HUD_GetHullBounds(int hullnumber, float *mins, float *maxs)
{
	//	RecClGetHullBounds(hullnumber, mins, maxs);

	int iret = 0;

	Vector &vecMins = *reinterpret_cast<Vector *>(mins);
	Vector &vecMaxs = *reinterpret_cast<Vector *>(maxs);

	switch (hullnumber)
	{
	case 0: // Normal player
		vecMins = Vector(-16, -16, -36);
		vecMaxs = Vector(16, 16, 36);
		iret = 1;
		break;
	case 1: // Crouched player
		vecMins = Vector(-16, -16, -18);
		vecMaxs = Vector(16, 16, 18);
		iret = 1;
		break;
	case 2: // Point based hull
		vecMins = Vector(0, 0, 0);
		vecMaxs = Vector(0, 0, 0);
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int CL_DLLEXPORT HUD_ConnectionlessPacket(const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size)
{
	//	RecClConnectionlessPacket(net_from, args, response_buffer, response_buffer_size);

	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void CL_DLLEXPORT HUD_PlayerMoveInit(struct playermove_s *ppmove)
{
	//	RecClClientMoveInit(ppmove);

	PM_Init(ppmove);
}

char CL_DLLEXPORT HUD_PlayerMoveTexture(char *name)
{
	//	RecClClientTextureType(name);

	return PM_FindTextureType(name);
}

void CL_DLLEXPORT HUD_PlayerMove(struct playermove_s *ppmove, int server)
{
	//	RecClClientMove(ppmove, server);

	PM_Move(ppmove, server);
}

int CL_DLLEXPORT Initialize(cl_enginefunc_t *pEnginefuncs, int iVersion)
{
	// Initialize must only be called once, another call requires the DLL to be reloaded.
	// Sometimes the DLL isn't unloaded, in that case this code crashes the game.
	static bool bIsInitialized = false;

	if (bIsInitialized)
	{
		HUD_FatalError("The engine failed to unload client library during restart.");
		return 0;
	}

	bIsInitialized = true;

	// Check the interface version
	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	EngFuncs_Init(pEnginefuncs);

	// Save engine version before everything els
	// (in case the game crashes there, crash handler will know the engine version)
	gHUD.SaveEngineVersion();
	
	if (!CheckForInvalidBuilds(gHUD.GetEngineBuild()))
		return 0;

	console::Initialize();
	CvarSystem::RegisterCvars();
	EV_HookEvents();
	GetSDL()->Init();
	CheckWorkingDirectory();

	// Note 10.07.2020
	// There is something odd with IParticleMan on Linux.
	// SetVariables in the interface takes `Vector vViewAngles` (as a copy).
	// Reversing Linux binary shows that it takes a pointer (and it crashes there on Linux).
	//
	// Changing `Vector vViewAngles` to `Vector &vViewAngles` fixes the crash on Linux but causes a crash on Windows.
	// So I just disabled it completely since it isn't used in vanilla HL.
	//
	// CL_LoadParticleMan();

	return 1;
}

/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

int CL_DLLEXPORT HUD_VidInit(void)
{
	//	RecClHudVidInit();
	g_pViewport->VidInit();
	gHUD.VidInit();
	g_StudioRenderer.InitOnConnect();
	PM_ResetBHopDetection();
	PM_ResetUseSlowDownDetection();
	CResults::Get().Stop();
	GetClientVoiceMgr()->VidInit();

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all 
the hud variables.
==========================
*/

void CL_DLLEXPORT HUD_Init(void)
{
	//	RecClHudInit();
	console::HudInit();
	CEnginePatches::Get().Init();
	InitInput();
	ClientSteamContext().Activate();
	CClientOpenGL::Get().Init();
	gHUD.Init();
	CSvcMessages::Get().Init();
	EngFuncs_UpdateHooks();
	console::HudPostInit();
}

/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int CL_DLLEXPORT HUD_Redraw(float time, int intermission)
{
	//	RecClHudRedraw(time, intermission);

	gHUD.Redraw(time, intermission);

	return 1;
}

/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int CL_DLLEXPORT HUD_UpdateClientData(client_data_t *pcldata, float flTime)
{
	//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();

	return gHUD.UpdateClientData(pcldata, flTime);
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void CL_DLLEXPORT HUD_Reset(void)
{
	//	RecClHudReset();

	gHUD.VidInit();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

void CL_DLLEXPORT HUD_Frame(double time)
{
	//	RecClHudFrame(time);

	CEnginePatches::Get().RunFrame();
	EngFuncs_UpdateHooks();
	gHUD.Frame(time);
	GetClientVoiceMgr()->Frame(time);
}

/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void CL_DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	////	RecClVoiceStatus(entindex, bTalking);

	GetClientVoiceMgr()->UpdateSpeakerStatus(entindex, bTalking);
}

/*
==========================
HUD_DirectorMessage

Called when a director event message was received
==========================
*/

void CL_DLLEXPORT HUD_DirectorMessage(int iSize, void *pbuf)
{
	//	RecClDirectorMessage(iSize, pbuf);

	CHudSpectator::Get()->DirectorMessage(iSize, pbuf);
}

/*
==========================
HUD_ChatInputPosition

Sets the location of the input for chat text
==========================
*/

void CL_DLLEXPORT HUD_ChatInputPosition(int *x, int *y)
{
	// Do nothing
}

//---------------------------------------------------
// Client shutdown
//---------------------------------------------------
void CL_DLLEXPORT HUD_Shutdown(void)
{
	//	RecClShutdown();

	console::HudShutdown();
	gHUD.Shutdown();
	ShutdownInput();
	CL_UnloadParticleMan();
	ClientSteamContext().Shutdown();
	CClientOpenGL::Get().Shutdown();
	CEnginePatches::Get().Shutdown();
	console::HudPostShutdown();

#if defined(PLATFORM_WINDOWS) && defined(_DEBUG)
	void Win_ValidateHeap(const std::function<void(const char *)> &logfn);
	Win_ValidateHeap([](const char *) {});
#endif
}

extern "C" CL_DLLEXPORT void *ClientFactory()
{
	return (void *)(Sys_GetFactoryThis());
}

extern "C" void CL_DLLEXPORT F(void *pv)
{
	cldll_func_t *pcldll_func = (cldll_func_t *)pv;

	cldll_func_t cldll_func = {
		Initialize,
		HUD_Init,
		HUD_VidInit,
		HUD_Redraw,
		HUD_UpdateClientData,
		HUD_Reset,
		HUD_PlayerMove,
		HUD_PlayerMoveInit,
		HUD_PlayerMoveTexture,
		IN_ActivateMouse,
		IN_DeactivateMouse,
		IN_MouseEvent,
		IN_ClearStates,
		IN_Accumulate,
		CL_CreateMove,
		CL_IsThirdPerson,
		CL_CameraOffset,
		KB_Find,
		CAM_Think,
		V_CalcRefdef,
		HUD_AddEntity,
		HUD_CreateEntities,
		HUD_DrawNormalTriangles,
		HUD_DrawTransparentTriangles,
		HUD_StudioEvent,
		HUD_PostRunCmd,
		HUD_Shutdown,
		HUD_TxferLocalOverrides,
		HUD_ProcessPlayerState,
		HUD_TxferPredictionData,
		Demo_ReadBuffer,
		HUD_ConnectionlessPacket,
		HUD_GetHullBounds,
		HUD_Frame,
		HUD_Key_Event,
		HUD_TempEntUpdate,
		HUD_GetUserEntity,
		HUD_VoiceStatus,
		HUD_DirectorMessage,
		HUD_GetStudioModelInterface,
		HUD_ChatInputPosition,
		nullptr, // pGetPlayerTeam
		ClientFactory
	};

	*pcldll_func = cldll_func;
}

void HUD_FatalError(const char *fmt, ...)
{
	char buf[4096];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	GetSDL()->ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "BugfixedHL Fatal Error", buf);
	std::abort();
}

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	virtual const char *GetServerHostName()
	{
		if (g_pViewport)
		{
			return g_pViewport->GetServerName();
		}
		return "";
	}

	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex)
	{
		if (GetClientVoiceMgr())
			return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
		return false;
	}

	virtual void MutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, true);
		}
	}

	virtual void UnmutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, false);
		}
	}
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION);
