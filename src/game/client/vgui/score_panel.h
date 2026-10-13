#ifndef CSCOREPANEL_H
#define CSCOREPANEL_H

#include <array>
#include <map>
#include <vector>
#include <steam/steam_api.h>
#include <tier1/utlmap.h>
#include <vgui_controls/Frame.h>
#include <cdll_dll.h>
#include "IViewportPanel.h"
#include "viewport_panel_names.h"
#include "vgui/score_panel_cells.h"

namespace vgui2
{
class Label;
class Menu;
class CheckButton;
class SectionedListPanel;
class ImageList;
}

class CAvatarImage;

class CScorePanel : public vgui2::Frame, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CScorePanel, vgui2::Frame);

	CScorePanel();

	/**
	 * Updates server name label. Called on ServerName message.
	 */
	void UpdateServerName();

	/**
	 * Shows or hides mouse pointer.
	 */
	void EnableMousePointer(bool bEnable);

	/**
	 * Updates player's score in the scoreboard.
	 */
	void UpdateOnPlayerInfo(int client);

	/**
	 * Highlightes the killer of they killed this player.
	 */
	void DeathMsg(int killer, int victim);

	// Frame overrides
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void OnThink() override;
	virtual void OnCommand(const char *command) override;
	virtual void PaintBackground() override;
	virtual void Paint() override;

	// IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

	// Messages
	MESSAGE_FUNC_INT(OnItemContextMenu, "ItemContextMenu", itemID);

