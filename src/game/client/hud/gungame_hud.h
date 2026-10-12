#ifndef HUD_GUNGAME_HUD_H
#define HUD_GUNGAME_HUD_H
#include <vgui_controls/Panel.h>
#include "hud/base.h"
#include "scoreboard_data.h"

/**
 * GunGame HUD widget: the place block at the top center and the level bar at the bottom center.
 * Shows data of the scoreboard extension store (docs/scoreboard-protocol.md, 8.4).
 */
class CHudGunGamePanel : public CHudElemBase<CHudGunGamePanel>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CHudGunGamePanel, vgui2::Panel);

	CHudGunGamePanel();

	//! True when the gungame preset is active and the widget is enabled in the settings
	static bool IsWidgetEnabled();

	//! Screen Y of the top edge of the level bar or -1 if there is no level bar
	static int GetLevelBarTop();

	//! How far the speedometer and jumpspeed at the bottom must move up to stay above the level bar
	static int GetLevelBarOffset();

	void Init() override;
	void VidInit() override;
	void Think() override;
	void PaintBackground() override;
	void Paint() override;

private:
	CPanelAnimationVarAliasType(int, m_iPlaceY, "place_ypos", "12", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iLineGap, "line_gap", "2", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iMeterGap, "meter_gap", "6", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iDotSize, "dot_size", "7", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iDotGap, "dot_gap", "3", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCounterGap, "counter_gap", "6", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellWide, "cell_wide", "30", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellMinWide, "cell_min_wide", "14", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellTall, "cell_tall", "15", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellGap, "cell_gap", "3", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iIconPadding, "icon_padding", "2", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBarMaxWide, "bar_max_wide", "300", "proportional_int");

	CPanelAnimationVar(int, m_iAlphaPassed, "alpha_passed", "112");
	CPanelAnimationVar(int, m_iAlphaCurrent, "alpha_current", "48");
	CPanelAnimationVar(int, m_iAlphaFuture, "alpha_future", "24");
	CPanelAnimationVar(int, m_iAlphaDim, "alpha_dim", "80");

	CPanelAnimationVar(vgui2::HFont, m_LargeFont, "large_font", "GunGameHudLarge");
	CPanelAnimationVar(vgui2::HFont, m_TextFont, "text_font", "GunGameHudText");

	int m_iTarget = 0; //!< Player whose data is shown
	int m_iTeam = 0; //!< Team of the target if places are counted among teams, otherwise 0

	wchar_t m_wszTitle[64] = {};
	wchar_t m_wszInfo[128] = {};
	int m_iInfoY = 0;

	int m_iMeterCur = 0;
	int m_iMeterMax = 0; //!< 0 if the kills meter is hidden
	int m_iMeterY = 0;

	int m_iLevelCount = 0; //!< 0 if the level bar is hidden
	int m_iLevel = 0; //!< Current level, 0-based
	int m_iBarFirst = 0; //!< First shown level
	int m_iBarShown = 0; //!< Number of shown levels
	int m_iBarX = 0;
	int m_iBarY = 0;
	int m_iBarCellWide = 0;

	int m_iSpriteSerial = -1;
	int m_iSprites[bhl::sb::MAX_LIST_ITEMS] = {};

	int m_iDotTexture = -1;
	int m_iRingTexture = -1;
	int m_iCapTextures[2] = { -1, -1 };

	bool UpdateState();
	void UpdatePlace();
	void UpdateLevelBar();

	//! Cell of the target or, if it is empty, of the target's team
	const CScoreboardData::Value &GetCell(const char *key);

	Color GetColor(int alpha);
	int GetMeterBarWide();
	int GetLevelBarWide();
	void DrawRoundedBar(int x, int y, int wide, Color color);
	void DrawCenteredText(vgui2::HFont font, int y, const wchar_t *text, Color color);
};

#endif
