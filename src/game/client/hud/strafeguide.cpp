#include <cmath>
#include <complex>
#include "hud.h"
#include "cl_util.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "strafeguide.h"

enum border
{
	RED_GREEN,
	GREEN_WHITE,
	WHITE_GREEN,
	GREEN_RED
};

ConVar hud_strafeguide("hud_strafeguide", "0", FCVAR_BHL_ARCHIVE, "Enable strafing HUD");
ConVar hud_strafeguide_zoom("hud_strafeguide_zoom", "1", FCVAR_BHL_ARCHIVE, "Changes zoom for the strafing HUD");
ConVar hud_strafeguide_height("hud_strafeguide_height", "0", FCVAR_BHL_ARCHIVE, "Changes height for the strafing HUD");
ConVar hud_strafeguide_size("hud_strafeguide_size", "0", FCVAR_BHL_ARCHIVE, "Changes size for the strafing HUD");

DEFINE_HUD_ELEM(CHudStrafeGuide);

void CHudStrafeGuide::Init()
{
	m_iFlags = HUD_ACTIVE | HUD_DRAW_ALWAYS;
}

void CHudStrafeGuide::VidInit()
{
}

void CHudStrafeGuide::Draw(float time)
{
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) || gEngfuncs.IsSpectateOnly())
		return;

	if (!hud_strafeguide.GetBool())
		return;

	double fov = default_fov.GetFloat() / 180 * M_PI / 2;
	double zoom = hud_strafeguide_zoom.GetFloat();

	int size = gHUD.m_iFontHeight;
	int height = ScreenHeight / 2 - 2 * size;

	if (hud_strafeguide_size.GetBool())
		size = hud_strafeguide_size.GetFloat();

	if (hud_strafeguide_height.GetBool())
		height = hud_strafeguide_height.GetFloat();

	for (int i = 0; i < 4; ++i)
	{
		int r, g, b;
		switch (i)
		{
		case RED_GREEN:
		case WHITE_GREEN:
			r = 0;
			g = 255;
			b = 0;
			break;
		case GREEN_WHITE:
			r = 255;
			g = 255;
			b = 255;
			break;
		case GREEN_RED:
			r = 255;
			g = 0;
			b = 0;
			break;
		}

		double boxLeftBase = -angles[i];
		double boxRightBase = -angles[(i + 1) % 4];

		if (std::abs(boxLeftBase - boxRightBase) < 1e-10)
			continue;
		if (boxLeftBase >= boxRightBase)
			boxRightBase += 2 * M_PI;
		if (std::abs(boxLeftBase - boxRightBase) < 1e-10)
			continue;

		for (int iCopy = -8; iCopy <= 8; ++iCopy)
		{
			double boxLeft = boxLeftBase + iCopy * 2 * M_PI;
			double boxRight = boxRightBase + iCopy * 2 * M_PI;
			boxLeft *= zoom;
			boxRight *= zoom;

			if (std::abs(boxLeft) > fov && std::abs(boxRight) > fov && boxRight * boxLeft > 0)
				continue;

			boxLeft = boxLeft > fov ? fov : boxLeft < -fov ? -fov
			                                               : boxLeft;
			boxRight = boxRight > fov ? fov : boxRight < -fov ? -fov
			                                                  : boxRight;

			boxLeft = std::tan(boxLeft) / std::tan(fov);
			boxRight = std::tan(boxRight) / std::tan(fov);

			int boxLeftI = boxLeft / 1 * ScreenWidth / 2;
			int boxRightI = boxRight / 1 * ScreenWidth / 2;
			boxLeftI += ScreenWidth / 2;
			boxRightI += ScreenWidth / 2;

			FillRGBA(boxLeftI, height, boxRightI - boxLeftI, size, r, g, b, 60);
		}
	}
}

static double angleReduce(double a)
{
	double tmp = std::fmod(a, 2 * M_PI);
	if (tmp < 0)
		tmp += 2 * M_PI;
	if (tmp > M_PI)
		tmp -= 2 * M_PI;
	return tmp;
}

void CHudStrafeGuide::Update(struct ref_params_s *pparams)
{
	double frameTime = pparams->frametime;
	auto input = std::complex<double>(pparams->cmd->forwardmove, pparams->cmd->sidemove);
	double viewAngle = pparams->viewangles[1] / 180 * M_PI;

	if (std::norm(input) == 0)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (i < 2)
				angles[i] = M_PI;
			else
				angles[i] = -M_PI;
		}
		return;
	}

	std::complex<double> velocity = lastSimvel;
	lastSimvel = std::complex<double>(pparams->simvel[0], pparams->simvel[1]);

	bool onground = pparams->onground;
	double accelCoeff = onground ? pparams->movevars->accelerate : pparams->movevars->airaccelerate;
	//TODO: grab the entity friction from somewhere. pparams->movevars->friction is sv_friction
	//just use the default 1 for now
	double frictionCoeff = 1;

	double inputAbs = std::abs(input);
	if (onground)
		inputAbs = std::min<double>(inputAbs, pparams->movevars->maxspeed);
	else
		inputAbs = std::min<double>(inputAbs, 30);

	input *= inputAbs / std::abs(input);

	double uncappedAccel = accelCoeff * frictionCoeff * inputAbs * frameTime;
	double velocityAbs = std::abs(velocity);

	if (uncappedAccel >= 2 * velocityAbs)
		angles[RED_GREEN] = M_PI;
	else
		angles[RED_GREEN] = std::acos(-uncappedAccel / velocityAbs / 2);

	if (velocityAbs <= inputAbs)
		angles[GREEN_WHITE] = 0;
	else
		angles[GREEN_WHITE] = std::acos(inputAbs / velocityAbs);

	angles[GREEN_RED] = -angles[RED_GREEN];
	angles[WHITE_GREEN] = -angles[GREEN_WHITE];

	double inputAngle = std::log(input).imag();
	double velocityAngle;

	if (velocityAbs == 0)
		velocityAngle = 0;
	else
		velocityAngle = std::log(velocity).imag();

	for (int i = 0; i < 4; ++i)
	{
		angles[i] += velocityAngle + inputAngle - viewAngle;
		angles[i] = angleReduce(angles[i]);
	}
}
