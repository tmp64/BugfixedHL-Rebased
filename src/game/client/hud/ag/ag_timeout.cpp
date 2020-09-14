// Martin Webrant (BulliT)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "ag_timeout.h"
#include "ag_global.h"

DEFINE_HUD_ELEM(AgHudTimeout);

void AgHudTimeout::Init()
{
	HookMessage<&AgHudTimeout::MsgFunc_Timeout>("Timeout");

	m_iFlags = 0;
	m_State = Inactive;
	m_iTime = 0;
}

void AgHudTimeout::VidInit()
{
	m_State = Inactive;
}

void AgHudTimeout::Reset(void)
{
	m_iFlags &= ~HUD_ACTIVE;
}

void AgHudTimeout::Draw(float fTime)
{
	if (Inactive == m_State)
	{
		Reset();
		return;
	}

	char szText[64];
	szText[0] = '\0';

	int r, g, b, a;
	UnpackRGB(r, g, b, RGB_GREENISH);
	a = 255 * gHUD.GetHudTransparency();
	ScaleColors(r, g, b, a);

	if (Called == m_State)
		sprintf(szText, "Timeout called, stopping in %d seconds.", m_iTime);
	else if (Countdown == m_State)
		sprintf(szText, "Timeout, starting in %d seconds.", m_iTime);
	else
		return;

	AgDrawHudStringCentered(ScreenWidth / 2, gHUD.m_scrinfo.iCharHeight * 6, ScreenWidth, szText, r, g, b);
}

int AgHudTimeout::MsgFunc_Timeout(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_State = READ_BYTE();
	m_iTime = READ_BYTE();
	m_iFlags |= HUD_ACTIVE;

	return 1;
}
