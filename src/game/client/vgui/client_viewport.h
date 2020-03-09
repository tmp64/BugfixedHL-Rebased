#ifndef VGUI_CLIENT_VIEWPORT_H
#define VGUI_CLIENT_VIEWPORT_H
#include <vector>
#include <vgui_controls/Frame.h>
#include <global_consts.h>
#include "IViewportPanel.h"

enum
{
	MENU_DEFAULT = 1,
	MENU_TEAM = 2,
	MENU_CLASS = 3,
	//MENU_MAPBRIEFING = 4,	// Removed
	MENU_MOTD = 5,
	MENU_CLASSHELP = 6,
	MENU_CLASSHELP2 = 7,
	MENU_REPEATHELP = 8,

	MENU_SPECHELP = 9,

	MENU_SPY = 12,
	MENU_SPY_SKIN = 13,
	MENU_SPY_COLOR = 14,
	MENU_ENGINEER = 15,
	MENU_ENGINEER_FIX_DISPENSER = 16,
	MENU_ENGINEER_FIX_SENTRYGUN = 17,
	MENU_ENGINEER_FIX_MORTAR = 18,
	MENU_DISPENSER = 19,
	MENU_CLASS_CHANGE = 20,
	MENU_TEAM_CHANGE = 21,

	MENU_REFRESH_RATE = 25,

	MENU_VOICETWEAK = 50
};

class CScorePanel;

class CClientViewport : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CClientViewport, vgui2::EditablePanel);

public:
	CClientViewport();
	void Start();
	bool LoadHudAnimations();
	void ReloadScheme(const char *fromFile);
	void ActivateClientUI();
	void HideClientUI();

	virtual void OnThink();

	void ShowVGUIMenu(int iMenu);
	void HideAllVGUIMenu();
	bool IsScoreBoardVisible();
	void ShowScoreBoard();
	void HideScoreBoard();

	const char *GetServerName();
	Color GetTeamColor(int team);

	// Panel accessors
	inline CScorePanel *GetScoreBoard()
	{
		return m_pScorePanel;
	}

	// TeamFortressViewport stubs
	void UpdateCursorState();
	void ShowCommandMenu(int menuIndex);
	void HideCommandMenu();
	void InputSignalHideCommandMenu();
	void InputPlayerSpecial(void);
	bool SlotInput(int iSlot);
	bool AllowedToPrintText(void);
	void DeathMsg(int killer, int victim);
	void GetAllPlayersInfo(void);
	void UpdateSpectatorPanel();
	int KeyInput(int down, int keynum, const char *pszCurrentBinding);

private:
	std::vector<IViewportPanel *> m_Panels;

	vgui2::AnimationController *m_pAnimController = nullptr;
	CScorePanel *m_pScorePanel = nullptr;

	int m_iNumberOfTeams = 0;
	int m_iAllowSpectators = 0;
	char m_szServerName[MAX_SERVERNAME_LENGTH] = "<ERROR>";

	int m_iGotAllMOTD = 0;
	char m_szMOTD[MAX_UNICODE_MOTD_LENGTH];

	Color m_pTeamColors[5] = {
		Color(216, 216, 216, 255), // "Off" white (default)
		Color(125, 165, 210, 255), // Blue
		Color(200, 90, 70, 255), // Red
		Color(225, 205, 45, 255), // Yellow
		Color(145, 215, 140, 255) // Green
	};

	// Panel handling
	void CreateDefaultPanels();
	void AddNewPanel(IViewportPanel *panel);

	void UpdateOnPlayerInfo(int client);

public:
	// Messages
	void MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_Feign(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_BuildSt(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_RandomPC(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf);
};

inline Color CClientViewport::GetTeamColor(int team)
{
	return m_pTeamColors[team % ARRAYSIZE(m_pTeamColors)];
}

extern CClientViewport *g_pViewport;

#endif