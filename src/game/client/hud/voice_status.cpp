//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include "hud.h"
#include "cl_util.h"
#include "client_steam_context.h"
#include "cl_voice_status.h"
#include "voice_status.h"
#include "vgui/avatar_image.h"
#include "vgui/client_viewport.h"

DEFINE_HUD_ELEM(CHudVoiceStatus);

CHudVoiceStatus::CHudVoiceStatus()
    : vgui2::Panel(NULL, "HudVoiceStatus")
{
	SetParent(g_pViewport);
}

CHudVoiceStatus::~CHudVoiceStatus()
{
	ClearActiveList();
}

void CHudVoiceStatus::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_NameFont = pScheme->GetFont("Default", IsProportional());
	m_NoTeamColor = pScheme->GetColor("Orange", Color(255, 255, 255, 255));

	SetBgColor(Color(0, 0, 0, 0));
}

void CHudVoiceStatus::Init(void)
{
	BaseHudClass::Init();
	ClearActiveList();
}

void CHudVoiceStatus::RunFrame(float fTime)
{
	for (int iPlayerIndex = 1; iPlayerIndex <= VOICE_MAX_PLAYERS; iPlayerIndex++)
	{
		int activeSpeakerIndex = FindActiveSpeaker(iPlayerIndex);
		bool bSpeaking = GetClientVoiceMgr()->IsPlayerSpeaking(iPlayerIndex);

		if (activeSpeakerIndex != m_SpeakingList.InvalidIndex())
		{
			// update their speaking status
			m_SpeakingList[activeSpeakerIndex].bSpeaking = bSpeaking;
		}
		else
		{
			//=============================================================================
			// HPE_BEGIN:
			// [Forrest] Don't use UTIL_PlayerByIndex here.  It may be null for some players when
			// a match starts because the server only passes full player info as it affects
			// the client.
			//=============================================================================
			// if they are talking and not in the list, add them to the end
			if (bSpeaking)
			{
				ActiveSpeaker activeSpeaker;
				activeSpeaker.playerId = iPlayerIndex;
				activeSpeaker.bSpeaking = true;
				activeSpeaker.fAlpha = 0.0f;
				activeSpeaker.pAvatar = NULL;
				activeSpeaker.fAlphaMultiplier = 1.0f;

				//=============================================================================
				// HPE_BEGIN:
				// [pfreese] If a player is now talking set up their avatar
				//=============================================================================

				activeSpeaker.pAvatar = new CAvatarImage();
				activeSpeaker.pAvatar->SetDrawFriend(show_friend);
				CPlayerInfo *pi = GetPlayerInfo(iPlayerIndex)->Update();
				if (pi->IsConnected())
				{
					if (ClientSteamContext().SteamUtils())
					{
						CSteamID steamIDForPlayer(pi->GetValidSteamID64(), 1, ClientSteamContext().SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual);
						activeSpeaker.pAvatar->SetAvatarSteamID(steamIDForPlayer, k_EAvatarSize32x32);
					}
				}

				activeSpeaker.pAvatar->SetAvatarSize(avatar_wide, avatar_tall);

				//=============================================================================
				// HPE_END
				//=============================================================================

				m_SpeakingList.AddToTail(activeSpeaker);
			}
			//=============================================================================
			// HPE_END
			//=============================================================================
		}
	}

	for (int i = m_SpeakingList.Head(); i != m_SpeakingList.InvalidIndex();)
	{
		ActiveSpeaker &activeSpeaker = m_SpeakingList[i];

		if (activeSpeaker.bSpeaking)
		{
			if (fade_in_time > 0.0f)
			{
				activeSpeaker.fAlpha += fTime / fade_in_time;
				if (activeSpeaker.fAlpha > 1.0f)
					activeSpeaker.fAlpha = 1.0f;
			}
			else
			{
				activeSpeaker.fAlpha = 1.0f;
			}
		}
		else
		{
			if (fade_out_time > 0.0f)
			{
				activeSpeaker.fAlpha -= fTime / fade_out_time;
			}
			else
			{
				activeSpeaker.fAlpha = 0.0f;
			}

			if (activeSpeaker.fAlpha <= 0.0f)
			{
				// completely faded, remove them them from the list
				delete activeSpeaker.pAvatar;
				int iNext = m_SpeakingList.Next(i);
				m_SpeakingList.Remove(i);
				i = iNext;
				continue;
			}
		}
		i = m_SpeakingList.Next(i);
	}
}

