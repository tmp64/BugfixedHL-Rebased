#include <vgui/ILocalize.h>
#include <vgui_controls/Button.h>
#include "cvar_color.h"
#include "hud.h"
#include "cl_util.h"
#include "color_picker.h"

CCvarColor::CCvarColor(vgui2::Panel *parent, const char *panelName, const char *cvarName, const char *cvarTitle) :
	vgui2::EditablePanel(parent, panelName)
{
	m_pPreview = new vgui2::Panel(this, "ColorPreview");
	m_pBtn = new vgui2::Button(this, "PickColorBtn", "#BHL_PickColor", this, "pickcolor");
	m_pColorPicker = new CColorPicker(this, "ColorPicker", cvarTitle);
	m_pColorPicker->AddActionSignalTarget(this);

	if (cvarName)
	{
		m_pCvar = gEngfuncs.pfnGetCvarPointer(cvarName);
		if (!m_pCvar)
		{
			Msg("%s [CCvarColor]: cvar '%s' not found.\n", panelName, cvarName);
		}
		ResetData();
	}
}

void CCvarColor::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	int constexpr TALL = 24;
	SetSize(128, TALL);
	m_pPreview->SetBounds(0, 0, 48, TALL);
	m_pPreview->SetPaintBackgroundEnabled(true);
	m_pPreview->SetBgColor(m_NewColor);
	m_pPreview->SetBorder(pScheme->GetBorder("DepressedBorder"));
	m_pBtn->SetPos(52, 0);
}

void CCvarColor::OnCommand(const char *pCmd)
{
	if (!strcmp(pCmd, "pickcolor"))
	{
		m_pColorPicker->SetColor(m_NewColor);
		m_pColorPicker->Activate();
	}
}

void CCvarColor::ResetData()
{
	if (m_pCvar)
	{
		Color rgba;
		if (ParseColor(m_pCvar->string, rgba))
		{
			SetInitialColor(rgba);
		}
	}
}

void CCvarColor::ApplyChanges()
{
	if (m_pCvar)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "%s \"%d %d %d\"", m_pCvar->name, m_NewColor.r(), m_NewColor.g(), m_NewColor.b());
		gEngfuncs.pfnClientCmd(buf);
		SetInitialColor(m_NewColor);
	}
}

void CCvarColor::SetInitialColor(Color c)
{
	m_NewColor = c;
	m_pPreview->SetBgColor(m_NewColor);
	m_pColorPicker->SetInitialColor(m_NewColor);
}

Color CCvarColor::GetSelectedColor()
{
	return m_NewColor;
}

void CCvarColor::OnColorPicked(KeyValues *kv)
{
	m_NewColor.SetRawColor(kv->GetInt("color"));
	m_pPreview->SetBgColor(m_NewColor);

	// Relay down the line
	PostActionSignal(kv->MakeCopy());
}
