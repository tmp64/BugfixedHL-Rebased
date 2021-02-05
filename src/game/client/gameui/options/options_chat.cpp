#include <vgui_controls/Label.h>
#include <vgui_controls/ComboBox.h>
#include <KeyValues.h>
#include "client_vgui.h"
#include "options_chat.h"
#include "hud.h"
#include "cvar_check_button.h"
#include "cvar_text_entry.h"

CChatSubOptions::CChatSubOptions(vgui2::Panel *parent)
    : BaseClass(parent, "ChatSubOptions")
{
	SetSize(100, 100); // Silence "parent not sized yet" warning

	m_pTime = new CCvarTextEntry(this, "TimeCvar", "hud_saytext_time", CCvarTextEntry::CvarType::Int);
	m_pTimeLabel = new vgui2::Label(this, "TimeLabel", "#BHL_AdvOptions_Chat_Time");
	m_pChatSound = new CCvarCheckButton(this, "ChatSound", "#BHL_AdvOptions_Chat_Sound", "hud_saytext_sound");
	m_pChatSoundLabel = new vgui2::Label(this, "ChatSoundLabel", "#BHL_AdvOptions_Chat_SoundLabel");
	m_pMuteAllComms = new CCvarCheckButton(this, "MuteAllComms", "#BHL_AdvOptions_Chat_Mute", "cl_mute_all_comms");
	m_pMuteAllCommsLabel = new vgui2::Label(this, "MuteAllCommsLabel", "#BHL_AdvOptions_Chat_MuteLabel");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/ChatSubOptions.res");
}

void CChatSubOptions::OnResetData()
{
	m_pTime->ResetData();
	m_pChatSound->ResetData();
	m_pMuteAllComms->ResetData();
}

void CChatSubOptions::OnApplyChanges()
{
	m_pTime->ApplyChanges();
	m_pChatSound->ApplyChanges();
	m_pMuteAllComms->ApplyChanges();
}
