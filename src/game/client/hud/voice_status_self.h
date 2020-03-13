#ifndef HUD_VOICE_STATUS_SELF_H
#define HUD_VOICE_STATUS_SELF_H
#include <vgui_controls/Panel.h>
#include "base.h"

//=============================================================================
// Icon for the local player using voice
//=============================================================================
class CHudVoiceStatusSelf : public CHudElemBase<CHudVoiceStatusSelf>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CHudVoiceStatusSelf, vgui2::Panel);

	CHudVoiceStatusSelf();

	virtual void Paint();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

private:
	CPanelAnimationVarAliasType(int, m_iVoiceIconTexture, "VoiceIconTexture", "ui/gfx/hud/speaker_self", "textureid");

	Color m_clrIcon;
};

#endif
