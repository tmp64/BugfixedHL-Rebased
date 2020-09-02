#ifndef TESTS_CLIENT_CL_EXPORTS_H
#define TESTS_CLIENT_CL_EXPORTS_H
#include "../../game/client/wrect.h"
#include "../../game/client/cl_dll.h"
#include <APIProxy.h>

static_assert(sizeof(cldll_func_t) % sizeof(void *) == 0, "cldll_func_t is not divisible by pointer size");
constexpr size_t CLIENT_EXPORTED_FUNC_COUNT = sizeof(cldll_func_t) / sizeof(void *);

union ClientExports
{
	cldll_func_t funcs;
	void *array[CLIENT_EXPORTED_FUNC_COUNT];
};

/**
 * List of client exported function names.
 * nullptr entries should be ignored.
 */
constexpr const char *CLIENT_FUNC_NAMES[] = {
	"Initialize",
	"HUD_Init",
	"HUD_VidInit",
	"HUD_Redraw",
	"HUD_UpdateClientData",
	"HUD_Reset",
	"HUD_PlayerMove",
	"HUD_PlayerMoveInit",
	"HUD_PlayerMoveTexture",
	"IN_ActivateMouse",
	"IN_DeactivateMouse",
	"IN_MouseEvent",
	"IN_ClearStates",
	"IN_Accumulate",
	"CL_CreateMove",
	"CL_IsThirdPerson",
	"CL_CameraOffset",
	"KB_Find",
	"CAM_Think",
	"V_CalcRefdef",
	"HUD_AddEntity",
	"HUD_CreateEntities",
	"HUD_DrawNormalTriangles",
	"HUD_DrawTransparentTriangles",
	"HUD_StudioEvent",
	"HUD_PostRunCmd",
	"HUD_Shutdown",
	"HUD_TxferLocalOverrides",
	"HUD_ProcessPlayerState",
	"HUD_TxferPredictionData",
	"Demo_ReadBuffer",
	"HUD_ConnectionlessPacket",
	"HUD_GetHullBounds",
	"HUD_Frame",
	"HUD_Key_Event",
	"HUD_TempEntUpdate",
	"HUD_GetUserEntity",
	"HUD_VoiceStatus",
	"HUD_DirectorMessage",
	"HUD_GetStudioModelInterface",
	"HUD_ChatInputPosition",
	nullptr, // HUD_GetPlayerTeam - optional, not exported, seems to be unused by the engine
	"ClientFactory"
};

#endif
