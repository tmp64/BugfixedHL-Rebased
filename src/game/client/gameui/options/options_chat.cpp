#include <vgui_controls/Label.h>
#include <vgui_controls/ComboBox.h>
#include <KeyValues.h>
#include "client_vgui.h"
#include "options_chat.h"
#include "hud.h"
#include "cvar_check_button.h"
#include "cvar_text_entry.h"

CChatSubOptions::CChatSubOptions(vgui2::Panel *parent) : BaseClass(parent, nullptr)
{
	m_pChatStyleLabel = new vgui2::Label(this, "ChatStyleLabel", "#BHL_AdvOptions_Chat_ChatStyle");
	m_pChatStyleBox = new vgui2::ComboBox(this, "ChatStyleBox", 3, false);
	m_ChatStyleItems[1] = m_pChatStyleBox->AddItem("#BHL_AdvOptions_Chat_ChatStyle1", new KeyValues("VGUI2", "value", 1));
	m_ChatStyleItems[2] = m_pChatStyleBox->AddItem("#BHL_AdvOptions_Chat_ChatStyle2", new KeyValues("OldBottom", "value", 2));
	m_ChatStyleItems[3] = m_pChatStyleBox->AddItem("#BHL_AdvOptions_Chat_ChatStyle3", new KeyValues("OldTop", "value", 3));

	m_pVgui2Label = new vgui2::Label(this, "Vgui2Label", "#BHL_AdvOptions_Chat_V2Label");

	m_pTime = new CCvarTextEntry(this, "TimeCvar", "hud_saytext_time2", CCvarTextEntry::CvarType::Int);
	m_pTimeLabel = new vgui2::Label(this, "TimeLabel", "#BHL_AdvOptions_Chat_Time");

	m_pMuteAllComms = new CCvarCheckButton(this, "MuteAllComms", "#BHL_AdvOptions_Chat_Mute", "cl_mute_all_comms");
	m_pMuteAllCommsLabel = new vgui2::Label(this, "MuteAllCommsLabel", "#BHL_AdvOptions_Chat_MuteLabel");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/ChatSubOptions.res");
}

void CChatSubOptions::OnResetData()
{
	bool oldchat = !!gEngfuncs.pfnGetCvarFloat("hud_saytext_oldchat");
	bool oldpos = !!gEngfuncs.pfnGetCvarFloat("hud_saytext_oldpos");

	if (!oldchat)	// VGUI2
	{
		m_pChatStyleBox->ActivateItem(m_ChatStyleItems[1]);
	}
	else
	{
		if (oldpos)	// Old (top)
			m_pChatStyleBox->ActivateItem(m_ChatStyleItems[3]);
		else	// Old (bottom)
			m_pChatStyleBox->ActivateItem(m_ChatStyleItems[2]);
	}

	m_pTime->ResetData();
	m_pMuteAllComms->ResetData();
}

void CChatSubOptions::OnApplyChanges()
{
	KeyValues *userdata = m_pChatStyleBox->GetActiveItemUserData();
	Assert(userdata);
	int sel = userdata->GetInt("value", 0);
	Assert(sel >= 1 && sel <= 3);

	char buf[128];

	if (sel == 1)		// VGUI2
	{
		snprintf(buf, sizeof(buf), "hud_saytext_oldchat 0");
		gEngfuncs.pfnClientCmd(buf);
		snprintf(buf, sizeof(buf), "hud_saytext_oldpos 0");
		gEngfuncs.pfnClientCmd(buf);
	}
	else if (sel == 2)	// Old (bottom)
	{
		snprintf(buf, sizeof(buf), "hud_saytext_oldchat 1");
		gEngfuncs.pfnClientCmd(buf);
		snprintf(buf, sizeof(buf), "hud_saytext_oldpos 0");
		gEngfuncs.pfnClientCmd(buf);
	}
	else if (sel == 3)	// Old (top)
	{
		snprintf(buf, sizeof(buf), "hud_saytext_oldchat 1");
		gEngfuncs.pfnClientCmd(buf);
		snprintf(buf, sizeof(buf), "hud_saytext_oldpos 1");
		gEngfuncs.pfnClientCmd(buf);
	}

	m_pTime->ApplyChanges();
	m_pMuteAllComms->ApplyChanges();
}
