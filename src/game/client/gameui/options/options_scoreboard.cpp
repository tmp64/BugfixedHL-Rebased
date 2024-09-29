#include <vgui_controls/Label.h>
#include <vgui_controls/ComboBox.h>
#include <KeyValues.h>
#include "client_vgui.h"
#include "options_scoreboard.h"
#include "hud.h"
#include "cvar_check_button.h"
#include "cvar_text_entry.h"

CScoreboardSubOptions::CScoreboardSubOptions(vgui2::Panel *parent)
    : BaseClass(parent, "ScoreboardSubOptions")
{
	SetSize(100, 100); // Silence "parent not sized yet" warning

	m_pShowAvatars = new CCvarCheckButton(this, "ShowAvatars", "#BHL_AdvOptions_Scores_ShowAvatars", "hud_scoreboard_showavatars");
	m_pShowSteamId = new CCvarCheckButton(this, "ShowSteamId", "#BHL_AdvOptions_Scores_ShowSteamId", "hud_scoreboard_showsteamid");
	m_pShowPacketLoss = new CCvarCheckButton(this, "ShowPacketLoss", "#BHL_AdvOptions_Scores_ShowLoss", "hud_scoreboard_showloss");

	m_pShowEff = new CCvarCheckButton(this, "ShowEff", "#BHL_AdvOptions_Scores_ShowEff", "hud_scoreboard_showeff");
	m_pEffTypeLabel = new vgui2::Label(this, "EffTypeLabel", "#BHL_AdvOptions_Scores_EffType");
	m_pEffTypeBox = new vgui2::ComboBox(this, "EffTypeBox", 3, false);
	m_EffTypeItems[0] = m_pEffTypeBox->AddItem("#BHL_AdvOptions_Scores_EffType0", new KeyValues("Type0", "value", 0));
	m_EffTypeItems[1] = m_pEffTypeBox->AddItem("#BHL_AdvOptions_Scores_EffType1", new KeyValues("Type1", "value", 1));
	m_EffTypeItems[2] = m_pEffTypeBox->AddItem("#BHL_AdvOptions_Scores_EffType2", new KeyValues("Type2", "value", 2));

	m_pMouseLabel = new vgui2::Label(this, "MouseLabel", "#BHL_AdvOptions_Scores_Mouse");
	m_pMouseBox = new vgui2::ComboBox(this, "MouseBox", 3, false);
	m_MouseItems[0] = m_pMouseBox->AddItem("#BHL_AdvOptions_Scores_Mouse0", new KeyValues("None", "value", 0));
	m_MouseItems[1] = m_pMouseBox->AddItem("#BHL_AdvOptions_Scores_Mouse1", new KeyValues("MOUSE1", "value", 1));
	m_MouseItems[2] = m_pMouseBox->AddItem("#BHL_AdvOptions_Scores_Mouse2", new KeyValues("MOUSE2", "value", 2));

	m_pSizeLabel = new vgui2::Label(this, "SizeLabel", "#BHL_AdvOptions_Scores_Size");
	m_pSizeBox = new vgui2::ComboBox(this, "SizeBox", 3, false);
	m_SizeItems[0] = m_pSizeBox->AddItem("#BHL_AdvOptions_Scores_Size0", new KeyValues("Auto", "value", 0));
	m_SizeItems[1] = m_pSizeBox->AddItem("#BHL_AdvOptions_Scores_Size1", new KeyValues("Large", "value", 1));
	m_SizeItems[2] = m_pSizeBox->AddItem("#BHL_AdvOptions_Scores_Size2", new KeyValues("Compact", "value", 2));

	m_pShowInHud = new CCvarTextEntry(this, "ShowInHud", "hud_scores", CCvarTextEntry::CvarType::Int);

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/ScoreboardSubOptions.res");
}

void CScoreboardSubOptions::OnResetData()
{
	m_pShowAvatars->ResetData();
	m_pShowSteamId->ResetData();
	m_pShowPacketLoss->ResetData();
	m_pShowEff->ResetData();
	m_pShowInHud->ResetData();

	int type = gEngfuncs.pfnGetCvarFloat("hud_scoreboard_efftype");
	type = clamp(type, 0, 2);
	m_pEffTypeBox->ActivateItem(m_EffTypeItems[type]);

	type = gEngfuncs.pfnGetCvarFloat("hud_scoreboard_mousebtn");
	type = clamp(type, 0, 2);
	m_pMouseBox->ActivateItem(m_MouseItems[type]);

	type = gEngfuncs.pfnGetCvarFloat("hud_scoreboard_size");
	type = clamp(type, 0, 2);
	m_pSizeBox->ActivateItem(m_SizeItems[type]);
}

void CScoreboardSubOptions::OnApplyChanges()
{
	m_pShowAvatars->ApplyChanges();
	m_pShowSteamId->ApplyChanges();
	m_pShowPacketLoss->ApplyChanges();
	m_pShowEff->ApplyChanges();
	m_pShowInHud->ApplyChanges();
	ApplyEffType();
	ApplyMouse();
	ApplySize();
}

void CScoreboardSubOptions::ApplyEffType()
{
	KeyValues *userdata = m_pEffTypeBox->GetActiveItemUserData();
	Assert(userdata);
	int val = userdata->GetInt("value", 0);

	char buf[128];
	snprintf(buf, sizeof(buf), "hud_scoreboard_efftype %d", val);
	gEngfuncs.pfnClientCmd(buf);
}

void CScoreboardSubOptions::ApplyMouse()
{
	KeyValues *userdata = m_pMouseBox->GetActiveItemUserData();
	Assert(userdata);
	int val = userdata->GetInt("value", 0);
	Assert(val >= 0 && val <= 2);

	char buf[128];
	snprintf(buf, sizeof(buf), "hud_scoreboard_mousebtn %d", val);
	gEngfuncs.pfnClientCmd(buf);
}

void CScoreboardSubOptions::ApplySize()
{
	KeyValues *userdata = m_pSizeBox->GetActiveItemUserData();
	Assert(userdata);
	int val = userdata->GetInt("value", 0);
	Assert(val >= 0 && val <= 2);

	char buf[128];
	snprintf(buf, sizeof(buf), "hud_scoreboard_size %d", val);
	gEngfuncs.pfnClientCmd(buf);
}
