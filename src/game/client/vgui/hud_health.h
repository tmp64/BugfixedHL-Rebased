#ifndef VGUI_HUD_HEALTH_H
#define VGUI_HUD_HEALTH_H

#include <vgui_controls/EditablePanel.h>
#include "global_consts.h"
#include "IViewportPanel.h"
#include "viewport_panel_names.h"

namespace vgui2
{
class Label;
}

class CHudHealthPanel : public vgui2::EditablePanel, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CHudHealthPanel, vgui2::EditablePanel);

	CHudHealthPanel();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PaintBackground() override;

	void UpdateHealthPanel(int health);

	void OnThink() override;

	//IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

private:
	vgui2::Label *m_pHealthAmount = nullptr;
	vgui2::Label *m_pHealthAmountBg = nullptr;
	vgui2::Label *m_pHealthAmountGlow = nullptr;
	vgui2::Label *m_pHealthIcon = nullptr;

	float m_fFade = 0.0f;
	int m_iHealth = 100;

	ConVarRef m_pHudDim{"hud_dim"};
};

#endif
