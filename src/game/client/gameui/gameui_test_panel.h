#ifndef GAMEUI_TEST_PANEL_H
#define GAMEUI_TEST_PANEL_H
#include <vgui_controls/Frame.h>
#include <vgui_controls/RichText.h>

class CGameUITestPanel : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CGameUITestPanel, vgui2::Frame);

public:
	CGameUITestPanel(vgui2::Panel *pParent);

	virtual void PerformLayout() override;
	virtual void Activate() override;

private:
	vgui2::RichText *m_pText = nullptr;
};

#endif
