#include <cmath>
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "speedometer.h"

static ConVar hud_speedometer("hud_speedometer", "0", FCVAR_BHL_ARCHIVE, "Enable speedometer");
static ConVar hud_speedometer_below_cross("hud_speedometer_below_cross", "0", FCVAR_BHL_ARCHIVE, "Move speedometer to below the crosshair");

DEFINE_HUD_ELEM(CHudSpeedometer);

void CHudSpeedometer::Init()
{
	m_iFlags = HUD_ACTIVE;
}

void CHudSpeedometer::VidInit()
{
}

void CHudSpeedometer::Draw(float time)
{
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) || gEngfuncs.IsSpectateOnly())
		return;

	if (!hud_speedometer.GetBool())
		return;

	if (m_iOldSpeed != m_iSpeed)
		m_fFade = FADE_TIME;

	int r, g, b;
	float a;

	if (!hud_dim.GetBool())
		a = MIN_ALPHA + ALPHA_AMMO_MAX;
	else if (m_fFade > 0)
	{
		// Fade the number back to dim
		m_fFade -= (gHUD.m_flTimeDelta * 20);
		if (m_fFade <= 0)
			m_fFade = 0;
		a = MIN_ALPHA + (m_fFade / FADE_TIME) * ALPHA_AMMO_FLASH;
	}
	else
		a = MIN_ALPHA;

	int y;
	if (hud_speedometer_below_cross.GetBool())
		y = ScreenHeight / 2 + gHUD.m_iFontHeight / 2;
	else
		y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;

	a *= gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);
	gHUD.DrawHudNumberCentered(ScreenWidth / 2, y, m_iSpeed, r, g, b);

	m_iOldSpeed = m_iSpeed;
}

void CHudSpeedometer::UpdateSpeed(const float velocity[2])
{
	m_iSpeed = std::round(std::hypot(velocity[0], velocity[1]));
}
