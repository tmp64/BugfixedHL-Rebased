#ifndef HUD_MENU_H
#define HUD_MENU_H
#include "base.h"

class CHudMenu : public CHudElemBase<CHudMenu>
{
public:
	void Init();
	void InitHudData();
	void VidInit();
	void Reset();
	void Draw(float flTime);
	int MsgFunc_ShowMenu(const char *pszName, int iSize, void *pbuf);

	void SelectMenuItem(int menu_item);

	int m_fMenuDisplayed;
	int m_bitsValidSlots;
	float m_flShutoffTime;
	int m_fWaitingForMore;
};

#endif
