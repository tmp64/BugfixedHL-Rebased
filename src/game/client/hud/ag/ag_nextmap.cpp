// Martin Webrant (BulliT)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "hud/timer.h"
#include "ag_nextmap.h"
#include "ag_global.h"

DEFINE_HUD_ELEM(AgHudNextmap);

void AgHudNextmap::Init()
{
	HookMessage<&AgHudNextmap::MsgFunc_Nextmap>("Nextmap");

	m_iFlags = 0;
	m_szNextmap[0] = '\0';
	m_flTurnoff = 0;
}

void AgHudNextmap::VidInit()
{
}

void AgHudNextmap::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;
}

void AgHudNextmap::Draw(float fTime)
{
	if (gHUD.m_flTime > m_flTurnoff)
	{
		Reset();
		return;
	}

	char szText[64];

	int r, g, b, a;
	a = 255 * gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);

	sprintf(szText, "Nextmap is %s", m_szNextmap);
	AgDrawHudStringCentered(ScreenWidth / 2, gHUD.m_scrinfo.iCharHeight * 5, ScreenWidth, szText, r, g, b);
}

int AgHudNextmap::MsgFunc_Nextmap(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	V_strcpy_safe(m_szNextmap, READ_STRING());

	CHudTimer::Get()->SetNextmap(m_szNextmap);

	const int hud_nextmap = (int)CHudTimer::Get()->GetHudNextmap();
	if (hud_nextmap != 2 && hud_nextmap != 1)
	{
		m_flTurnoff = gHUD.m_flTime + 10; // Display for 10 seconds.
		m_iFlags |= HUD_ACTIVE;
	}

	return 1;
}
