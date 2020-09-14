// Martin Webrant (BulliT)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "ag_longjump.h"
#include "ag_global.h"

DEFINE_HUD_ELEM(AgHudLongjump);

void AgHudLongjump::Init()
{
	HookMessage<&AgHudLongjump::MsgFunc_Longjump>("Longjump");

	m_iFlags = 0;
	m_flTurnoff = 0;
}

void AgHudLongjump::VidInit()
{
}

void AgHudLongjump::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;
}

void AgHudLongjump::Draw(float fTime)
{
	if (gHUD.m_flTime > m_flTurnoff || gHUD.m_iIntermission)
	{
		Reset();
		return;
	}

	char szText[32];

	int r, g, b, a;
	UnpackRGB(r, g, b, RGB_GREENISH);
	a = 255 * gHUD.GetHudTransparency();
	ScaleColors(r, g, b, a);

	sprintf(szText, "Longjump %d", (int)(m_flTurnoff - gHUD.m_flTime));
	AgDrawHudStringCentered(ScreenWidth / 2, gHUD.m_scrinfo.iCharHeight * 2, ScreenWidth, szText, r, g, b);
}

int AgHudLongjump::MsgFunc_Longjump(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int iTime = READ_BYTE();

	m_flTurnoff = gHUD.m_flTime + iTime;
	m_iFlags |= HUD_ACTIVE;

	return 1;
}
