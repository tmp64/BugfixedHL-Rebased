#ifndef HUD_KILLFEED_H
#define HUD_KILLFEED_H
#include <vector>
#include <vgui_controls/Panel.h>
#include "hud/base.h"

class CHudDeathNoticePanel : public CHudElemBase<CHudDeathNoticePanel>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CHudDeathNoticePanel, vgui2::Panel);

	CHudDeathNoticePanel();

	void VidInit() override;
	void InitHudData() override;
	void Think() override;

	void AddItem(int killerId, int victimId, const char *killedwith);

	void ApplySettings(KeyValues *inResourceData) override;
	void PaintBackground() override;
	void Paint() override;

private:
	enum class EntryType : uint8_t
	{
		Other, //!< Other players
		Kill, //!< Kill of this player
		Death, //!< Death of this player
	};

	struct Entry
	{
		wchar_t wszKiller[MAX_PLAYER_NAME + 1] = { 0 };
		wchar_t wszVictim[MAX_PLAYER_NAME + 1] = { 0 };
		float flEndTime = 0;
		int iKillerLen = 0;
		int iKillerWide = 0;
		int iVictimLen = 0;
		int iVictimWide = 0;
		Color killerColor = Color(0, 0, 0, 0);
		Color victimColor = Color(0, 0, 0, 0);
		EntryType type = EntryType::Other;
		bool bIsSuicide = false;
		bool bIsTeamKill = false;
		int nSpriteIdx = 0;
		int iSpriteWide = 0;
	};

	int m_HUD_d_skull = 0; // sprite index of skull icon
	int m_nActiveList = 0;
	std::vector<Entry> m_EntryList[2];
	int m_iEntryCount = 0;

	CPanelAnimationVarAliasType(int, m_iHPadding, "hor_padding", "12", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iIconPadding, "icon_padding", "2", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iVMargin, "vert_margin", "4", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iRowHeight, "row_height", "13", "proportional_int");

	CPanelAnimationVar(vgui2::HFont, m_TextFont, "text_font", "Default");

	CPanelAnimationVar(Color, m_ColorBg, "color_bg", "0 0 0 64");
	CPanelAnimationVar(Color, m_ColorBgKill, "color_bg_kill", "255 119 0 96");
	CPanelAnimationVar(Color, m_ColorBgDeath, "color_bg_death", "255 0 0 96");

	CPanelAnimationVar(Color, m_ColorIcon, "color_icon", "255 80 0 255");
	CPanelAnimationVar(Color, m_ColorIconKill, "color_icon_kill", "255 80 0 255");
	CPanelAnimationVar(Color, m_ColorIconDeath, "color_icon_death", "241 237 212 255");
	CPanelAnimationVar(Color, m_ColorIconTK, "color_icon_teamkill", "10 240 10 255");

	CPanelAnimationVar(Color, m_ColorNameDefault, "default_name_color", "Orange");
	CPanelAnimationVar(Color, m_ColorNameKill, "default_name_color_kill", "Orange");
	CPanelAnimationVar(Color, m_ColorNameDeath, "default_name_color_death", "Orange");

	int m_iRowTall = 0;

	int GetEntryContentWide(const Entry &e);
	int GetColoredTextWide(const wchar_t *str, int len);
	int DrawColoredText(int x, int y, const wchar_t *str, int len, Color c);
};

#endif
