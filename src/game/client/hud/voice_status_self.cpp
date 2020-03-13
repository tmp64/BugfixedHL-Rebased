#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include "hud.h"
#include "cl_util.h"
#include "cl_voice_status.h"
#include "voice_status_self.h"
#include "vgui/client_viewport.h"

DEFINE_HUD_ELEM(CHudVoiceStatusSelf);

CHudVoiceStatusSelf::CHudVoiceStatusSelf()
    : vgui2::Panel(NULL, "HudVoiceSelfStatus")
{
	SetParent(g_pViewport);

	int m_iVoiceIconTexture = -1;

	m_clrIcon = Color(255, 255, 255, 254);
}

void CHudVoiceStatusSelf::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetBgColor(Color(0, 0, 0, 0));
}

void CHudVoiceStatusSelf::Paint()
{
	bool bShouldDraw = GetClientVoiceMgr()->m_bTalking;
	if (!bShouldDraw || m_iVoiceIconTexture == -1)
		return;

	int x, y, w, h;
	GetBounds(x, y, w, h);

	vgui2::surface()->DrawSetTexture(m_iVoiceIconTexture);
	vgui2::surface()->DrawSetColor(m_clrIcon);
	vgui2::surface()->DrawTexturedRect(0, 0, w, h);
}
