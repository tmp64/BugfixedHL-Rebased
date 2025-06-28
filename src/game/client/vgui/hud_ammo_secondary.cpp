#include <vgui/IPanel.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "hud.h"
#include "cl_util.h"
#include "client_steam_context.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "hud_ammo_secondary.h"
#include "hud/ammo.h"
#include "vgui/tga_image.h"

CHudAmmoSecondaryPanel::CHudAmmoSecondaryPanel()
    : BaseClass(nullptr, VIEWPORT_PANEL_HUD_AMMO_SEC)
{
	SetSize(100, 100); // Silence "parent not sized yet" warning
	SetProportional(true);

	m_pAmmoBgLabel = new vgui2::Label(this, "AmmoBg", "00");
	m_pAmmoLabelGlow = new vgui2::Label(this, "AmmoGlow", "00");
	m_pAmmoLabel = new vgui2::Label(this, "Ammo", "100");

	// Default icon
	m_pAmmoIcon = m_pAmmoIconDefault = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/default_ammo");

	// Ammo icons
	m_pAmmoGrenadeMP5 = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_grenade_mp5");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/HudAmmoSecondary.res");

	SetVisible(false);
}

void CHudAmmoSecondaryPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
}

void CHudAmmoSecondaryPanel::PaintBackground()
{
	DrawBox(0, 0, GetWide(), GetTall(), GetBgColor(), 1.0f);
	m_pAmmoIcon->SetPos(m_iAmmoIconX, m_iAmmoIconY);
	m_pAmmoIcon->SetSize(m_iAmmoIconWide, m_iAmmoIconTall);
	m_pAmmoIcon->SetColor(m_hudCurrentColor);
	m_pAmmoIcon->Paint();
}

enum AmmoType
{
	AMMO_TYPE_BUCKSHOT = 1,
	AMMO_TYPE_9MM = 2,
	AMMO_TYPE_GRENADE_MP5 = 3,
	AMMO_TYPE_357 = 4,
	AMMO_TYPE_ENERGY = 5,
	AMMO_TYPE_GRENADE_RPG = 6,
	AMMO_TYPE_BOLT = 7,
	AMMO_TYPE_GRENADE_TRIPMINE = 8,
	AMMO_TYPE_GRENADE_SATCHEL = 9,
	AMMO_TYPE_GRENADE_FRAG = 10,
	AMMO_TYPE_SNARK = 11,
	AMMO_TYPE_GRENADE_HORNET = 12
};

void CHudAmmoSecondaryPanel::UpdateAmmoSecondaryPanel(WEAPON *pWeapon, int maxClip, int ammo1, int ammo2)
{
	if (pWeapon)
	{
		if (m_iAmmoCount != ammo1 || m_iClip != pWeapon->iClip)
		{
			m_fFade = FADE_TIME; // Reset fade timer
		}

		m_iAmmoType = pWeapon->iAmmoType;
		m_iAmmo2Type = pWeapon->iAmmo2Type;
		m_iMax1 = pWeapon->iMax1;
		m_iMax2 = pWeapon->iMax2;
		m_iId = pWeapon->iId;
		m_iClip = pWeapon->iClip;
		m_iMaxClip = maxClip;
		m_iAmmoCount = ammo1;
		m_iAmmoCount2 = ammo2;

		// Set the ammo icon based on the ammo type
		switch (m_iAmmo2Type)
		{
		case AMMO_TYPE_GRENADE_MP5:
			m_pAmmoIcon = m_pAmmoGrenadeMP5;
			break;
		default:
			m_pAmmoIcon = m_pAmmoIconDefault;
			break;
		}
	}
}

const char *CHudAmmoSecondaryPanel::GetName()
{
	return VIEWPORT_PANEL_HUD_AMMO_SEC;
}

void CHudAmmoSecondaryPanel::Reset()
{
}

void CHudAmmoSecondaryPanel::OnThink()
{
	int r, g, b;
	// Does weapon have any ammo at all?
	if (m_iAmmoType > 0)
	{
		// Does this weapon have a clip?
		if (m_iClip >= 0)
		{
			// Get it from iClip
			gHUD.GetHudAmmoColor(m_iClip, m_iMaxClip, r, g, b);
		}
		else
		{
			// Get it from iMax1
			gHUD.GetHudAmmoColor(m_iAmmoCount, m_iMax1, r, g, b);
		}
	}

	// Does this weapon have a secondary ammo type?
	if (m_iAmmo2Type > 0)
	{
		if (m_iAmmoCount2 > 0)
			ShowPanel(true);
		else
			ShowPanel(false);
	}
	else
	{
		ShowPanel(false);
	}

	char buf[32];

	snprintf(buf, sizeof(buf), "%d", m_iAmmoCount2);
	m_pAmmoLabel->SetText(buf);
	m_pAmmoLabelGlow->SetText(buf);

	// Glow effect will be visible only when dimmed
	auto [a1, a2] = gHUD.GetHudDimAlphas(m_pHudDim.GetBool(), m_fFade, gHUD.m_flTimeDelta);

	if (!m_pHudDim.GetBool())
	{
		a2 = 255;
		a1 = 0;
	}

	m_hudCurrentColor = Color(r, g, b, a1);
	m_pAmmoLabelGlow->SetFgColor(m_hudCurrentColor);

	m_hudCurrentColor = Color(r, g, b, a2);
	m_pAmmoLabel->SetFgColor(m_hudCurrentColor);
}

void CHudAmmoSecondaryPanel::ShowPanel(bool state)
{
	if (state != IsVisible())
	{
		SetVisible(state);
	}
}

vgui2::VPANEL CHudAmmoSecondaryPanel::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CHudAmmoSecondaryPanel::IsVisible()
{
	return BaseClass::IsVisible();
}

void CHudAmmoSecondaryPanel::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}
