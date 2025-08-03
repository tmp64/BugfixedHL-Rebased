#ifndef VGUI_HUD_BATTERY_H
#define VGUI_HUD_BATTERY_H

#include <vgui_controls/EditablePanel.h>
#include "global_consts.h"
#include "IViewportPanel.h"
#include "viewport_panel_names.h"

namespace vgui2
{
class Label;
}

class CHudBatteryPanel : public vgui2::EditablePanel, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CHudBatteryPanel, vgui2::EditablePanel);

	CHudBatteryPanel();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PaintBackground() override;

	void UpdateBatteryPanel(int amount);

	void OnThink() override;

	//IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

private:
	vgui2::Label *m_pBatteryAmount = nullptr;
	vgui2::Label *m_pBatteryAmountBg = nullptr;
	vgui2::Label *m_pBatteryAmountGlow = nullptr;
	vgui2::Label *m_pBatteryIcon = nullptr;

	float m_fFade = 0.0f;
	int m_iBattery = 100;
};

#endif
