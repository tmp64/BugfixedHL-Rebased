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
#include "hud_ammo.h"
#include "hud/ammo.h"
#include "vgui/tga_image.h"

CHudAmmoPanel::CHudAmmoPanel()
    : BaseClass(nullptr, VIEWPORT_PANEL_HUD_AMMO)
{
	SetSize(100, 100); // Silence "parent not sized yet" warning
	SetProportional(true);

	m_pDigitRightBgLabel = new vgui2::Label(this, "AmmoBg", "000");
	m_pDigitRightLabelGlow = new vgui2::Label(this, "AmmoGlow", "000");
	m_pDigitRightLabel = new vgui2::Label(this, "Ammo", "100");
	m_pDigitLeftBgLabel = new vgui2::Label(this, "ClipBg", "000");
	m_pDigitLeftLabelGlow = new vgui2::Label(this, "ClipGlow", "000");
	m_pDigitLeftLabel = new vgui2::Label(this, "Clip", "100");

	// Default icon
	m_pAmmoIcon = m_pAmmoIconDefault = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/default_ammo");

	// Ammo icons
	m_pAmmo357 = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_357");
	m_pAmmo9mm = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_9mm");
	m_pAmmoBolt = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_bolt");
	m_pAmmoBuckshot = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_buckshot");
	m_pAmmoEnergy = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_energy");
	m_pAmmoGrenadeFrag = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_grenade_frag");
	m_pAmmoGrenadeHornet = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_grenade_hornet");
	m_pAmmoGrenadeMP5 = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_grenade_mp5");
	m_pAmmoGrenadeRPG = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_grenade_rpg");
	m_pAmmoGrenadeSatchel = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_grenade_satchel");
	m_pAmmoGrenadeTripmine = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_grenade_tripmine");
	m_pAmmoSnark = new CTGAImage(VGUI2_ROOT_DIR "gfx/hud/ammo_snark");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/HudAmmo.res");

	SetVisible(false);
}

void CHudAmmoPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	// Calculate positions here because variable binding doesn't support position flags.
	ComputePos(m_szBarX, m_iBarX, GetWide(), GetParent() ? GetParent()->GetWide() : 0, true);
	ComputePos(m_szBarFullX, m_iBarFullX, GetWide(), GetParent() ? GetParent()->GetWide() : 0, true);

	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
}

void CHudAmmoPanel::PaintBackground()
{
	// Draw the panel background
	DrawBox(0, 0, GetWide(), GetTall(), GetBgColor(), 1.0f);

	// Determine icon position and bar color
	int iconX = m_iAmmoIconFullX;
	Color barColor = m_hudCurrentColor;

	if (m_iClip < 0)
	{
		if (m_iBarShrink)
		{
			iconX = m_iAmmoIconX;
			barColor = Color{0, 0, 0, 0}; // Not visible when bar is shrunk
		}
		else
		{
			barColor = m_DividerOffColor; // Visible but dimmed
		}
	}

	// Draw the divider bar between clip and ammo
	vgui2::surface()->DrawSetColor(barColor);
	vgui2::surface()->DrawFilledRect(m_iDividerX, m_iDividerY, m_iDividerX + m_iDividerWide, m_iDividerY + m_iDividerTall);

	// Draw the ammo icon
	m_pAmmoIcon->SetPos(iconX, m_iAmmoIconY);
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

void CHudAmmoPanel::UpdateAmmoPanel(WEAPON *pWeapon, int maxClip, int ammo1, int ammo2)
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
		switch (m_iAmmoType)
		{
		case AMMO_TYPE_9MM:
			m_pAmmoIcon = m_pAmmo9mm;
			break;
		case AMMO_TYPE_357:
			m_pAmmoIcon = m_pAmmo357;
			break;
		case AMMO_TYPE_BOLT:
			m_pAmmoIcon = m_pAmmoBolt;
			break;
		case AMMO_TYPE_BUCKSHOT:
			m_pAmmoIcon = m_pAmmoBuckshot;
			break;
		case AMMO_TYPE_ENERGY:
			m_pAmmoIcon = m_pAmmoEnergy;
			break;
		case AMMO_TYPE_GRENADE_FRAG:
			m_pAmmoIcon = m_pAmmoGrenadeFrag;
			break;
		case AMMO_TYPE_GRENADE_HORNET:
			m_pAmmoIcon = m_pAmmoGrenadeHornet;
			break;
		case AMMO_TYPE_GRENADE_MP5:
			m_pAmmoIcon = m_pAmmoGrenadeMP5;
			break;
		case AMMO_TYPE_GRENADE_RPG:
			m_pAmmoIcon = m_pAmmoGrenadeRPG;
			break;
		case AMMO_TYPE_GRENADE_SATCHEL:
			m_pAmmoIcon = m_pAmmoGrenadeSatchel;
			break;
		case AMMO_TYPE_GRENADE_TRIPMINE:
			m_pAmmoIcon = m_pAmmoGrenadeTripmine;
			break;
		case AMMO_TYPE_SNARK:
			m_pAmmoIcon = m_pAmmoSnark;
			break;
		default:
			m_pAmmoIcon = m_pAmmoIconDefault;
			break;
		}
	}
}

