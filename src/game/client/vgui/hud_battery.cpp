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
#include "hud_battery.h"

CHudBatteryPanel::CHudBatteryPanel()
    : BaseClass(nullptr, VIEWPORT_PANEL_HUD_BATTERY)
{
	SetSize(100, 100); // Silence "parent not sized yet" warning
	SetProportional(true);

	m_pBatteryIcon = new vgui2::Label(this, "BatteryIcon", "a");
	m_pBatteryAmountBg = new vgui2::Label(this, "BatteryAmountBg", "000");
	m_pBatteryAmountGlow = new vgui2::Label(this, "BatteryAmountGlow", "100");
	m_pBatteryAmount = new vgui2::Label(this, "BatteryAmount", "100");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/HudBattery.res");

	SetVisible(false);
}

void CHudBatteryPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
}

void CHudBatteryPanel::PaintBackground()
{
	DrawBox(0, 0, GetWide(), GetTall(), GetBgColor(), 1.0f);
}

void CHudBatteryPanel::UpdateBatteryPanel(int amount)
{
	// Reset fade only if amount changed
	if (amount != m_iBattery)
		m_fFade = FADE_TIME;

	m_iBattery = amount;

	// Update health amount text
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", m_iBattery);
	m_pBatteryAmount->SetText(buf);
	m_pBatteryAmountGlow->SetText(buf);
}

const char *CHudBatteryPanel::GetName()
{
	return VIEWPORT_PANEL_HUD_BATTERY;
}

void CHudBatteryPanel::Reset()
{
}

void CHudBatteryPanel::OnThink()
{	
	// Glow effect will be visible only when dimmed
	auto [a1, a2] = gHUD.GetHudDimAlphas(m_fFade);

	Color currentColor = gHUD.GetHudColor(HudPart::Armor, m_iBattery);
	currentColor[3] = a1;
	m_pBatteryAmountGlow->SetFgColor(currentColor);

	currentColor[3] = a2;
	m_pBatteryAmount->SetFgColor(currentColor);
	m_pBatteryIcon->SetFgColor(currentColor);
}

void CHudBatteryPanel::ShowPanel(bool state)
{
	if (state != IsVisible())
	{
		SetVisible(state);
	}
}

vgui2::VPANEL CHudBatteryPanel::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CHudBatteryPanel::IsVisible()
{
	return BaseClass::IsVisible();
}

void CHudBatteryPanel::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}
