#ifndef VGUI_TEAM_MENU_H
#define VGUI_TEAM_MENU_H
#include <vgui_controls/Frame.h>
#include "IViewportPanel.h"

#define MAX_TEAMS_IN_MENU 4

namespace vgui2
{
class Button;
class RichText;
}

class CTeamMenu : public vgui2::Frame, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CTeamMenu, Frame);

	CTeamMenu();
	virtual ~CTeamMenu();

	virtual void OnCommand(const char *pCommand) override;

	void Update();
	void Activate();

	//IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

private:
	vgui2::Button *m_pTeamButtons[MAX_TEAMS_IN_MENU + 1];
	vgui2::Button *m_pAutoAssignButton = nullptr;
	vgui2::Button *m_pSpectateButton = nullptr;
	vgui2::Button *m_pCancelButton = nullptr;
	vgui2::RichText *m_pBriefingText = nullptr;
	bool m_bUpdatedMapName = false;

	CPanelAnimationVarAliasType(int, m_iBtnX, "btn_x", "24", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBtnY, "btn_y", "28", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBtnSpacing, "btn_spacing", "8", "proportional_int");
};

#endif
