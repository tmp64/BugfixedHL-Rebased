/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// hud.cpp
//
// implementation of CHud class
//

#include <cstring>
#include <cstdio>
#include <FileSystem.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Controls.h>

#include <appversion.h>
#include <bhl_urls.h>
#include <ClientSupportsFlags.h>
#include <pm_shared.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui/client_viewport.h"
#include "gameui/options/colorpicker/texture_manager.h"
#include "hud_renderer.h"

#include "demo.h"
#include "demo_api.h"
#include "cl_voice_status.h"
#include "bhlcfg.h"
#include "results.h"
#include "svc_messages.h"
#include "sdl_rt.h"
#include "fog.h"
#include "engine_builds.h"

#if USE_UPDATER
#include "updater/update_checker.h"
#include "updater/update_installer.h"
#endif

// HUD Elements
#include "hud/ammo.h"
#include "hud/chat.h"
#include "hud/crosshair.h"
#include "hud/health.h"
#include "hud/spectator.h"
#include "hud/geiger.h"
#include "hud/train.h"
#include "hud/battery.h"
#include "hud/flashlight.h"
#include "hud/message.h"
#include "hud/scores.h"
#include "hud/statusbar.h"
#include "hud/death_notice.h"
#include "hud/death_notice_panel.h"
#include "hud/ammo_secondary.h"
#include "hud/text_message.h"
#include "hud/status_icons.h"
#include "hud/menu.h"
#include "hud/voice_status.h"
#include "hud/voice_status_self.h"
#include "hud/speedometer.h"
#include "hud/jumpspeed.h"
#include "hud/timer.h"
#include "hud/strafeguide.h"

// Adrenaline Gamer HUD Elements
#include "hud/ag/ag_countdown.h"
#include "hud/ag/ag_ctf.h"
#include "hud/ag/ag_global.h"
#include "hud/ag/ag_location.h"
#include "hud/ag/ag_longjump.h"
#include "hud/ag/ag_nextmap.h"
#include "hud/ag/ag_playerid.h"
#include "hud/ag/ag_settings.h"
#include "hud/ag/ag_sudden_death.h"
#include "hud/ag/ag_timeout.h"
#include "hud/ag/ag_vote.h"

struct HudScaleInfo
{
	//! The sprite resolution.
	int iRes = 0;

	//! The minimum height for this scale to be selected automatically.
	int iHeight = 0;

	//! Enum value.
	EHudScale nScale = EHudScale::Auto;

	//! The file to test for support.
	const char *szTestFile = nullptr;

	//! @returns Whether this scale is supported by the given max scale.
	bool IsSupported(EHudScale maxScale) const
	{
		return nScale <= maxScale;
	}
};

//! The list of allowed HUD sizes.
static constexpr HudScaleInfo HUD_SCALE_INFO[] = {
	HudScaleInfo { 320,  240,  EHudScale::X05, "sprites/320hud1.spr" },
	HudScaleInfo { 640,  480,  EHudScale::X1,  "sprites/640hud1.spr" },
	HudScaleInfo { 1280, 960,  EHudScale::X2,  "sprites/1280/hud_bucket0.spr" },
	HudScaleInfo { 2560, 1920, EHudScale::X4,  "sprites/2560/hud_bucket0.spr" },
};

extern cvar_t *cl_lw;