const char *CHudAmmoPanel::GetName()
{
	return VIEWPORT_PANEL_HUD_AMMO;
}

void CHudAmmoPanel::Reset()
{
}

void CHudAmmoPanel::OnThink()
{
	int r, g, b;
	// Does weapon have any ammo at all?
	if (m_iAmmoType > 0)
	{
		char buf[32];

		// Does this weapon have a clip?
		if (m_iClip >= 0)
		{
			gHUD.GetHudAmmoColor(m_iClip, m_iMaxClip, r, g, b);

			// Show left (clip) and right (ammo) digits
			m_pDigitLeftLabel->SetVisible(true);
			m_pDigitLeftLabelGlow->SetVisible(true);
			m_pDigitRightLabel->SetVisible(true);
			m_pDigitRightLabelGlow->SetVisible(true);
			m_pDigitRightBgLabel->SetVisible(true);

			// Set clip text
			snprintf(buf, sizeof(buf), "%d", m_iClip);
			m_pDigitLeftLabel->SetText(buf);
			m_pDigitLeftLabelGlow->SetText(buf);

			// Set ammo text
			snprintf(buf, sizeof(buf), "%d", m_iAmmoCount);
			m_pDigitRightLabel->SetText(buf);
			m_pDigitRightLabelGlow->SetText(buf);

			// Position and size
			SetPos(m_iBarFullX, GetYPos());
			SetSize(m_iBarFullWide, GetTall());
		}
		else // No clip
		{
			gHUD.GetHudAmmoColor(m_iAmmoCount, m_iMax1, r, g, b);

			if (m_iBarShrink)
			{
				// Only show left digit for ammo
				m_pDigitLeftLabel->SetVisible(true);
				m_pDigitLeftLabelGlow->SetVisible(true);
				m_pDigitRightLabel->SetVisible(false);
				m_pDigitRightLabelGlow->SetVisible(false);
				m_pDigitRightBgLabel->SetVisible(false);

				snprintf(buf, sizeof(buf), "%d", m_iAmmoCount);
				m_pDigitLeftLabel->SetText(buf);
				m_pDigitLeftLabelGlow->SetText(buf);

				SetPos(m_iBarX, GetYPos());
				SetSize(m_iBarWide, GetTall());
			}
			else
			{
				// Only show right digit for ammo
				m_pDigitLeftLabel->SetVisible(false);
				m_pDigitLeftLabelGlow->SetVisible(false);
				m_pDigitRightLabel->SetVisible(true);
				m_pDigitRightLabelGlow->SetVisible(true);
				m_pDigitRightBgLabel->SetVisible(true);

				snprintf(buf, sizeof(buf), "%d", m_iAmmoCount);
				m_pDigitRightLabel->SetText(buf);
				m_pDigitRightLabelGlow->SetText(buf);

				SetPos(m_iBarFullX, GetYPos());
				SetSize(m_iBarFullWide, GetTall());
			}
		}
	}

	// Show glow effect when dimmed, if not, set full alpha and hide glow digits
	auto [a1, a2] = gHUD.GetHudDimAlphas(m_pHudDim.GetBool(), m_fFade, gHUD.m_flTimeDelta);
	
	// Set colors for glow and normal digits
	m_hudCurrentColor = Color(r, g, b, a1);
	m_pDigitRightLabelGlow->SetFgColor(m_hudCurrentColor);
	m_pDigitLeftLabelGlow->SetFgColor(m_hudCurrentColor);

	m_hudCurrentColor = Color(r, g, b, a2);
	m_pDigitLeftLabel->SetFgColor(m_hudCurrentColor);
	m_pDigitRightLabel->SetFgColor(m_hudCurrentColor);
}

void CHudAmmoPanel::ShowPanel(bool state)
{
	if (state != IsVisible())
	{
		SetVisible(state);
	}
}

vgui2::VPANEL CHudAmmoPanel::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CHudAmmoPanel::IsVisible()
{
	return BaseClass::IsVisible();
}

void CHudAmmoPanel::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}
