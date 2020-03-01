#ifndef VGUI_CLIENT_VIEWPORT_H
#define VGUI_CLIENT_VIEWPORT_H
#include <vgui_controls/Frame.h>

class CClientViewport : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CClientViewport, vgui2::EditablePanel);

public:
	CClientViewport();
	bool LoadHudAnimations();
	void ReloadScheme(const char *fromFile);
	void ActivateClientUI();
	void HideClientUI();

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
	void MsgFunc_SpecFade(const char *pszName, int iSize, void *pbuf);
	void MsgFunc_ResetFade(const char *pszName, int iSize, void *pbuf);

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
	bool IsScoreBoardVisible(void);
	void ShowScoreBoard(void);
	void HideScoreBoard(void);
	void UpdateSpectatorPanel();
	int KeyInput(int down, int keynum, const char *pszCurrentBinding);

private:
	vgui2::AnimationController *m_pAnimController = nullptr;
};

extern CClientViewport *g_pViewport;

#endif