void CHudVoiceStatus::Paint()
{
	if (m_iVoiceIconTexture == -1)
		return;

	int x, y, w, h;
	GetBounds(x, y, w, h);

	// Heights to draw the current voice item at
	int ypos = h - item_tall;

	int length = m_SpeakingList.Count();

	int iFontHeight = 0;

	if (length > 0)
	{
		vgui2::surface()->DrawSetTextFont(m_NameFont);
		vgui2::surface()->DrawSetTextColor(Color(255, 255, 255, 255));
		iFontHeight = vgui2::surface()->GetFontTall(m_NameFont);
	}

	//draw everyone in the list!
	FOR_EACH_LL(m_SpeakingList, i)
	{
		int playerId = m_SpeakingList[i].playerId;
		bool bIsAlive = true;
		CPlayerInfo *pi = GetPlayerInfo(playerId)->Update();

		float newAlphaMultiplier = m_SpeakingList[i].fAlphaMultiplier * m_SpeakingList[i].fAlpha;

		Color COLOR_WHITE = Color(255, 255, 255, 255 * newAlphaMultiplier);
		Color bgColor;

		if (pi->IsConnected() && pi->GetTeamNumber() != 0)
		{
			bgColor = g_pViewport->GetTeamColor(pi->GetTeamNumber());
		}
		else
		{
			bgColor = m_NoTeamColor;
		}

		bgColor[3] = 128 * newAlphaMultiplier;

		const char *pName = pi->IsConnected() ? pi->GetDisplayName(false) : "unknown";
		wchar_t szconverted[64];
		g_pVGuiLocalize->ConvertANSIToUnicode(pName, szconverted, sizeof(szconverted));

		// Draw the item background
		vgui2::surface()->DrawSetColor(bgColor);
		vgui2::surface()->DrawFilledRect(0, ypos, item_wide, ypos + item_tall);

		//=============================================================================
		// HPE_BEGIN:
		// [pfreese] Draw the avatar for the given player
		//=============================================================================

		// Draw the players icon
		if (show_avatar && m_SpeakingList[i].pAvatar)
		{
			m_SpeakingList[i].pAvatar->SetPos(avatar_xpos, ypos + avatar_ypos);
			m_SpeakingList[i].pAvatar->SetColor(COLOR_WHITE);
			m_SpeakingList[i].pAvatar->Paint();
		}

		//=============================================================================
		// HPE_END
		//=============================================================================

		// Draw the voice icon
		if (show_voice_icon && m_iVoiceIconTexture != -1)
		{
			vgui2::surface()->DrawSetTexture(m_iVoiceIconTexture);
			vgui2::surface()->DrawSetColor(COLOR_WHITE);
			vgui2::surface()->DrawTexturedRect(voice_icon_xpos, ypos + voice_icon_ypos,
			    voice_icon_xpos + voice_icon_wide, ypos + voice_icon_ypos + voice_icon_tall);
		}

		// Draw the player's name
		vgui2::surface()->DrawSetTextColor(COLOR_WHITE);
		vgui2::surface()->DrawSetTextPos(text_xpos, ypos + (item_tall / 2) - (iFontHeight / 2));

		int iTextSpace = item_wide - text_xpos;

		// write as much of the name as will fit, truncate the rest and add ellipses
		int iNameLength = wcslen(szconverted);
		const wchar_t *pszconverted = szconverted;
		int iTextWidthCounter = 0;
		for (int j = 0; j < iNameLength; j++)
		{
			iTextWidthCounter += vgui2::surface()->GetCharacterWidth(m_NameFont, pszconverted[j]);

			if (iTextWidthCounter > iTextSpace)
			{
				if (j > 3)
				{
					szconverted[j - 2] = '.';
					szconverted[j - 1] = '.';
					szconverted[j] = '\0';
				}
				break;
			}
		}

		vgui2::surface()->DrawPrintText(szconverted, wcslen(szconverted));

		ypos -= (item_spacing + item_tall);
	}
}

int CHudVoiceStatus::FindActiveSpeaker(int playerId)
{
	FOR_EACH_LL(m_SpeakingList, i)
	{
		if (m_SpeakingList[i].playerId == playerId)
			return i;
	}
	return m_SpeakingList.InvalidIndex();
}

void CHudVoiceStatus::ClearActiveList()
{
	FOR_EACH_LL(m_SpeakingList, i)
	{
		delete m_SpeakingList[i].pAvatar;
	}

	m_SpeakingList.RemoveAll();
}
