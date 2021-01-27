#ifndef CCVARTEXTENTRY_H
#define CCVARTEXTENTRY_H
#include <vgui_controls/Slider.h>

typedef struct cvar_s cvar_t;

class CCvarSlider : public vgui2::Slider
{
	DECLARE_CLASS_SIMPLE(CCvarSlider, vgui2::Slider);
public:
	CCvarSlider(vgui2::Panel *parent, const char *panelName, const char *cvarName);
	void ResetData();
	void ApplyChanges();

private:
	cvar_t *m_pCvar = nullptr;
};

#endif