ConVar cl_bhopcap("cl_bhopcap", "2", FCVAR_BHL_ARCHIVE, "Enables/disables bhop speed cap, '2' - detect automatically");
ConVar hud_color("hud_color", "255 160 0", FCVAR_BHL_ARCHIVE, "Main color of HUD elements");
ConVar hud_color1("hud_color1", "0 255 0", FCVAR_BHL_ARCHIVE, "HUD color when >= 90%");
ConVar hud_color2("hud_color2", "255 160 0", FCVAR_BHL_ARCHIVE, "HUD color when [50%; 90%)");
ConVar hud_color3("hud_color3", "255 96 0", FCVAR_BHL_ARCHIVE, "HUD color when (25%; 50%)");
ConVar hud_draw("hud_draw", "1", FCVAR_ARCHIVE, "Opacity of the HUD");
ConVar hud_dim("hud_dim", "1", FCVAR_BHL_ARCHIVE, "Dim inactive HUD elements");
ConVar hud_scale("hud_scale", "0", FCVAR_BHL_ARCHIVE, "HUD Scale: Auto, 50%, 100%, 200%, 400% (restart required)");
ConVar hud_capturemouse("hud_capturemouse", "1", FCVAR_ARCHIVE);
ConVar hud_classautokill("hud_classautokill", "1", FCVAR_ARCHIVE | FCVAR_USERINFO, "Whether or not to suicide immediately on TF class switch");
ConVar cl_autowepswitch("cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_BHL_ARCHIVE | FCVAR_USERINFO, "Controls autoswitching to best weapon on pickup\n  0 - never, 1 - always, 2 - unless firing");
ConVar cl_righthand("cl_righthand", "0", FCVAR_BHL_ARCHIVE, "If enabled, activates the left-handed mode");
ConVar cl_viewmodel_fov("cl_viewmodel_fov", "0", FCVAR_BHL_ARCHIVE, "Sets the field-of-view for the viewmodel");
ConVar showtriggers("showtriggers", "0", 0, "Shows trigger brushes");

ConVar aghl_version("aghl_version", APP_VERSION, 0, "BugfixedHL version");
ConVar aghl_supports("aghl_supports", "0", 0, "Bitfield of features supported by this client");
ConVar cl_enable_html_motd("cl_enable_html_motd", "1", FCVAR_BHL_ARCHIVE, "Enables/disables support for HTML MOTDs");

ConVar zoom_sensitivity_ratio("zoom_sensitivity_ratio", "1.2", FCVAR_ARCHIVE | FCVAR_BHL_ARCHIVE);

static Color s_DefaultColorCodeColors[10] = {
	Color(0xFF, 0xAA, 0x00, 0xFF), // ^0 orange/reset
	Color(0xFF, 0x00, 0x00, 0xFF), // ^1 red
	Color(0x00, 0xFF, 0x00, 0xFF), // ^2 green
	Color(0xFF, 0xFF, 0x00, 0xFF), // ^3 yellow
	Color(0x00, 0x00, 0xFF, 0xFF), // ^4 blue
	Color(0x00, 0xFF, 0xFF, 0xFF), // ^5 cyan
	Color(0xFF, 0x00, 0xFF, 0xFF), // ^6 magenta
	Color(0x88, 0x88, 0x88, 0xFF), // ^7 grey
	Color(0xFF, 0xFF, 0xFF, 0xFF), // ^8 white
	Color(0xFF, 0xAA, 0x00, 0xFF), // ^9 orange/reset
};

const Color NoTeamColor::Orange = Color(255, 178, 0, 255);
const Color NoTeamColor::White = Color(216, 216, 216, 255);

CON_COMMAND(pm_reset, "Resets player movement settings if they were detected incorrectly")
{
	PM_ResetBHopDetection();
	PM_ResetUseSlowDownDetection();
	ConPrintf("Player movement settings reset.\n");
}

// inputw32.cpp
void IN_RunFrame();

template <void (CClientViewport::*FUNC)(const char *, int, void *)>
void HookViewportMessage(const char *name)
{
	gEngfuncs.pfnHookUserMsg((char *)name, [](const char *pszName, int iSize, void *pbuf) -> int {
		if (g_pViewport)
			(g_pViewport->*FUNC)(pszName, iSize, pbuf);
		return 1;
	});
}

template <int (CHud::*FUNC)(const char *, int, void *)>
void HookHudMessage(const char *name)
{
	gEngfuncs.pfnHookUserMsg((char *)name, [](const char *pszName, int iSize, void *pbuf) -> int {
		return (gHUD.*FUNC)(pszName, iSize, pbuf);
	});
}

static void AboutCommand(void)
{
	ConPrintf("BugfixedHL-Rebased\n");
	ConPrintf("Bugfixed and improved Half-Life Client\n");
	ConPrintf("Version: " APP_VERSION "%s\n", IsDebug() ? " [Debug Build]" : "");
	ConPrintf("Engine: %s\n", gHUD.GetEngineVersion());
	ConPrintf("\n");
	ConPrintf("Github: " BHL_GITHUB_URL "\n");
	ConPrintf("Discussion forum: " BHL_FORUM_URL "\n");
}

//! Gets the current HUD size (either user-selected or auto-detected).
//! @returns iRes.
static int GetHudSize(const SCREENINFO &screenInfo, EHudScale maxScale)
{
	EHudScale userScale = hud_scale.GetEnumClamped<EHudScale>();

	if (userScale != EHudScale::Auto)
	{
		// Use user override
		userScale = clamp(userScale, EHudScale::X05, maxScale);
		const HudScaleInfo &info = *std::find_if(std::begin(HUD_SCALE_INFO), std::end(HUD_SCALE_INFO), [&](const HudScaleInfo &x)
		    { return x.nScale == userScale; });

		gEngfuncs.Con_DPrintf("HUD Size Override: %dx%d\n", info.iRes, info.iHeight);
		return info.iRes;
	}

	// Auto-detect
	for (auto it = std::rbegin(HUD_SCALE_INFO); it != std::rend(HUD_SCALE_INFO); ++it)
	{
		if (!it->IsSupported(maxScale))
			continue;

		if (screenInfo.iHeight >= it->iHeight)
		{
			// Found the largest one.
			gEngfuncs.Con_DPrintf(
				"HUD Size Auto-detect: %dx%d for screen %dx%d\n",
				it->iRes, it->iHeight,
			    screenInfo.iWidth, screenInfo.iHeight);
			return it->iRes;
		}
	}

	// Too low resolution. Fall back to the smallest one.
	const HudScaleInfo &fallbackInfo = HUD_SCALE_INFO[0];
	gEngfuncs.Con_DPrintf(
	    "HUD Size Auto-detect: fallback %dx%d for too small screen %dx%d\n",
	    fallbackInfo.iRes, fallbackInfo.iHeight,
	    screenInfo.iWidth, screenInfo.iHeight);
	return fallbackInfo.iRes;
}

static int CountSpritesOfRes(const client_sprite_t* pSpriteList, int nTotalCount, int iRes)
{
	int count = 0;

	for (int i = 0; i < nTotalCount; i++)
	{
		if (pSpriteList[i].iRes == iRes)
			count++;
	}

	return count;
}

CHud::CHud()
{
}

CHud::~CHud()
{
}

// This is called every time the DLL is loaded
void CHud::Init(void)
{
	// Check that elem list is empty
	Assert(m_HudList.empty());

	// Fill color code colors with default ones
	memcpy(m_ColorCodeColors, s_DefaultColorCodeColors, sizeof(s_DefaultColorCodeColors));

	// Set player info IDs
	for (int i = 1; i < MAX_PLAYERS; i++)
		CPlayerInfo::m_sPlayerInfo[i].m_iIndex = i;

	// Check for AG
	m_bIsAg = !strcmp(gEngfuncs.pfnGetGameDirectory(), "ag");
	PM_SetIsAG(m_bIsAg);

	m_MaxHudScale = DetectMaxHudScale();

	HookHudMessage<&CHud::MsgFunc_Logo>("Logo");
	HookHudMessage<&CHud::MsgFunc_ResetHUD>("ResetHUD");
	HookHudMessage<&CHud::MsgFunc_GameMode>("GameMode");
	HookHudMessage<&CHud::MsgFunc_InitHUD>("InitHUD");
	HookHudMessage<&CHud::MsgFunc_ViewMode>("ViewMode");
	HookHudMessage<&CHud::MsgFunc_SetFOV>("SetFOV");
	HookHudMessage<&CHud::MsgFunc_Concuss>("Concuss");
	HookHudMessage<&CHud::MsgFunc_Logo>("Logo");
	HookHudMessage<&CHud::MsgFunc_Fog>("Fog");

	// TFFree CommandMenu
	HookCommand("+commandmenu", [] {
		if (g_pViewport)
			g_pViewport->ShowCommandMenu();
	});

	HookCommand("-commandmenu", [] {
		if (g_pViewport)
			g_pViewport->InputSignalHideCommandMenu();
	});

	HookCommand("ForceCloseCommandMenu", [] {
		if (g_pViewport)
			g_pViewport->HideCommandMenu();
	});

	HookCommand("special", [] {
		if (g_pViewport)
			g_pViewport->InputPlayerSpecial();
	});

	HookCommand("about", AboutCommand);

	EngineClientCmd("alias zpecial \"append _zpecial\"");

	HookViewportMessage<&CClientViewport::MsgFunc_ValClass>("ValClass");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamNames>("TeamNames");
	HookViewportMessage<&CClientViewport::MsgFunc_Feign>("Feign");
	HookViewportMessage<&CClientViewport::MsgFunc_Detpack>("Detpack");
	HookViewportMessage<&CClientViewport::MsgFunc_MOTD>("MOTD");
	HookViewportMessage<&CClientViewport::MsgFunc_HtmlMOTD>("HtmlMOTD");
	HookViewportMessage<&CClientViewport::MsgFunc_BuildSt>("BuildSt");
	HookViewportMessage<&CClientViewport::MsgFunc_RandomPC>("RandomPC");
	HookViewportMessage<&CClientViewport::MsgFunc_ServerName>("ServerName");
	HookViewportMessage<&CClientViewport::MsgFunc_ScoreInfo>("ScoreInfo");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamScore>("TeamScore");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamInfo>("TeamInfo");

	HookViewportMessage<&CClientViewport::MsgFunc_Spectator>("Spectator");
	HookViewportMessage<&CClientViewport::MsgFunc_AllowSpec>("AllowSpec");

	// VGUI Menus
	HookViewportMessage<&CClientViewport::MsgFunc_VGUIMenu>("VGUIMenu");

	m_iLogo = 0;
	m_iFOV = 0;

	cl_lw = gEngfuncs.pfnGetCvarPointer("cl_lw");

	m_pSpriteList = NULL;

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	// Load default HUD colors into m_HudColor*.
	UpdateHudColors();

	// Create HUD elements
	RegisterHudElem<CHudAmmo>();
	RegisterHudElem<CHudHealth>();
	RegisterHudElem<CHudChat>();
	RegisterHudElem<CHudCrosshair>();
	RegisterHudElem<CHudSpectator>();
	RegisterHudElem<CHudGeiger>();
	RegisterHudElem<CHudTrain>();
	RegisterHudElem<CHudBattery>();
	RegisterHudElem<CHudFlashlight>();
	RegisterHudElem<CHudMessage>();
	RegisterHudElem<CHudScores>();
	RegisterHudElem<CHudStatusBar>();
	RegisterHudElem<CHudDeathNotice>();
	RegisterHudElem<CHudAmmoSecondary>();
	RegisterHudElem<CHudTextMessage>();
	RegisterHudElem<CHudStatusIcons>();
	RegisterHudElem<CHudMenu>();
	RegisterHudElem<CHudVoiceStatus>();
	RegisterHudElem<CHudVoiceStatusSelf>();
	RegisterHudElem<CHudSpeedometer>();
	RegisterHudElem<CHudJumpspeed>();
	RegisterHudElem<CHudTimer>();
	RegisterHudElem<CHudStrafeGuide>();

	if (CHudRenderer::Get().IsAvailable())
	{
		RegisterHudElem<CHudDeathNoticePanel>();
	}

	ClientVoiceMgr_Init();

	// Create AG HUD elements
	RegisterHudElem<AgHudGlobal>();
	RegisterHudElem<AgHudCountdown>();
	RegisterHudElem<AgHudCTF>();
	RegisterHudElem<AgHudLocation>();
	RegisterHudElem<AgHudLongjump>();
	RegisterHudElem<AgHudNextmap>();
	RegisterHudElem<AgHudPlayerId>();
	RegisterHudElem<AgHudSettings>();
	RegisterHudElem<AgHudSuddenDeath>();
	RegisterHudElem<AgHudTimeout>();
	RegisterHudElem<AgHudVote>();

	// Init HUD elements
	for (CHudElem *i : m_HudList)
		i->Init();

	m_HudList.shrink_to_fit();
	MsgFunc_ResetHUD(0, 0, NULL);
	colorpicker::gTexMgr.Init();

	g_pViewport->ReloadLayout();

	bhlcfg::Init();
	CResults::Get().Init();

#if USE_UPDATER
	CHttpClient::Get();
	CUpdateChecker::Get().Init();
#endif

	UpdateSupportsCvar();

	if (GetEngineBuild() >= ENGINE_BUILD_ANNIVERSARY_FIRST)
	{
		gEngfuncs.pfnClientCmd("richpresence_gamemode\n"); // reset
		gEngfuncs.pfnClientCmd("richpresence_update\n");
	}
}

void CHud::VidInit(void)
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// Reset all player info
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		GetPlayerInfo(i)->Reset();
	}

	// Reset all team info
	for (int i = 0; i <= MAX_TEAMS; i++)
	{
		GetTeamInfo(i)->Reset(i);
	}

	CSvcMessages::Get().VidInit();

	// ----------
	// Load Sprites
	// ---------
	//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;

	// Only update the scale once - otherwise sprites break
	if (m_iRes == -1)
		m_iRes = GetHudSize(m_scrinfo, GetMaxHudScale());

	// Only load this once
	if (!m_pSpriteList)
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (!m_pSpriteList)
			HUD_FatalError("Failed to load sprites/hud.txt.\nYour game is corrupted.");

		// count the number of sprites of the appropriate res
		m_iSpriteCount = CountSpritesOfRes(m_pSpriteList, m_iSpriteCountAllRes, m_iRes);

		if (m_iSpriteCount == 0)
		{
			Warning("Found no sprites with resolution of %d. Defaulting to %d.\n", m_iRes, HUD_FALLBACK_RES);
			m_iRes = HUD_FALLBACK_RES;
			m_iSpriteCount = CountSpritesOfRes(m_pSpriteList, m_iSpriteCountAllRes, m_iRes);
		}

		if (m_iSpriteCount == 0)
			HUD_FatalError("Failed to find sprites with resolution of %d in sprites/hud.txt.\nYour game is corrupted.", m_iRes);

		// allocated memory for sprite handle arrays
		m_rghSprites.resize(m_iSpriteCount);
		m_rgrcRects.resize(m_iSpriteCount);
		m_rgszSpriteNames.resize(m_iSpriteCount);
		m_rgSpritePaths.resize(m_iSpriteCount);

		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for (int j = 0; j < m_iSpriteCountAllRes; j++)
		{
			if (p->iRes == m_iRes)
			{
				char sz[256];
				sprintf(sz, "sprites/%s.spr", p->szSprite);
				m_rghSprites[index] = SPR_Load(sz);
				m_rgrcRects[index] = p->rc;
				Q_strncpy(m_rgszSpriteNames[index].name, p->szName, MAX_SPRITE_NAME_LENGTH);
				m_rgSpritePaths[index] = sz;

				index++;
			}

			p++;
		}

		// Add AG CTF sprites on non-AG clients
		// AG has them in hud.txt
		if (!IsAG())
		{
			AddSprite(client_sprite_t { "item_flag_team1", "ag_ctf", 0, 640, wrect_t { 120, 160, 0, 40 } });
			AddSprite(client_sprite_t { "item_flag_team2", "ag_ctf", 0, 640, wrect_t { 120, 160, 0, 40 } });
			AddSprite(client_sprite_t { "icon_ctf_home", "ag_ctf", 0, 640, wrect_t { 0, 40, 0, 40 } });
			AddSprite(client_sprite_t { "icon_ctf_stolen", "ag_ctf", 0, 640, wrect_t { 40, 80, 0, 40 } });
			AddSprite(client_sprite_t { "icon_ctf_lost", "ag_ctf", 0, 640, wrect_t { 80, 120, 0, 40 } });
			AddSprite(client_sprite_t { "icon_ctf_carry", "ag_ctf", 0, 640, wrect_t { 120, 160, 0, 40 } });
			AddSprite(client_sprite_t { "icon_ctf_score", "ag_ctf_score", 0, 640, wrect_t { 0, 16, 0, 16 } });
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		Assert(m_rghSprites.size() == m_iSpriteCount);
		Assert(m_rgrcRects.size() == m_iSpriteCount);
		Assert(m_rgszSpriteNames.size() == m_iSpriteCount);
		Assert(m_rgSpritePaths.size() == m_iSpriteCount);

		for (int i = 0; i < m_iSpriteCount; i++)
		{
			m_rghSprites[i] = SPR_Load(m_rgSpritePaths[i].c_str());
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex("number_0");

	if (m_HUD_number_0 == -1)
		HUD_FatalError("Failed to find sprite 'number_0' in the sprite list.\nYour game is corrupted.");

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	gFog.ClearFog();

	for (CHudElem *i : m_HudList)
		i->VidInit();
}

void CHud::Frame(double time)
{
	m_iFrameCount++;

	vgui2::GetAnimationController()->UpdateAnimations(gEngfuncs.GetClientTime());
	colorpicker::gTexMgr.RunFrame();

	IN_RunFrame();

	CHudVoiceStatus::Get()->RunFrame(time);
	CResults::Get().Frame();

#if USE_UPDATER
	CHttpClient::Get().RunFrame();
	CUpdateChecker::Get().RunFrame();
	CUpdateInstaller::Get().RunFrame();
#endif

	// Update aghl_supports
	if (cl_enable_html_motd.GetBool() != m_bIsHTMLEnabled)
	{
		m_bIsHTMLEnabled = cl_enable_html_motd.GetBool();
		UpdateSupportsCvar();
	}

	while (m_NextFrameQueue.size())
	{
		auto &i = m_NextFrameQueue.front();
		i();
		m_NextFrameQueue.pop();
	}
}

void CHud::Shutdown()
{
#if USE_UPDATER
	CUpdateInstaller::Get().Shutdown();
	CHttpClient::Get().Shutdown();
#endif
	bhlcfg::Shutdown();
	ClientVoiceMgr_Shutdown();
	colorpicker::gTexMgr.Shutdown();

	for (CHudElem *i : m_HudList)
	{
		// VGUI2 is shut down before HUD_Shutdown is called.
		// vgui2::~Panel calls VGUI2 interfaces which are not available.
		if (!dynamic_cast<vgui2::Panel *>(i))
			delete i;
	}
}

void CHud::ApplyViewportSchemeSettings(vgui2::IScheme *pScheme)
{
	char buf[64];
	for (int i = 0; i <= 9; i++)
	{
		snprintf(buf, sizeof(buf), "ColorCode%d", i);
		m_ColorCodeColors[i] = pScheme->GetColor(buf, s_DefaultColorCodeColors[i]);
	}

	vgui2::Label::SetDefaultColorCodeArray(m_ColorCodeColors);
}

void CHud::SaveEngineVersion()
{
	cvar_t *sv_version = sv_version = gEngfuncs.pfnGetCvarPointer("sv_version");
	if (sv_version)
	{
		Q_strncpy(m_szEngineVersion, sv_version->string, sizeof(m_szEngineVersion));

		// Parse build number
		std::string_view version = m_szEngineVersion;
		size_t lastComma = version.rfind(',');

		if (lastComma != std::string::npos)
		{
			const char *buildStr = m_szEngineVersion + lastComma + 1;
			m_iEngineBuildNumber = atoi(buildStr);
		}
	}
	else
	{
		Q_strncpy(m_szEngineVersion, "< sv_version not found >", sizeof(m_szEngineVersion));
		m_iEngineBuildNumber = -1;
	}
}

bool CHud::IsAG()
{
	return m_bIsAg;
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud::GetSpriteIndex(const char *SpriteName)
{
	// look through the loaded sprite name list for SpriteName
	for (int i = 0; i < m_iSpriteCount; i++)
	{
		if (Q_stricmp(SpriteName, m_rgszSpriteNames[i].name) == 0)
			return i;
	}

	return -1; // invalid sprite
}

void CHud::AddSprite(const client_sprite_t &p)
{
	// Search for existing sprite
	int i = 0;
	for (i = 0; i < m_iSpriteCount; i++)
	{
		if (!Q_stricmp(m_rgszSpriteNames[i].name, p.szName))
			return;
	}

	char sz[256];
	snprintf(sz, sizeof(sz), "sprites/%s.spr", p.szSprite);

	m_rghSprites.push_back(SPR_Load(sz));
	m_rgrcRects.push_back(p.rc);

	// Copy sprite name
	m_rgszSpriteNames.push_back({});
	Q_strncpy(m_rgszSpriteNames[m_iSpriteCount].name, p.szName, MAX_SPRITE_NAME_LENGTH);

	m_rgSpritePaths.emplace_back(sz);

	m_iSpriteCount++;

	Assert(m_rghSprites.size() == m_iSpriteCount);
	Assert(m_rgrcRects.size() == m_iSpriteCount);
	Assert(m_rgszSpriteNames.size() == m_iSpriteCount);
	Assert(m_rgSpritePaths.size() == m_iSpriteCount);
}

float g_lastFOV = 0.0;

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV(void)
{
	if (gEngfuncs.pDemoAPI->IsRecording())
	{
		// Write it
		int i = 0;
		unsigned char buf[100];

		// Active
		*(float *)&buf[i] = g_lastFOV;
		i += sizeof(float);

		Demo_WriteBuffer(TYPE_ZOOM, i, buf);
	}

	if (gEngfuncs.pDemoAPI->IsPlayingback())
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

float CHud::GetSensitivity(void)
{
	return m_flMouseSensitivity;
}

EBHopCap CHud::GetBHopCapState()
{
	return cl_bhopcap.GetEnumClamped<EBHopCap>();
}

bool CHud::IsHTMLEnabled()
{
	return m_bIsHTMLEnabled;
}

void CHud::CallOnNextFrame(std::function<void()> f)
{
	Assert(f);
	m_NextFrameQueue.push(f);
}

Color CHud::GetHudColor(HudPart hudPart, int value)
{
	if (hudPart == HudPart::Common)
		return m_HudColor;

	if (value >= 90)
		return m_HudColor1;

	if (value >= 50)
		return m_HudColor2;

	if (value > 25 || hudPart == HudPart::Armor)
		return m_HudColor3;

	return Color(255, 0, 0, 255);
}

void CHud::GetHudColor(HudPart hudPart, int value, int &r, int &g, int &b)
{
	Color c = GetHudColor(hudPart, value);
	r = c.r();
	g = c.g();
	b = c.b();
}

void CHud::GetHudAmmoColor(int value, int maxvalue, int &r, int &g, int &b)
{
	Color c;
	if (maxvalue == -1 || maxvalue == 0)
	{
		// Custom weapons will use default colors
		c = m_HudColor;
	}
	else if ((value * 100) / maxvalue > 90)
	{
		c = m_HudColor1;
	}
	else if ((value * 100) / maxvalue > 50)
	{
		c = m_HudColor2;
	}
	else if ((value * 100) / maxvalue > 20)
	{
		c = m_HudColor3;
	}
	else
	{
		r = 255;
		g = 0;
		b = 0;
		return;
	}

	r = c.r();
	g = c.g();
	b = c.b();
}

float CHud::GetHudTransparency()
{
	return clamp(hud_draw.GetFloat(), 0.f, 1.f);
}

Color CHud::GetClientColor(int idx, Color noTeamColor)
{
	int team = GetPlayerInfo(idx)->Update()->GetTeamNumber();

	if (team == 0)
		return noTeamColor;
	else
		return g_pViewport->GetTeamColor(team);
}

void CHud::GetClientColorAsFloat(int idx, float out[3], Color noTeamColor)
{
	Color c = GetClientColor(idx, noTeamColor);
	out[0] = c[0] / 255.f;
	out[1] = c[1] / 255.f;
	out[2] = c[2] / 255.f;
}

void CHud::UpdateHudColors()
{
	ParseColor(hud_color.GetString(), m_HudColor);
	ParseColor(hud_color1.GetString(), m_HudColor1);
	ParseColor(hud_color2.GetString(), m_HudColor2);
	ParseColor(hud_color3.GetString(), m_HudColor3);
}

void CHud::UpdateSupportsCvar()
{
	bhl::E_ClientSupports supports = bhl::E_ClientSupports::UnicodeMotd;

	if (IsHTMLEnabled())
		SetEnumFlag(supports, bhl::E_ClientSupports::HtmlMotd);

	char buf[64];
	snprintf(buf, sizeof(buf), "aghl_supports %u", static_cast<unsigned int>(supports));
	gEngfuncs.pfnClientCmd(buf);
}

EHudScale CHud::DetectMaxHudScale()
{
	const HudScaleInfo *pMaxScaleInfo = nullptr;

	for (const HudScaleInfo& i : HUD_SCALE_INFO)
	{
		if (g_pFullFileSystem->FileExists(i.szTestFile))
		{
			pMaxScaleInfo = &i;
		}
		else
		{
			// If i is not supported, then i + 1 isn't supported as well.
			// Limited by the use of "x <= maxScale" check.
			break;
		}
	}

	if (!pMaxScaleInfo)
	{
		GetSDL()->ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "BugfixedHL Error", "HUD sprites are missing. Verify game files.");
		std::abort();
	}

	gEngfuncs.Con_DPrintf("Maximum HUD scale: %dx%d\n", pMaxScaleInfo->iRes, pMaxScaleInfo->iHeight);
	return pMaxScaleInfo->nScale;
}

CON_COMMAND(append, "Puts a command into the end of the command buffer")
{
	if (gEngfuncs.Cmd_Argc() != 2)
	{
		if (!gEngfuncs.pDemoAPI->IsPlayingback())
			gEngfuncs.Con_Printf("append <command> - put the command into the end of the command buffer.\n");

		return;
	}

	EngineClientCmd(gEngfuncs.Cmd_Argv(1));
}

CON_COMMAND(_toggle, "Switches cvar values from arguments.")
{
	int argc = gEngfuncs.Cmd_Argc();
	if (argc <= 1 || argc == 3)
	{
		gEngfuncs.Con_Printf("usage: _toggle <cvar> or _toggle <cvar> <val1> <val2> [val3] ... [valN]\n");
		return;
	}

	ConVarRef cvar(gEngfuncs.Cmd_Argv(1));

	if (!cvar.IsValid())
	{
		gEngfuncs.Con_Printf("_toggle failed: cvar '%s' not found.\n", gEngfuncs.Cmd_Argv(1));
		return;
	}

	char cmd[256];

	if (argc == 2)
	{
		// Toggle between 0 and 1
		sprintf(cmd, "%s %d", gEngfuncs.Cmd_Argv(1), (int)(!cvar.GetBool()));
		EngineClientCmd(cmd);
		return;
	}
	else
	{
		for (int i = 2; i < argc; i++)
		{
			if (!strcmp(cvar.GetString(), gEngfuncs.Cmd_Argv(i)))
			{
				if (i + 1 < argc)
				{
					// switch cvar value to the next one
					sprintf(cmd, "%s \"%s\"", gEngfuncs.Cmd_Argv(1), gEngfuncs.Cmd_Argv(i + 1));
					EngineClientCmd(cmd);
					return;
				}
				else
				{
					// if we have get to the top of _toggle values list, then start from the beginning
					sprintf(cmd, "%s \"%s\"", gEngfuncs.Cmd_Argv(1), gEngfuncs.Cmd_Argv(2));
					EngineClientCmd(cmd);
					return;
				}
			}
		}

		// if cvar value isn't equal to any values from _toggle, then set it to the first value of _toggle
		cvar.SetValue(gEngfuncs.Cmd_Argv(2));
		return;
	}
}
