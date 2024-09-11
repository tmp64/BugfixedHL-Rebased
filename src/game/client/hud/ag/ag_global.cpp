// Martin Webrant (BulliT)

#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "com_weapons.h"
#include "parsemsg.h"
#include "ag_global.h"

int g_GameType = GT_STANDARD;

DEFINE_HUD_ELEM(AgHudGlobal);

void AgHudGlobal::Init()
{
	HookMessage<&AgHudGlobal::MsgFunc_PlaySound>("PlaySound");
	HookMessage<&AgHudGlobal::MsgFunc_CheatCheck>("CheatCheck");
	HookMessage<&AgHudGlobal::MsgFunc_WhString>("WhString");
	HookMessage<&AgHudGlobal::MsgFunc_SpikeCheck>("SpikeCheck");
	HookMessage<&AgHudGlobal::MsgFunc_Gametype>("Gametype");
	HookMessage<&AgHudGlobal::MsgFunc_AuthID>("AuthID");
	HookMessage<&AgHudGlobal::MsgFunc_MapList>("MapList");
	HookMessage<&AgHudGlobal::MsgFunc_CRC32>("CRC32");
	HookMessage<&AgHudGlobal::MsgFunc_Splash>("Splash");

	m_iFlags = 0;
}

void AgHudGlobal::VidInit()
{
}

void AgHudGlobal::Reset(void)
{
	m_iFlags |= HUD_ACTIVE;
}

void AgHudGlobal::Draw(float fTime)
{
}

int AgHudGlobal::MsgFunc_PlaySound(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	Vector origin;
	/*int iPlayer = */ READ_BYTE();
	for (int i = 0; i < 3; i++)
		origin[i] = READ_COORD();
	char *pszSound = READ_STRING();

	gEngfuncs.pfnPlaySoundByName(pszSound, 1);

	return 1;
}

int AgHudGlobal::MsgFunc_CheatCheck(const char *pszName, int iSize, void *pbuf)
{
	return 1;
}

int AgHudGlobal::MsgFunc_WhString(const char *pszName, int iSize, void *pbuf)
{
	return 1;
}

int AgHudGlobal::MsgFunc_SpikeCheck(const char *pszName, int iSize, void *pbuf)
{
	return 1;
}

int AgHudGlobal::MsgFunc_Gametype(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	g_GameType = READ_BYTE();
	return 1;
}

int AgHudGlobal::MsgFunc_AuthID(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int slot = READ_BYTE();
	char *steamid = READ_STRING();

	if (CPlayerInfo *pi = GetPlayerInfoSafe(slot))
	{
		if (!strncmp(steamid, "STEAM_", 6) || !strncmp(steamid, "VALVE_", 6))
			Q_strncpy(pi->m_szSteamID, steamid + 6, sizeof(pi->m_szSteamID)); // cutout "STEAM_" or "VALVE_" start of the string
		else
			Q_strncpy(pi->m_szSteamID, steamid, sizeof(pi->m_szSteamID));
	}

	return 1;
}

int AgHudGlobal::MsgFunc_MapList(const char *pszName, int iSize, void *pbuf)
{
	return 1;
}

int AgHudGlobal::MsgFunc_CRC32(const char *pszName, int iSize, void *pbuf)
{
	return 1;
}

int AgHudGlobal::MsgFunc_Splash(const char *pszName, int iSize, void *pbuf)
{
	return 1;
}

// Colors
int iNumConsoleColors = 16;
int arrConsoleColors[16][3] = {
	{ 255, 170, 0 }, // HL console (default)
	{ 255, 0, 0 }, // Red
	{ 0, 255, 0 }, // Green
	{ 255, 255, 0 }, // Yellow
	{ 0, 0, 255 }, // Blue
	{ 0, 255, 255 }, // Cyan
	{ 255, 0, 255 }, // Violet
	{ 136, 136, 136 }, // Q
	{ 255, 255, 255 }, // White
	{ 0, 0, 0 }, // Black
	{ 200, 90, 70 }, // Redb
	{ 145, 215, 140 }, // Green
	{ 225, 205, 45 }, // Yellow
	{ 125, 165, 210 }, // Blue
	{ 70, 70, 70 },
	{ 200, 200, 200 },
};

int GetColor(char cChar)
{
	int iColor = -1;
	if (cChar >= '0' && cChar <= '9')
		iColor = cChar - '0';
	return iColor;
}

