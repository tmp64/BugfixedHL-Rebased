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
// Message.cpp
//
// implementation of CHudMessage class
//

#include <string.h>
#include <stdio.h>
#include <tier1/strtools.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "message.h"
#include "timer.h"

// 1 Global client_textmessage_t for custom messages that aren't in the titles.txt
constexpr int MAX_MESSAGE_TEXT_LENGTH = 1024;
client_textmessage_t g_pCustomMessage;
char *g_pCustomName = "Custom";
char g_pCustomText[MAX_MESSAGE_TEXT_LENGTH];

ConVar hud_message_draw_always("hud_message_draw_always", "0", FCVAR_BHL_ARCHIVE, "Display the server messages even when hud_draw is 0. Useful when recording HLKZ movies.");

DEFINE_HUD_ELEM(CHudMessage);

void CHudMessage::Init(void)
{
	BaseHudClass::Init();

	HookMessage<&CHudMessage::MsgFunc_HudText>("HudText");
	HookMessage<&CHudMessage::MsgFunc_GameTitle>("GameTitle");

	Reset();
};

void CHudMessage::VidInit(void)
{
	m_HUD_title_half = gHUD.GetSpriteIndex("title_half");
	m_HUD_title_life = gHUD.GetSpriteIndex("title_life");
};

void CHudMessage::Reset()
{
	memset(m_pMessages, 0, sizeof(m_pMessages[0]) * MAX_HUD_MESSAGES);
	memset(m_startTime, 0, sizeof(m_startTime[0]) * MAX_HUD_MESSAGES);
	for (int i = 0; i < MAX_HUD_MESSAGES; i++)
		m_sMessageStrings[i].clear();

	m_gameTitleTime = 0;
	m_pGameTitle = NULL;
	m_bEndAfterMessage = false;
}

void CHudMessage::CStrToWide(const char *pString, std::wstring &wstr)
{
	wchar_t wTextBuf[MAX_MESSAGE_TEXT_LENGTH];
	Q_UTF8ToWString(pString, wTextBuf, sizeof(wTextBuf), STRINGCONVERT_REPLACE);
	wstr = wTextBuf;
}

float CHudMessage::FadeBlend(float fadein, float fadeout, float hold, float localTime)
{
	float fadeTime = fadein + hold;
	float fadeBlend;

	if (localTime < 0)
		return 0;

	if (localTime < fadein)
	{
		fadeBlend = 1 - ((fadein - localTime) / fadein);
	}
	else if (localTime > fadeTime)
	{
		if (fadeout > 0)
			fadeBlend = 1 - ((localTime - fadeTime) / fadeout);
		else
			fadeBlend = 0;
	}
	else
		fadeBlend = 1;

	return fadeBlend;
}

int CHudMessage::XPosition(float x, int width, int totalWidth)
{
	int xPos;

	if (x == -1)
	{
		xPos = (ScreenWidth - width) / 2;
	}
	else
	{
		if (x < 0)
			xPos = (1.0 + x) * ScreenWidth - totalWidth; // Alight right
		else
			xPos = x * ScreenWidth;
	}

	if (xPos + width > ScreenWidth)
		xPos = ScreenWidth - width;
	else if (xPos < 0)
		xPos = 0;

	return xPos;
}

int CHudMessage::YPosition(float y, int height)
{
	int yPos;

	if (y == -1) // Centered?
		yPos = (ScreenHeight - height) * 0.5;
	else
	{
		// Alight bottom?
		if (y < 0)
			yPos = (1.0 + y) * ScreenHeight - height; // Alight bottom
		else // align top
			yPos = y * ScreenHeight;
	}

	if (yPos + height > ScreenHeight)
		yPos = ScreenHeight - height;
	else if (yPos < 0)
		yPos = 0;

	return yPos;
}

