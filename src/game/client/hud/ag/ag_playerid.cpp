// Martin Webrant (BulliT)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "ag_playerid.h"
#include "ag_global.h"

DEFINE_HUD_ELEM(AgHudPlayerId);

void AgHudPlayerId::Init()
{
	HookMessage<&AgHudPlayerId::MsgFunc_PlayerId>("PlayerId");

	m_iFlags = 0;
	m_flTurnoff = 0.0;
	m_iPlayer = 0;
	m_bTeam = false;
	m_iHealth = 0;
	m_iArmour = 0;

	m_pCvarHudPlayerId = gEngfuncs.pfnRegisterVariable("hud_playerid", "1", FCVAR_BHL_ARCHIVE);
}

void AgHudPlayerId::VidInit()
{
}

void AgHudPlayerId::Reset(void)
{
	m_iFlags &= ~HUD_ACTIVE;
	m_iPlayer = 0;
}

void AgHudPlayerId::Draw(float fTime)
{
	if (m_iPlayer <= 0 || m_pCvarHudPlayerId->value == 0)
		return;

	if (gHUD.m_flTime > m_flTurnoff)
	{
		Reset();
		return;
	}

	CPlayerInfo *pi = GetPlayerInfo(m_iPlayer)->Update();

	if (pi->IsConnected())
	{
		char szText[64];
		if (m_bTeam)
			sprintf(szText, "%s %d/%d", pi->GetDisplayName(), m_iHealth, m_iArmour);
		else
			sprintf(szText, "%s", pi->GetDisplayName());

		int r, g, b, a;
		if (m_bTeam)
			UnpackRGB(r, g, b, RGB_GREENISH);
		else
			UnpackRGB(r, g, b, RGB_REDISH);
		a = 255 * gHUD.GetHudTransparency();
		ScaleColors(r, g, b, a);

		if (CVAR_GET_FLOAT("hud_centerid"))
		{
			AgDrawHudStringCentered(ScreenWidth / 2, ScreenHeight - ScreenHeight / 4, ScreenWidth, szText, r, g, b);
		}
		else
		{
			if (gHUD.GetColorCodeAction() != ColorCodeAction::Ignore)
				RemoveColorCodesInPlace(szText);
			gHUD.DrawHudString(10, ScreenHeight - ScreenHeight / 8, 0, szText, r, g, b);
		}
	}
}

int AgHudPlayerId::MsgFunc_PlayerId(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iPlayer = READ_BYTE();
	m_bTeam = READ_BYTE() == 1;
	m_iHealth = READ_SHORT();
	m_iArmour = READ_SHORT();

	if (m_pCvarHudPlayerId->value == 0)
		m_iFlags &= ~HUD_ACTIVE;
	else
		m_iFlags |= HUD_ACTIVE;

	m_flTurnoff = gHUD.m_flTime + 2; // Hold for 2 seconds.

	return 1;
}