int AgDrawHudString(int xpos, int ypos, int iMaxX, const char *szIt, int r, int g, int b)
{
	// calc center
	int iSizeX = 0;
	char *pszIt = (char *)szIt;
	for (; *pszIt != 0 && *pszIt != '\n'; pszIt++)
		iSizeX += gHUD.m_scrinfo.charWidths[*pszIt]; // variable-width fonts look cool

	int rx = r, gx = g, bx = b;

	pszIt = (char *)szIt;
	// draw the string until we hit the null character or a newline character
	for (; *pszIt != 0 && *pszIt != '\n'; pszIt++)
	{
		if (*pszIt == '^')
		{
			pszIt++;
			int iColor = GetColor(*pszIt);
			if (iColor < iNumConsoleColors && iColor >= 0)
			{
				if (0 >= iColor) // || 0 == g_pcl_show_colors->value)
				{
					rx = r;
					gx = g;
					bx = b;
				}
				else
				{
					rx = arrConsoleColors[iColor][0];
					gx = arrConsoleColors[iColor][1];
					bx = arrConsoleColors[iColor][2];
				}
				pszIt++;
				if (*pszIt == 0 || *pszIt == '\n')
					break;
			}
			else
				pszIt--;
		}

		int next = xpos + gHUD.m_scrinfo.charWidths[*pszIt]; // variable-width fonts look cool
		if (next > iMaxX)
			return xpos;
		TextMessageDrawChar(xpos, ypos, *pszIt, rx, gx, bx);
		xpos = next;
	}
	return xpos;
}

int AgDrawHudString(int xpos, int ypos, int iMaxX, const wchar_t *wszIt, int r, int g, int b)
{
	wchar_t *pwszIt = (wchar_t *)wszIt;
	int rx = r, gx = g, bx = b;

	// draw the string until we hit the null character or a newline character
	for (; *pwszIt != 0 && *pwszIt != '\n'; pwszIt++)
	{
		if (IsColorCode(pwszIt))
		{
			pwszIt++;
			int iColor = GetColor(*pwszIt);
			if (iColor < iNumConsoleColors && iColor >= 0)
			{
				if (0 >= iColor) // || 0 == g_pcl_show_colors->value)
				{
					rx = r;
					gx = g;
					bx = b;
				}
				else
				{
					rx = arrConsoleColors[iColor][0];
					gx = arrConsoleColors[iColor][1];
					bx = arrConsoleColors[iColor][2];
				}
				pwszIt++;
				if (*pwszIt == 0 || *pwszIt == '\n')
					break;
			}
			else
				pwszIt--;
		}

		int next = xpos + gHUD.GetHudCharWidth(*pwszIt);
		if (next > iMaxX)
			return xpos;

		TextMessageDrawChar(xpos, ypos, *pwszIt, rx, gx, bx);
		xpos = next;
	}

	return xpos;
}

int AgDrawHudStringCentered(int xpos, int ypos, int iMaxX, const char *szIt, int r, int g, int b)
{
	// calc center
	int iSizeX = 0;
	char *pszIt = (char *)szIt;
	for (; *pszIt != 0 && *pszIt != '\n'; pszIt++)
		iSizeX += gHUD.m_scrinfo.charWidths[*pszIt]; // variable-width fonts look cool

	// Subtract half sizex from xpos to center it.
	xpos = xpos - iSizeX / 2;

	int rx = r, gx = g, bx = b;

	pszIt = (char *)szIt;
	// draw the string until we hit the null character or a newline character
	for (; *pszIt != 0 && *pszIt != '\n'; pszIt++)
	{
		if (*pszIt == '^')
		{
			pszIt++;
			int iColor = GetColor(*pszIt);
			if (iColor < iNumConsoleColors && iColor >= 0)
			{
				if (0 >= iColor) // || 0 == g_pcl_show_colors->value)
				{
					rx = r;
					gx = g;
					bx = b;
				}
				else
				{
					rx = arrConsoleColors[iColor][0];
					gx = arrConsoleColors[iColor][1];
					bx = arrConsoleColors[iColor][2];
				}
				pszIt++;
				if (*pszIt == 0 || *pszIt == '\n')
					break;
			}
			else
				pszIt--;
		}

		int next = xpos + gHUD.m_scrinfo.charWidths[*pszIt]; // variable-width fonts look cool
		if (next > iMaxX)
			return xpos;
		TextMessageDrawChar(xpos, ypos, *pszIt, rx, gx, bx);
		xpos = next;
	}
	return xpos;
}