void CHudMessage::MessageScanNextChar(Color srcColor)
{
	int srcRed = 0, srcGreen = 0, srcBlue = 0, destRed = 0, destGreen = 0, destBlue = 0;
	int blend = 0;

	srcRed = srcColor.r();
	srcGreen = srcColor.g();
	srcBlue = srcColor.b();
	destRed = destGreen = destBlue = 0;
	blend = 0; // Pure source

	if (gHUD.m_Rainbow.IsEnabled())
		gHUD.m_Rainbow.GetRainbowColor(m_parms.x, m_parms.y, srcRed, srcGreen, srcBlue);

	switch (m_parms.pMessage->effect)
	{
	// Fade-in / Fade-out
	case 0:
	case 1:
		blend = m_parms.fadeBlend;
		break;

	case 2:
		m_parms.charTime += m_parms.pMessage->fadein;
		if (m_parms.charTime > m_parms.time)
		{
			srcRed = srcGreen = srcBlue = 0;
			blend = 0; // pure source
		}
		else
		{
			float deltaTime = m_parms.time - m_parms.charTime;

			if (m_parms.time > m_parms.fadeTime)
			{
				blend = m_parms.fadeBlend;
			}
			else if (deltaTime > m_parms.pMessage->fxtime)
				blend = 0; // pure dest
			else
			{
				destRed = m_parms.pMessage->r2;
				destGreen = m_parms.pMessage->g2;
				destBlue = m_parms.pMessage->b2;
				blend = 255 - (deltaTime * (1.0 / m_parms.pMessage->fxtime) * 255.0 + 0.5);
			}
		}
		break;
	}
	if (blend > 255)
		blend = 255;
	else if (blend < 0)
		blend = 0;

	m_parms.r = ((srcRed * (255 - blend)) + (destRed * blend)) >> 8;
	m_parms.g = ((srcGreen * (255 - blend)) + (destGreen * blend)) >> 8;
	m_parms.b = ((srcBlue * (255 - blend)) + (destBlue * blend)) >> 8;

	if (m_parms.pMessage->effect == 1 && m_parms.charTime != 0)
	{
		if (m_parms.x >= 0 && m_parms.y >= 0 && (m_parms.x + gHUD.GetHudCharWidth(m_parms.currentChar)) <= ScreenWidth)
			TextMessageDrawChar(m_parms.x, m_parms.y, m_parms.currentChar, m_parms.pMessage->r2, m_parms.pMessage->g2, m_parms.pMessage->b2);
	}
}

