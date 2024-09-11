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
// death notice
//
#include <string.h>
#include <stdio.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "death_notice.h"
#include "spectator.h"
#include "vgui/client_viewport.h"
#include "death_notice_panel.h"

struct DeathNoticeItem
{
	char szKiller[MAX_PLAYERNAME_LENGTH * 2];
	char szVictim[MAX_PLAYERNAME_LENGTH * 2];
	int iId; // the index number of the associated sprite
	int iSuicide;
	int iTeamKill;
	int iNonPlayerKill;
	float flDisplayTime;
	float KillerColor[3];
	float VictimColor[3];
	bool bKillerHasColor;
	bool bVictimHasColor;
};

extern ConVar hud_deathnotice_vgui;
ConVar cl_killsound("cl_killsound", "1", FCVAR_BHL_ARCHIVE, "Play a sound on kill");
ConVar cl_killsound_path("cl_killsound_path", "buttons/bell1.wav", FCVAR_BHL_ARCHIVE, "Path to a sound on kill");
ConVar hud_deathnotice_time("hud_deathnotice_time", "6", FCVAR_ARCHIVE | FCVAR_BHL_ARCHIVE, "How long should death notice stay up for");
ConVar hud_deathnotice_color("hud_deathnotice_color", "255 80 0", FCVAR_BHL_ARCHIVE, "Color of death notice sprite");
ConVar hud_deathnotice_color_tk("hud_deathnotice_color_tk", "10 240 10", FCVAR_BHL_ARCHIVE, "Color of death notice teamkill sprite");
ConVar hud_deathnotice_draw_always("hud_deathnotice_draw_always", "0", FCVAR_BHL_ARCHIVE, "Display the kill feed even when hud_draw is 0. Useful when recording frag movies.");

#define MAX_DEATHNOTICES 4
static int DEATHNOTICE_DISPLAY_TIME = 6;

#define DEATHNOTICE_TOP 32

DeathNoticeItem rgDeathNoticeList[MAX_DEATHNOTICES + 1];

DEFINE_HUD_ELEM(CHudDeathNotice);

void CHudDeathNotice::Init(void)
{
	BaseHudClass::Init();

	HookMessage<&CHudDeathNotice::MsgFunc_DeathMsg>("DeathMsg");
}

void CHudDeathNotice::InitHudData()
{
	memset(rgDeathNoticeList, 0, sizeof(rgDeathNoticeList));
}

void CHudDeathNotice::VidInit()
{
	m_HUD_d_skull = gHUD.GetSpriteIndex("d_skull");
}

void CHudDeathNotice::Draw(float flTime)
{
	if (hud_deathnotice_vgui.GetBool() && CHudDeathNoticePanel::Get())
		return;

	int x, y;

	Color spriteColor = Color(255, 80, 0, 255);
	Color tkSpriteColor = Color(10, 240, 10, 255); // teamkill - sickly green
	ParseColor(hud_deathnotice_color.GetString(), spriteColor);
	ParseColor(hud_deathnotice_color_tk.GetString(), tkSpriteColor);

	for (int i = 0; i < MAX_DEATHNOTICES; i++)
	{
		if (rgDeathNoticeList[i].iId == 0)
			break; // we've gone through them all

		if (rgDeathNoticeList[i].flDisplayTime < flTime)
		{ // display time has expired
			// remove the current item from the list
			memmove(&rgDeathNoticeList[i], &rgDeathNoticeList[i + 1], sizeof(DeathNoticeItem) * (MAX_DEATHNOTICES - i));
			i--; // continue on the next item;  stop the counter getting incremented
			continue;
		}

		rgDeathNoticeList[i].flDisplayTime = min(rgDeathNoticeList[i].flDisplayTime, gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME);

		// Only draw if the viewport will let me
		if (g_pViewport && g_pViewport->AllowedToPrintText())
		{
			// Draw the death notice
			y = YRES(DEATHNOTICE_TOP) + 2 + (20 * i); //!!!

			int id = (rgDeathNoticeList[i].iId == -1) ? m_HUD_d_skull : rgDeathNoticeList[i].iId;
			HSPRITE spr = gHUD.GetSprite(id);
			wrect_t rect = gHUD.GetSpriteRect(id);
			x = ScreenWidth - ConsoleStringLen(rgDeathNoticeList[i].szVictim) - (rect.right - rect.left);

			if (!rgDeathNoticeList[i].iSuicide)
			{
				x -= (5 + ConsoleStringLen(rgDeathNoticeList[i].szKiller));

				// Draw killers name
				float *color = (rgDeathNoticeList[i].bKillerHasColor) ? rgDeathNoticeList[i].KillerColor : nullptr;
				x = 5 + DrawConsoleString(x, y, rgDeathNoticeList[i].szKiller, color);
			}

			// Draw death weapon
			if (rgDeathNoticeList[i].iTeamKill)
				SPR_Set(spr, tkSpriteColor.r(), tkSpriteColor.g(), tkSpriteColor.b());
			else
				SPR_Set(spr, spriteColor.r(), spriteColor.g(), spriteColor.b());

			SPR_DrawAdditive(0, x, y, &rect);

			x += (rect.right - rect.left);

			// Draw victims name (if it was a player that was killed)
			if (rgDeathNoticeList[i].iNonPlayerKill == FALSE)
			{
				float *color = (rgDeathNoticeList[i].bVictimHasColor) ? rgDeathNoticeList[i].VictimColor : nullptr;
				x = DrawConsoleString(x, y, rgDeathNoticeList[i].szVictim, color);
			}
		}
	}
}

