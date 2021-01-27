#ifndef COLORPICKER_CPICKERPANEL_H
#define COLORPICKER_CPICKERPANEL_H
#include <vgui_controls/EditablePanel.h>

namespace vgui2
{
class ImagePanel;
}

class CColorPicker;

namespace colorpicker
{

class CSelCirclePanel;
class CPickerImagePanel;
class CPickerMousePanel;

class CPickerPanel : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CPickerPanel, vgui2::EditablePanel);
public:
	CPickerPanel(vgui2::Panel *pParent, const char *panelName);
	virtual ~CPickerPanel();
	void ApplySchemeSettings(vgui2::IScheme *pScheme);
	void SetCirclePos(int x, int y);

	void SetHSV(float hue, float sat, float val);
	void SetHue(float hue);
	void GetHSV(float &hue, float &sat, float &val);
	CColorPicker *GetParentPicker();

private:
	CPickerImagePanel *m_pImagePanel = nullptr;
	CSelCirclePanel *m_pCircle = nullptr;
	CPickerMousePanel *m_pMousePanel = nullptr;
	float m_flHue = 0, m_flSat = 0, m_flVal = 0;

	friend class CPickerMousePanel;
};

}

#endif