void CHudMessage::MessageScanStart(void)
{
	switch (m_parms.pMessage->effect)
	{
	// Fade-in / out with flicker
	case 0:
	case 1:
		m_parms.fadeTime = m_parms.pMessage->fadein + m_parms.pMessage->holdtime;

		if (m_parms.time < m_parms.pMessage->fadein)
		{
			m_parms.fadeBlend = ((m_parms.pMessage->fadein - m_parms.time) * (1.0 / m_parms.pMessage->fadein) * 255);
		}
		else if (m_parms.time > m_parms.fadeTime)
		{
			if (m_parms.pMessage->fadeout > 0)
				m_parms.fadeBlend = (((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255);
			else
				m_parms.fadeBlend = 255; // Pure dest (off)
		}
		else
			m_parms.fadeBlend = 0; // Pure source (on)
		m_parms.charTime = 0;

		if (m_parms.pMessage->effect == 1 && (rand() % 100) < 10)
			m_parms.charTime = 1;
		break;

	case 2:
		m_parms.fadeTime = (m_parms.pMessage->fadein * m_parms.length) + m_parms.pMessage->holdtime;

		if (m_parms.time > m_parms.fadeTime && m_parms.pMessage->fadeout > 0)
			m_parms.fadeBlend = (((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255);
		else
			m_parms.fadeBlend = 0;
		break;
	}
}

void CHudMessage::MessageDrawScan(client_textmessage_t *pMessage, float time, const std::wstring &wstr)
{
	int i, j, width;
	wchar_t wLine[MAX_HUD_STRING + 1];

	// Alpha value is used as a boolean.
	// 1 means that this is the first char with new color
	Color lineColor[MAX_HUD_STRING + 1];

	const wchar_t *wText = wstr.c_str();
	const wchar_t *pwText;
	int lineHeight = gHUD.m_scrinfo.iCharHeight + ADJUST_MESSAGE;
	ColorCodeAction nColorMode = gHUD.GetColorCodeAction();

	// Count lines and width
	m_parms.time = time;
	m_parms.charTime = 0;
	m_parms.pMessage = pMessage;
	m_parms.lines = 1;
	m_parms.length = 0;
	m_parms.totalWidth = 0;
	width = 0;
	pwText = wText;
	while (*pwText)
	{
		if (nColorMode != ColorCodeAction::Ignore && IsColorCode(pwText))
		{
			pwText += 1;
		}
		else if (*pwText == '\n')
		{
			m_parms.lines++;
			if (width > m_parms.totalWidth)
				m_parms.totalWidth = width;
			width = 0;
		}
		else
		{
			int c = (int)*pwText;
			width += gHUD.GetHudCharWidth(c);
		}
		pwText++;
		m_parms.length++;
	}
	m_parms.totalHeight = m_parms.lines * (lineHeight);
	m_parms.y = YPosition(pMessage->y, m_parms.totalHeight);

	MessageScanStart();

	pwText = wText;
	for (i = 0; i < m_parms.lines; i++)
	{
		m_parms.lineLength = 0;
		m_parms.width = 0;
		memset(lineColor, 0, sizeof(lineColor));
		lineColor[0] = Color(pMessage->r1, pMessage->g1, pMessage->b1, 255);
		while (*pwText && *pwText != '\n')
		{
			int c = *pwText;
			if (m_parms.lineLength < MAX_HUD_STRING)
			{
				if (nColorMode != ColorCodeAction::Ignore && IsColorCode(pwText))
				{
					int coloridx = *(pwText + 1) - L'0';
					if (coloridx == 0 || coloridx == 9 || nColorMode == ColorCodeAction::Strip)
					{
						// Reset
						lineColor[m_parms.lineLength] = Color(pMessage->r1, pMessage->g1, pMessage->b1, 255);
					}
					else
					{
						lineColor[m_parms.lineLength] = gHUD.GetColorCodeColor(coloridx);
					}
					lineColor[m_parms.lineLength][3] = 1;
					pwText += 2;
					continue;
				}
				else
				{
					wLine[m_parms.lineLength] = c;
					Color &color = lineColor[m_parms.lineLength];
					if (m_parms.lineLength > 0 && !color.a())
						color = lineColor[m_parms.lineLength - 1];
					m_parms.width += gHUD.GetHudCharWidth(c);
					m_parms.lineLength++;
				}
			}
			pwText++;
		}
		pwText++; // Skip LF
		wLine[m_parms.lineLength] = 0;

		m_parms.x = XPosition(pMessage->x, m_parms.width, m_parms.totalWidth);

		for (j = 0; j < m_parms.lineLength; j++)
		{
			m_parms.currentChar = wLine[j];
			int nextX = m_parms.x + gHUD.GetHudCharWidth(m_parms.currentChar);
			MessageScanNextChar(lineColor[j]);

			if (m_parms.x >= 0 && m_parms.y >= 0 && nextX <= ScreenWidth)
				TextMessageDrawChar(m_parms.x, m_parms.y, m_parms.currentChar, m_parms.r, m_parms.g, m_parms.b);
			m_parms.x = nextX;
		}

		m_parms.y += lineHeight;
	}
}

void CHudMessage::Draw(float fTime)
{
	int i, drawn;
	client_textmessage_t *pMessage;
	float endTime = 0;

	drawn = 0;

	if (m_gameTitleTime > 0)
	{
		float localTime = gHUD.m_flTime - m_gameTitleTime;
		float brightness;

		// Maybe timer isn't set yet
		if (m_gameTitleTime > gHUD.m_flTime)
			m_gameTitleTime = gHUD.m_flTime;

		if (localTime > (m_pGameTitle->fadein + m_pGameTitle->holdtime + m_pGameTitle->fadeout))
			m_gameTitleTime = 0;
		else
		{
			brightness = FadeBlend(m_pGameTitle->fadein, m_pGameTitle->fadeout, m_pGameTitle->holdtime, localTime);

			int halfWidth = gHUD.GetSpriteRect(m_HUD_title_half).right - gHUD.GetSpriteRect(m_HUD_title_half).left;
			int fullWidth = halfWidth + gHUD.GetSpriteRect(m_HUD_title_life).right - gHUD.GetSpriteRect(m_HUD_title_life).left;
			int fullHeight = gHUD.GetSpriteRect(m_HUD_title_half).bottom - gHUD.GetSpriteRect(m_HUD_title_half).top;

			int x = XPosition(m_pGameTitle->x, fullWidth, fullWidth);
			int y = YPosition(m_pGameTitle->y, fullHeight);

			SPR_Set(gHUD.GetSprite(m_HUD_title_half), brightness * m_pGameTitle->r1, brightness * m_pGameTitle->g1, brightness * m_pGameTitle->b1);
			SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_title_half));

			SPR_Set(gHUD.GetSprite(m_HUD_title_life), brightness * m_pGameTitle->r1, brightness * m_pGameTitle->g1, brightness * m_pGameTitle->b1);
			SPR_DrawAdditive(0, x + halfWidth, y, &gHUD.GetSpriteRect(m_HUD_title_life));

			drawn = 1;
		}
	}
	// Fixup level transitions
	for (i = 0; i < MAX_HUD_MESSAGES; i++)
	{
		// Assume m_parms.time contains last time
		if (m_pMessages[i])
		{
			pMessage = m_pMessages[i];
			if (m_startTime[i] > gHUD.m_flTime)
				m_startTime[i] = gHUD.m_flTime + m_parms.time - m_startTime[i] + 0.2; // Server takes 0.2 seconds to spawn, adjust for this
		}
	}

	for (i = 0; i < MAX_HUD_MESSAGES; i++)
	{
		if (!m_pMessages[i])
			continue;

		pMessage = m_pMessages[i];

		// Filter out MiniAG timer that passed before we detected server AG version
		if (CHudTimer::Get()->GetAgVersion() == CHudTimer::SV_AG_MINI && (fabs(pMessage->y - 0.01) < 0.0002f && fabs(pMessage->x - 0.5) < 0.0002f || // Original MiniAG coordinates
		        fabs(pMessage->y - 0.01) < 0.0002f && fabs(pMessage->x + 1) < 0.0002f // Russian Crossfire MiniAG coordinates
		        ))
		{
			// TODO: Additional checks on text in the message...
			m_pMessages[i] = NULL;
			continue;
		}

		// This is when the message is over
		switch (pMessage->effect)
		{
		case 0:
		case 1:
			endTime = m_startTime[i] + pMessage->fadein + pMessage->fadeout + pMessage->holdtime;
			break;

		// Fade in is per character in scanning messages
		case 2:
			endTime = m_startTime[i] + (pMessage->fadein * strlen(pMessage->pMessage)) + pMessage->fadeout + pMessage->holdtime;
			break;
		}

		if (fTime <= endTime)
		{
			float messageTime = fTime - m_startTime[i];

			// Draw the message
			// effect 0 is fade in/fade out
			// effect 1 is flickery credits
			// effect 2 is write out (training room)
			MessageDrawScan(pMessage, messageTime, m_sMessageStrings[i]);

			drawn++;
		}
		else
		{
			// The message is over
			m_pMessages[i] = NULL;
			m_sMessageStrings[i].clear();

			if (m_bEndAfterMessage)
			{
				// leave game
				gEngfuncs.pfnClientCmd("wait\nwait\nwait\nwait\nwait\nwait\nwait\ndisconnect\n");
			}
		}
	}

	// Remember the time -- to fix up level transitions
	m_parms.time = gHUD.m_flTime;
	// Don't call until we get another message
	if (!drawn)
		m_iFlags &= ~HUD_ACTIVE;
}

void CHudMessage::Think()
{
	if (hud_message_draw_always.GetBool())
		m_iFlags |= HUD_DRAW_ALWAYS;
	else
		m_iFlags &= ~HUD_DRAW_ALWAYS;
}

void CHudMessage::MessageAdd(const char *pName, float time)
{
	int i, j;
	client_textmessage_t *tempMessage;

	for (i = 0; i < MAX_HUD_MESSAGES; i++)
	{
		if (m_pMessages[i])
			continue;

		// Trim off a leading # if it's there
		if (pName[0] == '#')
			tempMessage = TextMessageGet(pName + 1);
		else
			tempMessage = TextMessageGet(pName);
		// If we couldnt find it in the titles.txt or server's received messages, just create it
		if (!tempMessage)
		{
			g_pCustomMessage.effect = 2;
			g_pCustomMessage.r1 = g_pCustomMessage.g1 = g_pCustomMessage.b1 = g_pCustomMessage.a1 = 100;
			g_pCustomMessage.r2 = 240;
			g_pCustomMessage.g2 = 110;
			g_pCustomMessage.b2 = 0;
			g_pCustomMessage.a2 = 0;
			g_pCustomMessage.x = -1; // Centered
			g_pCustomMessage.y = 0.7;
			g_pCustomMessage.fadein = 0.01;
			g_pCustomMessage.fadeout = 1.5;
			g_pCustomMessage.fxtime = 0.25;
			g_pCustomMessage.holdtime = 5;
			g_pCustomMessage.pName = g_pCustomName;
			g_pCustomMessage.pMessage = g_pCustomText;
			V_strcpy_safe(g_pCustomText, pName);

			tempMessage = &g_pCustomMessage;
		}

		// Filter out MiniAG timer
		if (CHudTimer::Get()->GetAgVersion() == CHudTimer::SV_AG_MINI && (fabs(tempMessage->y - 0.01) < 0.0002f && fabs(tempMessage->x - 0.5) < 0.0002f || // Original MiniAG coordinates
		        fabs(tempMessage->y - 0.01) < 0.0002f && fabs(tempMessage->x + 1) < 0.0002f // Russian Crossfire MiniAG coordinates
		        ))
		{
			// TODO: Additional checks on text in the message...
			return;
		}

		for (j = 0; j < MAX_HUD_MESSAGES; j++)
		{
			if (!m_pMessages[j])
				continue;

			// is this message already in the list
			if (!strcmp(tempMessage->pMessage, m_pMessages[j]->pMessage))
			{
				// Convert the string to std::wstring
				CStrToWide(m_pMessages[j]->pMessage, m_sMessageStrings[j]);
				return;
			}

			// get rid of any other messages in same location (only one displays at a time)
			if (fabs(tempMessage->y - m_pMessages[j]->y) < 0.0002)
			{
				if (fabs(tempMessage->x - m_pMessages[j]->x) < 0.0002)
				{
					m_pMessages[j] = NULL;
					m_sMessageStrings[j].clear();
				}
			}
		}

		// Convert the string to std::wstring
		CStrToWide(tempMessage->pMessage, m_sMessageStrings[i]);

		m_pMessages[i] = tempMessage;
		m_startTime[i] = time;
		return;
	}
}

int CHudMessage::MsgFunc_HudText(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	char *pString = READ_STRING();

	bool bIsEnding = false;
	constexpr std::string_view HL1_ENDING_STR = "END3";

	if (pString == HL1_ENDING_STR)
	{
		m_bEndAfterMessage = true;
	}

	MessageAdd(pString, gHUD.m_flTime);
	// Remember the time -- to fix up level transitions
	m_parms.time = gHUD.m_flTime;

	// Turn on drawing
	if (!(m_iFlags & HUD_ACTIVE))
		m_iFlags |= HUD_ACTIVE;

	return 1;
}

int CHudMessage::MsgFunc_GameTitle(const char *pszName, int iSize, void *pbuf)
{
	m_pGameTitle = TextMessageGet("GAMETITLE");
	if (m_pGameTitle != NULL)
	{
		m_gameTitleTime = gHUD.m_flTime;

		// Turn on drawing
		if (!(m_iFlags & HUD_ACTIVE))
			m_iFlags |= HUD_ACTIVE;
	}

	return 1;
}

void CHudMessage::MessageAdd(client_textmessage_t *newMessage)
{
	m_parms.time = gHUD.m_flTime;

	// Turn on drawing
	if (!(m_iFlags & HUD_ACTIVE))
		m_iFlags |= HUD_ACTIVE;

	for (int i = 0; i < MAX_HUD_MESSAGES; i++)
	{
		if (!m_pMessages[i])
		{
			m_pMessages[i] = newMessage;
			m_startTime[i] = gHUD.m_flTime;
			CStrToWide(newMessage->pMessage, m_sMessageStrings[i]);
			return;
		}
	}
}