private:
	static constexpr int TEAM_SPECTATOR = MAX_TEAMS + 1;
	static constexpr int HEADER_SECTION_ID = 0;
	static constexpr float HIGHLIGHT_KILLER_TIME = 10.f;
	static constexpr float UPDATE_PERIOD = 0.5f;

	//! Update period while the store has a running Clock, so its seconds change on time
	static constexpr float CLOCK_UPDATE_PERIOD = 0.1f;

	//! First index of custom cell images in m_pImageList (avatars use 1..MAX_PLAYERS)
	static constexpr int CELL_IMAGE_BASE = MAX_PLAYERS + 1;

	//! Width of a row is the width of the list minus this padding (SectionedListPanel::LayoutPanels)
	static constexpr int LIST_ROW_PADDING = 10;

	//! Gap at the right of a cell (COLUMN_DATA_GAP of SectionedListPanel)
	static constexpr int COLUMN_DATA_GAP = 2;

	enum class MenuAction
	{
		Mute,
		SteamProfile,
		SteamURL,
		CopyName,
		CopyNameRaw,
		CopySteamID,
		CopySteamID64,
	};

	enum class SizeMode
	{
		Auto = 0,
		Normal = 1,
		Compact = 2,
	};

	class CSectionHeader;

	struct TeamData
	{
		int iFrags = 0;
		int iDeaths = 0;
		int iPlayerCount = 0;
	};

	struct PlayerData
	{
		bool bIsConnected = false;
		int nItemID = -1;
		int nTeamID = 0;
	};

	enum class ColumnType
	{
		Rank,
		Avatar,
		Name,
		SteamID,
		Eff,
		Frags,
		Deaths,
		Ping,
		Custom,
	};

	struct CustomColumn
	{
		ColumnType type = ColumnType::Custom;
		int nColumnID = -1; //!< Column id in the store for ColumnType::Custom
		int nSlot = -1; //!< Cell image slot of a custom column, -1 for a standard one
		int iAlign = 0; //!< 0 left, 1 center, 2 right
		int iFlags = 0; //!< SectionedListPanel column flags
		int iWidth = 0;
		char szName[32] = {}; //!< Column name, also the key in row data
		wchar_t wszTitle[64] = {};
	};

	//! Row of the banner in the header (docs/scoreboard-protocol.md, 6.8)
	struct BannerRow
	{
		wchar_t wszText[bhl::sb::TEMPLATE_BUF_LEN] = {}; //!< Evaluated text, "" when the row is not drawn
		Color color;
		int x = 0, y = 0, wide = 0, tall = 0; //!< Set by LayoutBanner
	};

	struct MenuData
	{
		// Menu items
		int nMuteItemID;
		int nProfilePageItemID;
		int nProfileUrlItemID;

		// Selected player info
		int nItemID;
		int nClient;
		uint64 nSteamID64;
	};

	vgui2::SectionedListPanel *m_pPlayerList = nullptr;
	vgui2::Label *m_pServerNameLabel = nullptr;
	vgui2::Label *m_pMapNameLabel = nullptr;
	vgui2::Label *m_pPlayerCountLabel = nullptr;
	vgui2::ImageList *m_pImageList = nullptr;
	vgui2::Menu *m_pPlayerMenu = nullptr;

	std::array<TeamData, MAX_TEAMS + 2> m_TeamData;
	std::array<bool, MAX_TEAMS + 2> m_IsTeamSectionCreated;
	std::array<PlayerData, MAX_PLAYERS + 1> m_PlayerData;
	std::array<int, MAX_TEAMS + 1> m_SortedTeamIDs;
	std::map<CSteamID, CAvatarImage *> m_PlayerAvatars;
	MenuData m_MenuData;

	int m_iKillerIndex = 0;
	float m_flKillerHighlightStart = 0;
	float m_flLastUpdateTime = 0;

	Color m_ThisPlayerBgColor = Color(0, 0, 0, 0);
	Color m_KillerBgColor = Color(0, 0, 0, 0);

	char m_szSpectatorTag[32] = "(spectator)";

	// Custom scoreboard (docs/scoreboard-protocol.md)
	BannerRow m_BannerTop; //!< On the line of the server name
	BannerRow m_BannerBottom; //!< On the line of the map name
	std::vector<CustomColumn> m_CustomColumns;
	std::array<CSectionHeader *, MAX_TEAMS + 2> m_SectionHeaders = {}; //!< Headers of the sections by team
	ScoreCellStyle m_CellStyle;
	wchar_t m_wszTitle[bhl::sb::MAX_TITLE_LEN + 1] = {}; //!< Drawn in place of the server name
	wchar_t m_wszTag[32] = {};
	bool m_bCustom = false;
	bool m_bSectionsDirty = false; //!< A section was created outside of RefreshItems
	int m_iStoreSerial = -1;
	int m_iSchemaSerial = -1;
	int m_iSortedTeamCount = 0;
	int m_iDefaultLineSpacing = 0;
	int m_iLineSpacingExtra = 0; //!< Added to the line spacing while a Steps column is visible
	int m_iServerNameWide = -1; //!< Width of the server name label before the banner cut it

	// Column widths
	CPanelAnimationVarAliasType(int, m_iColumnWidthAvatar, "column_avatar", "26", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthName, "column_name", "170", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthSteamID, "column_steamid", "88", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthEff, "column_eff", "60", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthFrags, "column_frags", "60", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthDeaths, "column_deaths", "60", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthPing, "column_ping", "80", "proportional_int");

	CPanelAnimationVarAliasType(int, m_iMutedIconTexture, "muted_icon", "ui/gfx/muted_icon32", "textureid");
	CPanelAnimationVarAliasType(int, m_iMinHeight, "min_height", "200", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iVerticalMargin, "vertical_margin", "30", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBottomPadding, "bottom_padding", "4", "proportional_int");

	// Custom scoreboard
	CPanelAnimationVarAliasType(int, m_iColumnWidthRank, "column_rank", "24", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iColumnWidthNameMin, "column_name_min", "100", "proportional_int");
	CPanelAnimationVar(vgui2::HFont, m_CellFont, "cell_font", "Default");
	CPanelAnimationVar(vgui2::HFont, m_BadgeFont, "badge_font", "DefaultVerySmall");
	CPanelAnimationVar(Color, m_CellFilledColor, "cell_filled_color", "255 160 0 255");
	CPanelAnimationVar(Color, m_CellEmptyColor, "cell_empty_color", "84 56 16 255");
	CPanelAnimationVar(Color, m_CellCurrentColor, "cell_current_color", "255 255 255 255");
	CPanelAnimationVar(Color, m_CellLastColor, "cell_last_color", "255 64 32 255");
	CPanelAnimationVar(Color, m_CellIconColor, "cell_icon_color", "255 160 0 255");
	CPanelAnimationVar(Color, m_BadgeBgColor, "badge_bgcolor", "255 160 0 255");
	CPanelAnimationVar(Color, m_BadgeFgColor, "badge_fgcolor", "0 0 0 255");
	CPanelAnimationVarAliasType(int, m_iCellDotSize, "cell_dot_size", "6", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellDotGap, "cell_dot_gap", "2", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellStripTall, "cell_strip_tall", "2", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellStripGap, "cell_strip_gap", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellSpacing, "cell_spacing", "3", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iCellMargin, "cell_margin", "6", "proportional_int");

	// Banner in the header, its rows are elements placed BannerTop and BannerBottom
	CPanelAnimationVar(Color, m_BannerBgColor, "banner_bgcolor", "255 64 32 48");
	CPanelAnimationVar(Color, m_BannerAccentColor, "banner_accent_color", "255 160 0 255");
	CPanelAnimationVarAliasType(int, m_iBannerPadding, "banner_padding", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBannerAccentWide, "banner_accent_wide", "2", "proportional_int");

	/**
	 * Updates all labels, recreates all sections, recalculates all data.
	 * Called when panel becomes visible.
	 */
	void FullUpdate();

	/**
	 * Updates map name label.
	 */
	void UpdateMapName();

	/**
	 * Clears and recreates all sections and rows.
	 */
	void RefreshItems();

	/**
	 * Creates a section for specified team.
	 */
	void CreateSection(int nTeamID);

	/**
	 * Adds/removes/updates rows of all clients.
	 * Updates team player counts.
	 * Resizes the panel.
	 */
	void UpdateAllClients();

	/**
	 * Updates client's row in the scoreboard.
	 */
	void UpdateClientInfo(int client);

	/**
	 * Updates m_pImageList[i]: changes muted state and updates Steam avatar.
	 */
	void UpdateClientIcon(CPlayerInfo *pi);

	/**
	 * Updates team scores and player counts.
	 */
	void UpdateScoresAndCounts();

	/**
	 * Returns width of name column + widths of disabled columns.
	 */
	int GetNameColumnWidth();

	/**
	 * Returns player team [0; MAX_TEAMS] or TEAM_SPECTATOR.
	 */
	int GetPlayerTeam(CPlayerInfo *pi);

	/**
	 * Returns bright color if this is this player, fading red if it's the last killer or (0,0,0,0).
	 */
	Color GetPlayerBgColor(CPlayerInfo *pi);

	/**
	 * Calculates efficiency based on hud_scoreboard_efftype.
	 */
	float CalculateEfficiency(int kills, int deaths);

	/**
	 * Returns client icon size.
	 */
	int GetClientIconSize();

	/**
	 * Creates player context menu that is shown on right-click.
	 */
	void CreatePlayerMenu();

	/**
	 * Opens context menu for specified item.
	 */
	void OpenPlayerMenu(int itemID);

	/**
	 * Handles player menu action.
	 */
	void OnPlayerMenuCommand(MenuAction command);

	/**
	 * Returns current size mode.
	 */
	SizeMode GetSizeMode();

	/**
	 * Returns compact line spacing for given screen height
	 */
	int GetLineSpacingForHeight(int h);

	/**
	 * Returns line spacing override value for normal size.
	 */
	int GetLineSpacingForNormal();

	/**
	 * Returns line spacing override value for compact size.
	 */
	int GetLineSpacingForCompact();

	/**
	 * Restores default size.
	 */
	void RestoreSize();

	/**
	 * Resize and reposition the scoreboard.
	 */
	void Resize();

	/**
	 * Sorts rows in DESC order by frags.
	 */
	static bool StaticPlayerSortFuncByFrags(vgui2::SectionedListPanel *list, int itemID1, int itemID2);

	/**
	 * Returns true if team ilhs goes before team irhs.
	 */
	bool IsTeamBefore(int ilhs, int irhs);

	/**
	 * Returns true if the team sections are still in the order of IsTeamBefore.
	 */
	bool AreTeamsSorted();

	/**
	 * Builds m_CustomColumns.
	 */
	void BuildCustomColumns();

	/**
	 * Returns the cell image at an index of m_pImageList. Creates it on first use.
	 */
	CScoreCellImage *GetCellImage(int index);

	/**
	 * Fills custom cells of a player row.
	 */
	void UpdateCustomCells(CPlayerInfo *pi, KeyValues *kv);

	/**
	 * Fills custom cells of a team section header.
	 */
	void UpdateCustomTeamHeader(int nTeamID);

	/**
	 * Evaluates the banner rows. Lays the banner out again if a text changed.
	 */
	void UpdateBanner();

	/**
	 * Evaluates one banner row and sets its color.
	 * @param	schemeColor	Color of the row for an element with HudColor
	 * @return	true if its text changed
	 */
	bool UpdateBannerRow(BannerRow &row, bhl::sb::E_Place place, Color schemeColor);

	/**
	 * Places the banner in the header left of the player count and narrows the server name.
	 */
	void LayoutBanner();

	/**
	 * Sorts rows by the order of the custom scoreboard.
	 */
	static bool StaticPlayerSortFuncCustom(vgui2::SectionedListPanel *list, int itemID1, int itemID2);
};

#endif