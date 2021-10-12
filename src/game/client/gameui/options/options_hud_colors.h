#ifndef OPTIONS_HUD_COLOR_H
#define OPTIONS_HUD_COLOR_H
#include <vgui_controls/PropertyPage.h>

class CCvarColor;

class CHudSubOptionsColors : public vgui2::PropertyPage
{
public:
	DECLARE_CLASS_SIMPLE(CHudSubOptionsColors, vgui2::PropertyPage);

	CHudSubOptionsColors(vgui2::Panel *parent);

	virtual void OnResetData();
	virtual void OnApplyChanges();

private:
	vgui2::Label *m_pColorLabel[4];
	CCvarColor *m_pColorValue[4];
};

#endif
