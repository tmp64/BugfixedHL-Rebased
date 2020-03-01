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

#include <appversion.h>
#include <bhl_urls.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui/client_viewport.h"

#include "demo.h"
#include "demo_api.h"
#include "voice_status.h"

// HUD Elements
#include "hud/ammo.h"
#include "hud/health.h"
#include "hud/saytext.h"
#include "hud/spectator.h"
#include "hud/geiger.h"
#include "hud/train.h"
#include "hud/battery.h"
#include "hud/flashlight.h"
#include "hud/message.h"
#include "hud/statusbar.h"
#include "hud/death_notice.h"
#include "hud/ammo_secondary.h"
#include "hud/text_message.h"
#include "hud/status_icons.h"
#include "hud/menu.h"

hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1]; // player info from the engine
extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1]; // additional player info sent directly to the client dll

class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 255;

		if (entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo) / sizeof(g_PlayerExtraInfo[0]))
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if (iTeam < 0)
			{
				iTeam = 0;
			}

			// FIXME:
			/*iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];*/
		}
	}

	virtual void UpdateCursorState()
	{
		g_pViewport->UpdateCursorState();
	}

	virtual int GetAckIconHeight()
	{
		return ScreenHeight - gHUD.m_iFontHeight * 3 - 6;
	}

	virtual bool CanShowSpeakerLabels()
	{
		// FIXME:
		/*if (g_pViewport && g_pViewport->m_pScoreBoard)
			return !g_pViewport->m_pScoreBoard->isVisible();
		else
			return false;*/
		return true;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;

extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

extern cvar_t *sensitivity;
cvar_t *cl_lw = NULL;

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
	ConsolePrint("BugfixedHL-NG\n");
	ConsolePrint("Bugfixed and improved Half-Life Client\n");
	ConsolePrint("Version: " APP_VERSION "\n");
	ConsolePrint("\n");
	ConsolePrint("Github: " BHL_GITHUB_URL "\n");
	ConsolePrint("Discussion forum: " BHL_FORUM_URL "\n");
}

