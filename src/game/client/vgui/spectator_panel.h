#ifndef VGUI_SPECTATOR_PANEL_H
#define VGUI_SPECTATOR_PANEL_H

#include <vgui_controls/EditablePanel.h>
#include "global_consts.h"
#include "IViewportPanel.h"
#include "viewport_panel_names.h"

namespace vgui2
{
class HTML;
class Label;
class RichText;
}

class CSpectatorInfoPanel;
class CPlayerInfo;
class CAvatarImage;

class CSpectatorPanel : public vgui2::EditablePanel, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CSpectatorPanel, vgui2::EditablePanel);

	CSpectatorPanel();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PaintBackground() override;

	void UpdateSpectatingPlayer(int iPlayerIdx);
	void EnableInsetView(bool bIsEnabled);

	//IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

private:
	struct PlayerData
	{
		CPlayerInfo *pi = nullptr;
		CAvatarImage *pInfoAvatar = nullptr;
	};

	struct InsetData
	{
		bool bDraw = false;
		int iX, iY, iWide, iTall;
	};

	PlayerData m_PlayerData[MAX_PLAYERS + 1];
	CSpectatorInfoPanel *m_pInfoPanel = nullptr;
	InsetData m_Inset;

	CPanelAnimationVar(Color, m_InsetColor, "avatar_border_color", "59 58 34 207");

	void FillViewport();

	friend class CSpectatorInfoPanel;
};

#endif
