#include "hud.h"
#include "cl_util.h"
#include "engfuncs.h"
#include "rainbow.h"
#include "hud_renderer.h"

extern ConVar hud_client_renderer;
extern ConVar hud_rainbow;

cl_enginefunc_t gEngfuncs;

static cl_enginefunc_t s_OrigEngfuncs;
static bool s_bRendererHook = false;
static bool s_bRainbowHook = false;

void EngFuncs_Init(cl_enginefunc_t *pEngfuncs)
{
	memcpy(&s_OrigEngfuncs, pEngfuncs, sizeof(cl_enginefunc_t));
	gEngfuncs = s_OrigEngfuncs;
}

void EngFuncs_UpdateHooks()
{
	bool bRendererHook = hud_client_renderer.GetBool();
	bool bRainbowHook = hud_rainbow.GetBool();

	if (bRendererHook == s_bRendererHook && bRainbowHook == s_bRainbowHook)
		return;

	s_bRendererHook = bRendererHook;
	s_bRainbowHook = bRainbowHook;
	gEngfuncs = s_OrigEngfuncs;

	CHudRenderer::Get().HookFuncs();
	gHUD.m_Rainbow.HookFuncs();
}
