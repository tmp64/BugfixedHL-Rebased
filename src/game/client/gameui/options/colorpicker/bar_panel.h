#ifndef COLORPICKER_CBARPANEL_H
#define COLORPICKER_CBARPANEL_H
#include <vgui_controls/EditablePanel.h>

namespace vgui2
{
class ImagePanel;
}

class CColorPicker;

namespace colorpicker
{

class CSelCirclePanel;
class CBarImagePanel;
class CBarMousePanel;

class CBarPanel : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CBarPanel, vgui2::EditablePanel);
public:
	CBarPanel(vgui2::Panel *pParent, const char *panelName);
	virtual ~CBarPanel();
	void ApplySchemeSettings(vgui2::IScheme *pScheme);
	void SetCirclePos(int x);

	void SetHue(float hue);
	float GetHue();
	CColorPicker *GetParentPicker();

private:
	CBarImagePanel *m_pImagePanel = nullptr;
	CSelCirclePanel *m_pCircle = nullptr;
	CBarMousePanel *m_pMousePanel = nullptr;

	friend class CBarMousePanel;
};

}

#endif
