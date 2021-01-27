#include <vgui_controls/Label.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ComboBox.h>
#include <KeyValues.h>
#include "client_vgui.h"
#include "cvar_text_entry.h"
#include "cvar_check_button.h"
#include "options_general.h"
#include "hud.h"

CGeneralSubOptions::CGeneralSubOptions(vgui2::Panel *parent)
    : BaseClass(parent, nullptr)
{
	m_pFovLabel = new vgui2::Label(this, "FovLabel", "#BHL_AdvOptions_General_FOV");
	m_pFovValue = new CCvarTextEntry(this, "FovValue", "default_fov", CCvarTextEntry::CvarType::Float);
	m_pFovSlider = new vgui2::Slider(this, "FovSlider");
	m_pFovSlider->SetRange(90, 150);
	m_pFovSlider->SetValue(m_pFovValue->GetFloat());
	m_pFovSlider->AddActionSignalTarget(this);

	m_pRawInput = new vgui2::CheckButton(this, "RawInput", "__RawInput");
	m_pRawInputLabel = new vgui2::Label(this, "RawInputLabel", "#BHL_AdvOptions_General_Input");

	m_pKillSnd = new CCvarCheckButton(this, "KillSnd", "#BHL_AdvOptions_General_KillSnd", "cl_killsound");
	m_pKillSndLabel = new vgui2::Label(this, "KillSndLabel", "#BHL_AdvOptions_General_KillSnd2");

	m_pMOTD = new CCvarCheckButton(this, "MOTD", "#BHL_AdvOptions_General_HTML", "cl_enable_html_motd");
	m_pMOTDLabel = new vgui2::Label(this, "MOTDLabel", "#BHL_AdvOptions_General_HTML2");

	m_pAutoDemo = new CCvarCheckButton(this, "AutoDemo", "#BHL_AdvOptions_General_AutoDemo", "results_demo_autorecord");
	m_pAutoDemoLabel = new vgui2::Label(this, "AutoDemoLabel", "#BHL_AdvOptions_General_AutoDemo2");
	m_pKeepFor = new CCvarTextEntry(this, "KeepFor", "results_demo_keepdays", CCvarTextEntry::CvarType::Int);
	m_pKeepForLabel = new vgui2::Label(this, "KeepForLabel", "#BHL_AdvOptions_General_KeepFor");
	m_pDaysLabel = new vgui2::Label(this, "DaysLabel", "#BHL_AdvOptions_General_Days");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/GeneralSubOptions.res");
}

void CGeneralSubOptions::PerformLayout()
{
	BaseClass::PerformLayout();

	// Overwrite text from .res file
	m_pRawInput->SetText(GetRawInputText());
}

void CGeneralSubOptions::OnResetData()
{
	m_pFovValue->ResetData();
	m_pRawInput->SetSelected(GetRawInputVal());
	m_pKillSnd->ResetData();
	m_pMOTD->ResetData();
	m_pAutoDemo->ResetData();
	m_pKeepFor->ResetData();
}

void CGeneralSubOptions::OnApplyChanges()
{
	m_pFovValue->ApplyChanges();
	SetRawInputVal(m_pRawInput->IsSelected());
	m_pKillSnd->ApplyChanges();
	m_pMOTD->ApplyChanges();
	m_pAutoDemo->ApplyChanges();
	m_pKeepFor->ApplyChanges();
}

void CGeneralSubOptions::OnSliderMoved(KeyValues *kv)
{
	void *pPanel = kv->GetPtr("panel");
	if (pPanel == m_pFovSlider)
	{
		float val = kv->GetFloat("position");
		m_pFovValue->SetValue(val);
	}
}

void CGeneralSubOptions::OnCvarTextChanged(KeyValues *kv)
{
	void *pPanel = kv->GetPtr("panel");
	if (pPanel == m_pFovValue)
	{
		float val = kv->GetFloat("value");
		m_pFovSlider->SetValue(val, false);
	}
}

#ifdef _WIN32

// Windows
void CGeneralSubOptions::SetRawInputVal(bool state)
{
	const char *str;
	if (state)
		str = "m_input 2";
	else
		str = "m_input 1";
	gEngfuncs.pfnClientCmd((char *)str);

#ifndef VGUI2_BUILD_4554
	gEngfuncs.pfnClientCmd("m_rawinput 0");
#endif
}

bool CGeneralSubOptions::GetRawInputVal()
{
	float val = gEngfuncs.pfnGetCvarFloat("m_input");
	return (val == 2);
}

const char *CGeneralSubOptions::GetRawInputText()
{
	return "#BHL_AdvOptions_General_InputWin";
}

#else

// Linux
void CGeneralSubOptions::SetRawInputVal(bool state)
{
	const char *str;
	if (state)
		str = "m_rawinput 1";
	else
		str = "m_rawinput 0";
	gEngfuncs.pfnClientCmd((char *)str);
}

bool CGeneralSubOptions::GetRawInputVal()
{
	float val = gEngfuncs.pfnGetCvarFloat("m_input");
	return (val == 1);
}

const char *CGeneralSubOptions::GetRawInputText()
{
	return "#BHL_AdvOptions_General_InputLin";
}

#endif
