// Martin Webrant (BulliT)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "ag_countdown.h"
#include "ag_global.h"

DEFINE_HUD_ELEM(AgHudCountdown);

void AgHudCountdown::Init()
{
	HookMessage<&AgHudCountdown::MsgFunc_Countdown>("Countdown");

	m_iFlags = 0;
	m_btCountdown = -1;
}

void AgHudCountdown::VidInit()
{
}

void AgHudCountdown::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;
	m_btCountdown = -1;
}

void AgHudCountdown::Draw(float fTime)
{
	if (gHUD.m_iIntermission)
		return;

	char szText[128];

	int r, g, b, a;
	a = 255 * gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);

	if (m_btCountdown != 50)
	{
		int iWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
		//int iHeight = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).bottom - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).top;

		gHUD.DrawHudNumber(ScreenWidth / 2 - iWidth / 2, gHUD.m_scrinfo.iCharHeight * 10, DHN_DRAWZERO, m_btCountdown, r, g, b);
		if (0 != strlen(m_szPlayer1) && 0 != strlen(m_szPlayer2))
		{
			// Write arena text.
			sprintf(szText, "%s vs %s", m_szPlayer1, m_szPlayer2);
			AgDrawHudStringCentered(ScreenWidth / 2, gHUD.m_scrinfo.iCharHeight * 7, ScreenWidth, szText, r, g, b);
		}
		else
		{
			// Write match text.
			V_strcpy_safe(szText, "Match about to start");
			AgDrawHudStringCentered(ScreenWidth / 2, gHUD.m_scrinfo.iCharHeight * 7, ScreenWidth, szText, r, g, b);
		}
	}
	else
	{
		if (strlen(m_szPlayer1) != 0)
		{
			sprintf(szText, "Last round won by %s", m_szPlayer1);
			AgDrawHudStringCentered(ScreenWidth / 2, gHUD.m_scrinfo.iCharHeight * 7, ScreenWidth, szText, r, g, b);
		}
		else
		{
			V_strcpy_safe(szText, "Waiting for players to get ready");
			AgDrawHudStringCentered(ScreenWidth / 2, gHUD.m_scrinfo.iCharHeight * 7, ScreenWidth, szText, r, g, b);
		}
	}
}

int AgHudCountdown::MsgFunc_Countdown(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// Update data
	m_btCountdown = READ_BYTE();
	char btSound = READ_BYTE();
	V_strcpy_safe(m_szPlayer1, READ_STRING());
	V_strcpy_safe(m_szPlayer2, READ_STRING());

	if (m_btCountdown >= 0)
	{
		m_iFlags |= HUD_ACTIVE;

		if (btSound)
		{
			// Play countdown sound
			switch (m_btCountdown)
			{
			case 0:
				PlaySound("barney/ba_bring.wav", 1);
				break;
			case 1:
				PlaySound("fvox/one.wav", 1);
				break;
			case 2:
				PlaySound("fvox/two.wav", 1);
				break;
			case 3:
				PlaySound("fvox/three.wav", 1);
				break;
			case 4:
				PlaySound("fvox/four.wav", 1);
				break;
			case 5:
				PlaySound("fvox/five.wav", 1);
				break;
			case 6:
				PlaySound("fvox/six.wav", 1);
				break;
			case 7:
				PlaySound("fvox/seven.wav", 1);
				break;
			case 8:
				PlaySound("fvox/eight.wav", 1);
				break;
			case 9:
				PlaySound("fvox/nine.wav", 1);
				break;
			case 10:
				PlaySound("fvox/ten.wav", 1);
				break;
			default:
				break;
			}
		}
	}
	else
		m_iFlags &= ~HUD_ACTIVE;

	return 1;
}
