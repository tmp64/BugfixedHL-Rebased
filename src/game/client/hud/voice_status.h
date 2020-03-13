#ifndef HUD_VOICE_STATUS_H
#define HUD_VOICE_STATUS_H
#include <tier1/utllinkedlist.h>
#include <vgui_controls/Panel.h>
#include "base.h"

class CAvatarImage;

//=============================================================================
// Icons for other players using voice
//=============================================================================
class CHudVoiceStatus : public CHudElemBase<CHudVoiceStatus>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CHudVoiceStatus, vgui2::Panel);

	CHudVoiceStatus();
	~CHudVoiceStatus(void);

	virtual void Paint();
	virtual void Init();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	void RunFrame(float fTime);

protected:
	void ClearActiveList();
	int FindActiveSpeaker(int playerId);

private:
	Color m_NoTeamColor;

	struct ActiveSpeaker
	{
		int playerId;
		CAvatarImage *pAvatar;
		bool bSpeaking;
		float fAlpha;
		float fAlphaMultiplier;
	};

	CUtlLinkedList<ActiveSpeaker> m_SpeakingList;
	// CUtlLinkedList< CAvatarImagePanel* > m_SpeakingListAvatar;

	CPanelAnimationVarAliasType(int, m_iVoiceIconTexture, "VoiceIconTexture", "ui/gfx/hud/speaker", "textureid");

	//CPanelAnimationVar(vgui2::HFont, m_NameFont, "Default", "Default");
	vgui2::HFont m_NameFont = -1;

	CPanelAnimationVarAliasType(float, item_tall, "item_tall", "16", "proportional_float");
	CPanelAnimationVarAliasType(float, item_wide, "item_wide", "160", "proportional_float");
	CPanelAnimationVarAliasType(float, item_spacing, "item_spacing", "2", "proportional_float");

	CPanelAnimationVarAliasType(bool, show_avatar, "show_avatar", "0", "bool");
	CPanelAnimationVarAliasType(bool, show_friend, "show_friend", "1", "bool");
	CPanelAnimationVarAliasType(float, avatar_ypos, "avatar_ypos", "0", "proportional_float");
	CPanelAnimationVarAliasType(float, avatar_xpos, "avatar_xpos", "16", "proportional_float");
	CPanelAnimationVarAliasType(float, avatar_tall, "avatar_tall", "16", "proportional_float");
	CPanelAnimationVarAliasType(float, avatar_wide, "avatar_wide", "16", "proportional_float");

	CPanelAnimationVarAliasType(bool, show_voice_icon, "show_voice_icon", "1", "bool");
	CPanelAnimationVarAliasType(float, voice_icon_ypos, "icon_ypos", "0", "proportional_float");
	CPanelAnimationVarAliasType(float, voice_icon_xpos, "icon_xpos", "24", "proportional_float");
	CPanelAnimationVarAliasType(float, voice_icon_tall, "icon_tall", "16", "proportional_float");
	CPanelAnimationVarAliasType(float, voice_icon_wide, "icon_wide", "16", "proportional_float");

	CPanelAnimationVarAliasType(bool, show_dead_icon, "show_dead_icon", "1", "bool");
	CPanelAnimationVarAliasType(float, dead_icon_ypos, "dead_ypos", "0", "proportional_float");
	CPanelAnimationVarAliasType(float, dead_icon_xpos, "dead_xpos", "0", "proportional_float");
	CPanelAnimationVarAliasType(float, dead_icon_tall, "dead_tall", "16", "proportional_float");
	CPanelAnimationVarAliasType(float, dead_icon_wide, "dead_wide", "16", "proportional_float");

	CPanelAnimationVarAliasType(float, text_xpos, "text_xpos", "40", "proportional_float");

	CPanelAnimationVarAliasType(float, fade_in_time, "fade_in_time", "0.0", "float");
	CPanelAnimationVarAliasType(float, fade_out_time, "fade_out_time", "0.0", "float");
};

#endif