// This is called every time the DLL is loaded
void CHud::Init(void)
{
	// Check that elem list is empty
	Assert(m_HudList.empty());

	HookHudMessage<&CHud::MsgFunc_Logo>("Logo");
	HookHudMessage<&CHud::MsgFunc_ResetHUD>("ResetHUD");
	HookHudMessage<&CHud::MsgFunc_GameMode>("GameMode");
	HookHudMessage<&CHud::MsgFunc_InitHUD>("InitHUD");
	HookHudMessage<&CHud::MsgFunc_ViewMode>("ViewMode");
	HookHudMessage<&CHud::MsgFunc_SetFOV>("SetFOV");
	HookHudMessage<&CHud::MsgFunc_Concuss>("Concuss");
	HookHudMessage<&CHud::MsgFunc_Logo>("Logo");

	// TFFree CommandMenu
	HookCommand("+commandmenu", [] {
		// FIXME:
		//if (g_pViewport)
		//	g_pViewport->ShowCommandMenu(g_pViewport->m_StandardMenu);
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

	HookViewportMessage<&CClientViewport::MsgFunc_ValClass>("ValClass");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamNames>("TeamNames");
	HookViewportMessage<&CClientViewport::MsgFunc_Feign>("Feign");
	HookViewportMessage<&CClientViewport::MsgFunc_Detpack>("Detpack");
	HookViewportMessage<&CClientViewport::MsgFunc_MOTD>("MOTD");
	HookViewportMessage<&CClientViewport::MsgFunc_BuildSt>("BuildSt");
	HookViewportMessage<&CClientViewport::MsgFunc_RandomPC>("RandomPC");
	HookViewportMessage<&CClientViewport::MsgFunc_ServerName>("ServerName");
	HookViewportMessage<&CClientViewport::MsgFunc_ScoreInfo>("ScoreInfo");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamScore>("TeamScore");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamInfo>("TeamInfo");

	HookViewportMessage<&CClientViewport::MsgFunc_Spectator>("Spectator");
	HookViewportMessage<&CClientViewport::MsgFunc_AllowSpec>("AllowSpec");

	HookViewportMessage<&CClientViewport::MsgFunc_SpecFade>("SpecFade");
	HookViewportMessage<&CClientViewport::MsgFunc_ResetFade>("ResetFade");

	// VGUI Menus
	HookViewportMessage<&CClientViewport::MsgFunc_VGUIMenu>("VGUIMenu");

	CVAR_CREATE("hud_classautokill", "1", FCVAR_ARCHIVE | FCVAR_USERINFO); // controls whether or not to suicide immediately on TF class switch
	CVAR_CREATE("hud_takesshots", "0", FCVAR_ARCHIVE); // controls whether or not to automatically take screenshots at the end of a round

	m_iLogo = 0;
	m_iFOV = 0;

	CVAR_CREATE("zoom_sensitivity_ratio", "1.2", 0);
	default_fov = CVAR_CREATE("default_fov", "90", FCVAR_ARCHIVE);
	m_pCvarStealMouse = CVAR_CREATE("hud_capturemouse", "1", FCVAR_ARCHIVE);
	m_pCvarDraw = CVAR_CREATE("hud_draw", "1", FCVAR_ARCHIVE);
	cl_lw = gEngfuncs.pfnGetCvarPointer("cl_lw");

	m_pSpriteList = NULL;

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	// Create HUD elements
	RegisterHudElem<CHudAmmo>();
	RegisterHudElem<CHudHealth>();
	RegisterHudElem<CHudSayText>();
	RegisterHudElem<CHudSpectator>();
	RegisterHudElem<CHudGeiger>();
	RegisterHudElem<CHudTrain>();
	RegisterHudElem<CHudBattery>();
	RegisterHudElem<CHudFlashlight>();
	RegisterHudElem<CHudMessage>();
	RegisterHudElem<CHudStatusBar>();
	RegisterHudElem<CHudDeathNotice>();
	RegisterHudElem<CHudAmmoSecondary>();
	RegisterHudElem<CHudTextMessage>();
	RegisterHudElem<CHudStatusIcons>();
	RegisterHudElem<CHudMenu>();

	RegisterHudElem<CVoiceStatus>();
	CVoiceStatus::Get()->SetVoiceStatusHelper(&g_VoiceStatusHelper);
	CVoiceStatus::Get()->SetParentPanel((vgui::Panel **)&g_pViewport);

	// Init HUD elements
	for (CHudElem *i : m_HudList)
		i->Init();

	m_HudList.shrink_to_fit();
	MsgFunc_ResetHUD(0, 0, NULL);
}

CHud::CHud()
{
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud::~CHud()
{
	for (CHudElem *i : m_HudList)
	{
		delete i;
	}
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
		if (strncmp(SpriteName, m_rgszSpriteNames.data() + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH) == 0)
			return i;
	}

	return -1; // invalid sprite
}

void CHud::VidInit(void)
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// ----------
	// Load Sprites
	// ---------
	//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;

	if (ScreenWidth < 640)
		m_iRes = 320;
	else
		m_iRes = 640;

	// Only load this once
	if (!m_pSpriteList)
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			int j;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
			m_rghSprites.resize(m_iSpriteCount);
			m_rgrcRects.resize(m_iSpriteCount);
			m_rgszSpriteNames.resize(m_iSpriteCount * MAX_SPRITE_NAME_LENGTH);

			p = m_pSpriteList;
			int index = 0;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy(&m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH);

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for (int j = 0; j < m_iSpriteCountAllRes; j++)
		{
			if (p->iRes == m_iRes)
			{
				char sz[256];
				sprintf(sz, "sprites/%s.spr", p->szSprite);
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex("number_0");

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	for (CHudElem *i : m_HudList)
		i->VidInit();
}

void CHud::Frame(double time)
{
	while (m_NextFrameQueue.size())
	{
		auto &i = m_NextFrameQueue.front();
		i();
		m_NextFrameQueue.pop();
	}
}

void CHud::Shutdown()
{
}

int CHud::MsgFunc_Logo(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// update Train data
	m_iLogo = READ_BYTE();

	return 1;
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase(const char *in, char *out)
{
	int len, start, end;

	len = strlen(in);

	// scan backward for '.'
	end = len - 1;
	while (end && in[end] != '.' && in[end] != '/' && in[end] != '\\')
		end--;

	if (in[end] != '.') // no '.', copy to end
		end = len - 1;
	else
		end--; // Found ',', copy to left of '.'

	// Scan backward for '/'
	start = len - 1;
	while (start >= 0 && in[start] != '/' && in[start] != '\\')
		start--;

	if (in[start] != '/' && in[start] != '\\')
		start = 0;
	else
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy(out, &in[start], len);
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame(const char *game)
{
	const char *gamedir;
	char gd[1024];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if (gamedir && gamedir[0])
	{
		COM_FileBase(gamedir, gd);
		if (!_stricmp(gd, game))
			return 1;
	}
	return 0;
}

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

int CHud::MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT("default_fov");

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	if (cl_lw && cl_lw->value)
		return 1;

	g_lastFOV = newfov;

	if (newfov == 0)
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if (m_iFOV == def_fov)
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)def_fov) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return 1;
}

float CHud::GetSensitivity(void)
{
	return m_flMouseSensitivity;
}

void CHud::CallOnNextFrame(std::function<void()> f)
{
	Assert(f);
	m_NextFrameQueue.push(f);
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
	safe_strcpy(str, str_orig, sizeof(str));
	for (char *c = str; *c; c++)
		*c = tolower(*c);

	std::vector<FindResult> found;

	// Iterate all cvars
	{
		char buf[128];
		cvar_t *item = gEngfuncs.GetFirstCvarPtr();
		for (; item; item = item->next)
		{
			safe_strcpy(buf, item->name, sizeof(buf));
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
			safe_strcpy(buf, item->name, sizeof(buf));
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
