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
#include "hud_health.h"

CHudHealthPanel::CHudHealthPanel()
    : BaseClass(nullptr, VIEWPORT_PANEL_HUD_HEALTH)
{
	SetSize(100, 100); // Silence "parent not sized yet" warning
	SetProportional(true);

	m_pHealthIcon = new vgui2::Label(this, "HealthIcon", "b");
	m_pHealthAmountBg = new vgui2::Label(this, "HealthAmountBg", "000");
	m_pHealthAmountGlow = new vgui2::Label(this, "HealthAmountGlow", "100");
	m_pHealthAmount = new vgui2::Label(this, "HealthAmount", "100");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/HudHealth.res");

	SetVisible(false);
}

void CHudHealthPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
}

void CHudHealthPanel::PaintBackground()
{
	DrawBox(0, 0, GetWide(), GetTall(), GetBgColor(), 1.0f);
}

void CHudHealthPanel::UpdateHealthPanel(int health)
{
	// Reset fade only if health changed
	if (health != m_iHealth)
		m_fFade = FADE_TIME;

	m_iHealth = health;

	// Update health amount text
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", health);
	m_pHealthAmount->SetText(buf);
	m_pHealthAmountGlow->SetText(buf);
}

const char *CHudHealthPanel::GetName()
{
	return VIEWPORT_PANEL_HUD_HEALTH;
}

void CHudHealthPanel::Reset()
{
}

void CHudHealthPanel::OnThink()
{	
	// Glow effect will be visible only when dimmed
	auto [a1, a2] = gHUD.GetHudDimAlphas(m_pHudDim.GetBool(), m_fFade, gHUD.m_flTimeDelta);

	Color currentColor = gHUD.GetHudColor(HudPart::Health, m_iHealth);
	currentColor[3] = a1;
	m_pHealthAmountGlow->SetFgColor(currentColor);

	currentColor[3] = a2;
	m_pHealthAmount->SetFgColor(currentColor);
	m_pHealthIcon->SetFgColor(currentColor);
}

void CHudHealthPanel::ShowPanel(bool state)
{
	if (state != IsVisible())
	{
		SetVisible(state);
	}
}

vgui2::VPANEL CHudHealthPanel::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CHudHealthPanel::IsVisible()
{
	return BaseClass::IsVisible();
}

void CHudHealthPanel::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}
