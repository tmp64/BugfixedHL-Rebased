#include <vgui/ILocalize.h>
#include "cvar_slider.h"
#include "hud.h"

CCvarSlider::CCvarSlider(vgui2::Panel *parent, const char *panelName, const char *cvarName)
    : vgui2::Slider(parent, panelName)
{
	m_pCvar = gEngfuncs.pfnGetCvarPointer(cvarName);
	if (!m_pCvar)
	{
		Msg("%s [CCvarSlider]: cvar '%s' not found.\n", panelName, cvarName);
	}
	ResetData();
}

void CCvarSlider::ResetData()
{
	if (m_pCvar)
	{
		SetValue(m_pCvar->value);
	}
}

void CCvarSlider::ApplyChanges()
{
	if (m_pCvar)
	{
		char buf[256];
		float value = GetValue();
		snprintf(buf, sizeof(buf), "%s \"%.2f\"", m_pCvar->name, value);
		gEngfuncs.pfnClientCmd(buf);
	}
}
