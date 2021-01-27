#ifndef COLORPICKER_CSELCIRCLEPANEL
#define COLORPICKER_CSELCIRCLEPANEL
#include <vgui_controls/ImagePanel.h>

namespace colorpicker
{

class CSelCircleImage;

class CSelCirclePanel : public vgui2::ImagePanel
{
	DECLARE_CLASS_SIMPLE(CSelCirclePanel, vgui2::ImagePanel);

public:
	CSelCirclePanel(vgui2::Panel *pParent, const char *panelName);
	virtual ~CSelCirclePanel();
	void ApplySchemeSettings(vgui2::IScheme *pScheme);

private:
	CSelCircleImage *m_pImage = nullptr;
};

}

#endif
