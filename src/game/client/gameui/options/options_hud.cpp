#include <vgui_controls/Label.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ComboBox.h>
#include <KeyValues.h>
#include <MinMax.h>
#include "client_vgui.h"
#include "options_hud.h"
#include "cvar_text_entry.h"
#include "cvar_color.h"
#include "cvar_check_button.h"
#include "hud.h"
#include "hud_renderer.h"

CHudSubOptions::CHudSubOptions(vgui2::Panel *parent)
    : BaseClass(parent, "HudSubOptions")
{
	SetSize(100, 100); // Silence "parent not sized yet" warning

	m_pOpacityLabel = new vgui2::Label(this, "OpacityLabel", "#BHL_AdvOptions_HUD_Opacity");
	m_pOpacityValue = new CCvarTextEntry(this, "OpacityValue", "hud_draw", CCvarTextEntry::CvarType::Float);
	m_pOpacitySlider = new vgui2::Slider(this, "OpacitySlider");
	m_pOpacitySlider->SetRange(0, 100);
	m_pOpacitySlider->SetValue(m_pOpacityValue->GetFloat() * 100.f);
	m_pOpacitySlider->AddActionSignalTarget(this);

	m_pRenderCheckbox = new CCvarCheckButton(this, "RenderCheckbox", "#BHL_AdvOptions_HUD_Render", "hud_client_renderer");
	m_pDimCheckbox = new CCvarCheckButton(this, "DimCheckbox", "#BHL_AdvOptions_HUD_Dim", "hud_dim");
	m_pMenuFKeys = new CCvarCheckButton(this, "MenuFKeys", "#BHL_AdvOptions_HUD_MenuFKeys", "hud_menu_fkeys");
	m_pWeaponSpriteCheckbox = new CCvarCheckButton(this, "WeaponSpriteCheckbox", "#BHL_AdvOptions_HUD_WeapSprite", "hud_weapon");
	m_pCenterIdCvar = new CCvarCheckButton(this, "CenterIdCvar", "#BHL_AdvOptions_HUD_CenterId", "hud_centerid");
	m_pRainbowCvar = new CCvarCheckButton(this, "RainbowCvar", "#BHL_AdvOptions_HUD_Rainbow", "hud_rainbow");

	m_pSpeedCheckbox = new CCvarCheckButton(this, "SpeedCheckbox", "#BHL_AdvOptions_HUD_Speed", "hud_speedometer");
	m_pSpeedCrossCheckbox = new CCvarCheckButton(this, "SpeedCrossCheckbox", "#BHL_AdvOptions_HUD_SpeedCross", "hud_speedometer_below_cross");

	m_pJumpSpeedCheckbox = new CCvarCheckButton(this, "JumpSpeedCheckbox", "#BHL_AdvOptions_HUD_JumpSpeed", "hud_jumpspeed");
	m_pJumpSpeedCrossCheckbox = new CCvarCheckButton(this, "JumpSpeedCrossCheckbox", "#BHL_AdvOptions_HUD_JumpSpeedCross", "hud_jumpspeed_below_cross");

	m_pDeathnoticeVGui = new CCvarCheckButton(this, "DeathnoticeCheckbox", "#BHL_AdvOptions_HUD_Deathnotice", "hud_deathnotice_vgui");

	m_pTimerBox = new CCVarComboBox(this, "TimerBox", "hud_timer");
	m_pTimerBox->AddItem("#BHL_AdvOptions_Hud_Timer0", "0");
	m_pTimerBox->AddItem("#BHL_AdvOptions_Hud_Timer1", "1");
	m_pTimerBox->AddItem("#BHL_AdvOptions_Hud_Timer2", "2");
	m_pTimerBox->AddItem("#BHL_AdvOptions_Hud_Timer3", "3");

	m_pScaleBox = new CCVarComboBox(this, "ScaleBox", "hud_scale");
	m_pScaleBox->AddItem("#BHL_AdvOptions_Hud_ScaleAuto", "0");

	constexpr const char *SCALES[] = { "50%", "100%", "200%", "400%" };

	for (int i = 1; i <= std::size(SCALES); i++)
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", i);
		int itemId = m_pScaleBox->AddItem(SCALES[i - 1], buf);

		bool isSupported = i <= (int)gHUD.GetMaxHudScale();
		m_pScaleBox->SetItemEnabled(itemId, isSupported);
	}

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/HudSubOptions.res");
	m_pOpacityLabel->MoveToFront(); // Obscured by the slider

	// Client sprite renderer only works in hardware mode
	if (!CHudRenderer::Get().IsAvailable())
	{
		m_pRenderCheckbox->SetEnabled(false);
		m_pDeathnoticeVGui->SetEnabled(false);
	}
}

void CHudSubOptions::OnResetData()
{
	if (m_pRenderCheckbox->IsEnabled())
		m_pRenderCheckbox->ResetData();
	else
		m_pRenderCheckbox->SetSelected(false);

	m_pOpacityValue->ResetData();
	m_pDimCheckbox->ResetData();
	m_pMenuFKeys->ResetData();
	m_pWeaponSpriteCheckbox->ResetData();
	m_pCenterIdCvar->ResetData();
	m_pRainbowCvar->ResetData();
	m_pSpeedCheckbox->ResetData();
	m_pSpeedCrossCheckbox->ResetData();
	m_pJumpSpeedCheckbox->ResetData();
	m_pJumpSpeedCrossCheckbox->ResetData();

	if (m_pDeathnoticeVGui->IsEnabled())
		m_pDeathnoticeVGui->ResetData();
	else
		m_pDeathnoticeVGui->SetSelected(false);

	m_pTimerBox->ResetData();
	m_pScaleBox->ResetData();
}

void CHudSubOptions::OnApplyChanges()
{
	if (m_pRenderCheckbox->IsEnabled())
		m_pRenderCheckbox->ApplyChanges();

	m_pOpacityValue->ApplyChanges();
	m_pDimCheckbox->ApplyChanges();
	m_pMenuFKeys->ApplyChanges();
	m_pWeaponSpriteCheckbox->ApplyChanges();
	m_pCenterIdCvar->ApplyChanges();
	m_pRainbowCvar->ApplyChanges();
	m_pSpeedCheckbox->ApplyChanges();
	m_pSpeedCrossCheckbox->ApplyChanges();
	m_pJumpSpeedCheckbox->ApplyChanges();
	m_pJumpSpeedCrossCheckbox->ApplyChanges();

	if (m_pDeathnoticeVGui->IsEnabled())
		m_pDeathnoticeVGui->ApplyChanges();

	m_pTimerBox->ApplyChanges();
	m_pScaleBox->ApplyChanges();
}

void CHudSubOptions::OnSliderMoved(KeyValues *kv)
{
	void *pPanel = kv->GetPtr("panel");
	if (pPanel == m_pOpacitySlider)
	{
		float val = kv->GetFloat("position") / 100.f;
		m_pOpacityValue->SetValue(val);
	}
}

void CHudSubOptions::OnCvarTextChanged(KeyValues *kv)
{
	void *pPanel = kv->GetPtr("panel");
	if (pPanel == m_pOpacityValue)
	{
		float val = kv->GetFloat("value") * 100.f;
		m_pOpacitySlider->SetValue(val, false);
	}
}
