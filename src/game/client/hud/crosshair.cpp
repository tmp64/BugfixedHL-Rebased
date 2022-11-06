#include "crosshair.h"
#include "ammo.h"
#include "hud.h"
#include "cl_util.h"

static ConVar cl_cross_enable("cl_cross_enable", "1", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_red("cl_cross_red", "0", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_green("cl_cross_green", "255", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_blue("cl_cross_blue", "255", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_gap("cl_cross_gap", "4", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_size("cl_cross_size", "6", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_thickness("cl_cross_thickness", "2", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_outline_thickness("cl_cross_outline_thickness", "0", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_dot("cl_cross_dot", "0", FCVAR_BHL_ARCHIVE);
static ConVar cl_cross_t("cl_cross_t", "0", FCVAR_BHL_ARCHIVE);
ConVar cl_cross_zoom("cl_cross_zoom", "2", FCVAR_BHL_ARCHIVE, "Zoomed crosshair mode: sprite, custom, both");

DEFINE_HUD_ELEM(CHudCrosshair);

CHudCrosshair::CHudCrosshair()
{
}

void CHudCrosshair::Init()
{
	m_iFlags |= HUD_ACTIVE | HUD_DRAW_ALWAYS;
}

void CHudCrosshair::Draw(float flTime)
{
	if (!(gHUD.m_iWeaponBits & (1 << (WEAPON_SUIT))))
		return;

	if (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL)
		return;

	if (!(m_iFlags & HUD_ACTIVE))
		return;

	if (gEngfuncs.GetMaxClients() == 1)
	{
		// These checks are limited to singleplayer since HL servers
		// reset the HUD incorrectly on full update (e.g. when starting a demo recording).
		// This causes ammo count and crosshair to disappear.
		if ((gHUD.m_iHideHUDDisplay & HIDEHUD_WEAPONS) || !CHudAmmo::Get()->m_pWeapon)
			return;
	}

	bool isZoomed = gHUD.m_iFOV < 90;
	bool shouldDrawZoomed = cl_cross_zoom.GetInt() == 1 || cl_cross_zoom.GetInt() == 2;

	// Draw custom crosshair if enabled
	if (cl_cross_enable.GetBool() && !(CHudAmmo::Get()->m_fOnTarget && CHudAmmo::Get()->m_pWeapon->hAutoaim) && (!isZoomed || shouldDrawZoomed))
	{
		CrosshairSettings settings;
		settings.color = Color(cl_cross_red.GetInt(), cl_cross_green.GetInt(), cl_cross_blue.GetInt(), 255);
		settings.gap = cl_cross_gap.GetInt();
		settings.thickness = cl_cross_thickness.GetInt();
		settings.outline = cl_cross_outline_thickness.GetInt();
		settings.size = cl_cross_size.GetInt();
		settings.dot = cl_cross_dot.GetBool();
		settings.t = cl_cross_t.GetBool();
		m_Img.SetPos(0, 0);
		m_Img.SetSize(ScreenWidth, ScreenHeight);
		m_Img.SetSettings(settings);
		m_Img.Paint();
	}
}

bool CHudCrosshair::IsEnabled()
{
	return cl_cross_enable.GetBool();
}