void CHudDeathNotice::Think()
{
	if (hud_deathnotice_draw_always.GetBool())
		m_iFlags |= HUD_DRAW_ALWAYS;
	else
		m_iFlags &= ~HUD_DRAW_ALWAYS;
}

// This message handler may be better off elsewhere
int CHudDeathNotice::MsgFunc_DeathMsg(const char *pszName, int iSize, void *pbuf)
{
	if (!GetThisPlayerInfo())
	{
		// Not yet connected
		return 1;
	}

	m_iFlags |= HUD_ACTIVE;

	BEGIN_READ(pbuf, iSize);

	int killer = READ_BYTE();
	int victim = READ_BYTE();

	char killedwith[MAX_WEAPON_NAME];
	V_strcpy_safe(killedwith, "d_");
	strncat(killedwith, READ_STRING(), sizeof(killedwith) - 3);
	killedwith[sizeof(killedwith) - 1] = 0;

	if (g_pViewport)
		g_pViewport->DeathMsg(killer, victim);

	CHudSpectator::Get()->DeathMessage(victim);

	if (hud_deathnotice_vgui.GetBool() && CHudDeathNoticePanel::Get())
		CHudDeathNoticePanel::Get()->AddItem(killer, victim, killedwith);

	int i;
	for (i = 0; i < MAX_DEATHNOTICES; i++)
	{
		if (rgDeathNoticeList[i].iId == 0)
			break;
	}
	if (i == MAX_DEATHNOTICES)
	{ // move the rest of the list forward to make room for this item
		memmove(rgDeathNoticeList, rgDeathNoticeList + 1, sizeof(DeathNoticeItem) * MAX_DEATHNOTICES);
		i = MAX_DEATHNOTICES - 1;
	}

	if (g_pViewport)
		g_pViewport->GetAllPlayersInfo();

	// Get the Killer's name
	CPlayerInfo *killerInfo = nullptr;
	const char *killer_name;
	if (killer != 0 && (killerInfo = GetPlayerInfoSafe(killer)))
	{
		killer_name = killerInfo->GetDisplayName();

		if (killerInfo->GetTeamNumber() == 0)
		{
			rgDeathNoticeList[i].bKillerHasColor = false;
		}
		else
		{
			rgDeathNoticeList[i].bKillerHasColor = true;
			gHUD.GetClientColorAsFloat(killer, rgDeathNoticeList[i].KillerColor, NoTeamColor::Orange);
		}

		strncpy(rgDeathNoticeList[i].szKiller, killer_name, MAX_PLAYERNAME_LENGTH);
		rgDeathNoticeList[i].szKiller[MAX_PLAYERNAME_LENGTH - 1] = 0;
	}
	else
	{
		killer_name = "";
		rgDeathNoticeList[i].szKiller[0] = 0;
	}

	// Get the Victim's name
	const char *victim_name = NULL;
	// If victim is -1, the killer killed a specific, non-player object (like a sentrygun)
	if (((char)victim) != -1 && GetPlayerInfoSafe(victim))
		victim_name = GetPlayerInfo(victim)->GetDisplayName();
	if (!victim_name)
	{
		victim_name = "";
		rgDeathNoticeList[i].szVictim[0] = 0;
	}
	else
	{
		CPlayerInfo *victimInfo = GetPlayerInfoSafe(victim);

		if (!victimInfo || victimInfo->GetTeamNumber() == 0)
		{
			rgDeathNoticeList[i].bVictimHasColor = false;
		}
		else
		{
			rgDeathNoticeList[i].bVictimHasColor = true;
			gHUD.GetClientColorAsFloat(victim, rgDeathNoticeList[i].VictimColor, NoTeamColor::Orange);
		}

		strncpy(rgDeathNoticeList[i].szVictim, victim_name, MAX_PLAYER_NAME);
		rgDeathNoticeList[i].szVictim[MAX_PLAYER_NAME - 1] = 0;
	}

	// Is it a non-player object kill?
	if (((char)victim) == -1)
	{
		rgDeathNoticeList[i].iNonPlayerKill = TRUE;

		// Store the object's name in the Victim slot (skip the d_ bit)
		V_strcpy_safe(rgDeathNoticeList[i].szVictim, killedwith + 2);
	}
	else
	{
		if (killer == victim || killer == 0)
			rgDeathNoticeList[i].iSuicide = TRUE;

		if (!strcmp(killedwith, "d_teammate"))
			rgDeathNoticeList[i].iTeamKill = TRUE;
	}

	// Find the sprite in the list
	int spr = gHUD.GetSpriteIndex(killedwith);

	rgDeathNoticeList[i].iId = spr;

	DEATHNOTICE_DISPLAY_TIME = hud_deathnotice_time.GetInt();
	rgDeathNoticeList[i].flDisplayTime = gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME;

	// Play kill sound
	if (killerInfo && killerInfo->IsThisPlayer() && !rgDeathNoticeList[i].iNonPlayerKill && !rgDeathNoticeList[i].iSuicide && cl_killsound.GetFloat() > 0)
	{
		PlaySound(cl_killsound_path.GetString(), cl_killsound.GetFloat());
	}

	// Set color of own kills/deaths to yellow
	if (killer == GetThisPlayerInfo()->GetIndex() || victim == GetThisPlayerInfo()->GetIndex())
		console::SetColor(ConColor::Yellow);

	// Print to console
	if (rgDeathNoticeList[i].iNonPlayerKill)
	{
		ConsolePrint(rgDeathNoticeList[i].szKiller);
		ConsolePrint(" killed a ");
		ConsolePrint(rgDeathNoticeList[i].szVictim);
		ConsolePrint("\n");
	}
	else
	{
		// record the death notice in the console
		if (rgDeathNoticeList[i].iSuicide)
		{
			ConsolePrint(rgDeathNoticeList[i].szVictim);

			if (!strcmp(killedwith, "d_world"))
			{
				ConsolePrint(" died");
			}
			else
			{
				ConsolePrint(" killed self");
			}
		}
		else if (rgDeathNoticeList[i].iTeamKill)
		{
			ConsolePrint(rgDeathNoticeList[i].szKiller);
			ConsolePrint(" killed his teammate ");
			ConsolePrint(rgDeathNoticeList[i].szVictim);
		}
		else
		{
			ConsolePrint(rgDeathNoticeList[i].szKiller);
			ConsolePrint(" killed ");
			ConsolePrint(rgDeathNoticeList[i].szVictim);
		}

		if (killedwith && *killedwith && (*killedwith > 13) && strcmp(killedwith, "d_world") && !rgDeathNoticeList[i].iTeamKill)
		{
			ConsolePrint(" with ");

			// replace the code names with the 'real' names
			if (!strcmp(killedwith + 2, "egon"))
				V_strcpy_safe(killedwith, "d_gluon gun");
			if (!strcmp(killedwith + 2, "gauss"))
				V_strcpy_safe(killedwith, "d_tau cannon");

			ConsolePrint(killedwith + 2); // skip over the "d_" part
		}

		ConsolePrint("\n");
	}

	console::ResetColor();

	return 1;
}
