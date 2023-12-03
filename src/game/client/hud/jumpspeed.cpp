#include <cmath>
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "jumpspeed.h"

extern ConVar hud_speedometer;
extern ConVar hud_speedometer_below_cross;
ConVar hud_jumpspeed("hud_jumpspeed", "0", FCVAR_BHL_ARCHIVE, "Enable jumpspeed");
ConVar hud_jumpspeed_below_cross("hud_jumpspeed_below_cross", "0", FCVAR_BHL_ARCHIVE, "Move jumpspeed to below the crosshair");

DEFINE_HUD_ELEM(CHudJumpspeed);

void CHudJumpspeed::Init()
{
	m_iFlags = HUD_ACTIVE | HUD_DRAW_ALWAYS;
}

void CHudJumpspeed::VidInit()
{
	passedTime = FADE_DURATION_JUMPSPEED;
}

void CHudJumpspeed::Draw(float flTime)
{
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) || gEngfuncs.IsSpectateOnly())
		return;

	if (!hud_jumpspeed.GetBool())
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
	if (!hud_jumpspeed_below_cross.GetBool())
	{
		y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;
	}
	else
	{
		int yoffset = 0;

		if (hud_speedometer.GetBool() && hud_speedometer_below_cross.GetBool())
			yoffset = gHUD.m_iFontHeight;

		y = ScreenHeight / 2 + gHUD.m_iFontHeight + yoffset;
	}

	if (hud_draw.GetFloat() > 0)
		a *= gHUD.GetHudTransparency();
	else
		a *= 1;
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);

	// Can be negative if we went back in time (for example, loaded a save).
	double timeDelta = std::fmax(flTime - lastTime, 0.0f);
	passedTime += timeDelta;

	// Check for Inf, NaN, etc.
	if (passedTime > FADE_DURATION_JUMPSPEED || !std::isnormal(passedTime))
		passedTime = FADE_DURATION_JUMPSPEED;

	float colorVel[3] = { r - fadingFrom[0] / FADE_DURATION_JUMPSPEED,
		g - fadingFrom[1] / FADE_DURATION_JUMPSPEED,
		b - fadingFrom[2] / FADE_DURATION_JUMPSPEED };

	r = static_cast<int>(r - colorVel[0] * (FADE_DURATION_JUMPSPEED - passedTime));
	g = static_cast<int>(g - colorVel[1] * (FADE_DURATION_JUMPSPEED - passedTime));
	b = static_cast<int>(b - colorVel[2] * (FADE_DURATION_JUMPSPEED - passedTime));

	lastTime = flTime;
	gHUD.DrawHudNumberCentered(ScreenWidth / 2, y, m_iSpeed, r, g, b);

	m_iOldSpeed = m_iSpeed;
}

void CHudJumpspeed::UpdateSpeed(const float velocity[3])
{
	if (FADE_DURATION_JUMPSPEED > 0.0f)
	{
		if ((velocity[2] != 0.0f && prevVel[2] == 0.0f)
		    || (velocity[2] > 0.0f && prevVel[2] < 0.0f))
		{
			double difference = std::hypot(velocity[0], velocity[1]) - m_iSpeed;
			if (difference != 0.0f)
			{
				if (difference > 0.0f)
				{
					fadingFrom[0] = 0;
					fadingFrom[1] = 255;
					fadingFrom[2] = 0;
				}
				else
				{
					fadingFrom[0] = 255;
					fadingFrom[1] = 0;
					fadingFrom[2] = 0;
				}

				passedTime = 0.0;
				m_iSpeed = std::hypot(velocity[0], velocity[1]);
			}
		}
	}
	VectorCopy(velocity, prevVel);
}