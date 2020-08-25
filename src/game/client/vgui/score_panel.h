#ifndef CSCOREPANEL_H
#define CSCOREPANEL_H

#include <array>
#include <map>
#include <steam/steam_api.h>
#include <tier1/utlmap.h>
#include <vgui_controls/Frame.h>
#include <cdll_dll.h>
#include "IViewportPanel.h"
#include "viewport_panel_names.h"

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

	struct TeamData
	{
		int iFrags = 0;
		int iDeaths = 0;
		int iPlayerCount = 0;
	};

	struct PlayerData
	{
		bool bIsConnected = false;
		int nItemID = 0;
		int nTeamID = 0;
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
	 * Resize and reposition the scoreboard.
	 */
	void Resize();

	/**
	 * Sorts rows in DESC order by frags.
	 */
	static bool StaticPlayerSortFuncByFrags(vgui2::SectionedListPanel *list, int itemID1, int itemID2);
};

#endif