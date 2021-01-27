#ifndef CCVARCHECKBUTTON_H
#define CCVARCHECKBUTTON_H
#include <vgui_controls/CheckButton.h>

typedef struct cvar_s cvar_t;

class CCvarCheckButton : public vgui2::CheckButton
{
	DECLARE_CLASS_SIMPLE(CCvarCheckButton, vgui2::CheckButton);
public:
	CCvarCheckButton(vgui2::Panel *parent, const char *panelName, const char *text, const char *cvarName, bool inverse = false);
	void ResetData();
	void ApplyChanges();

private:
	cvar_t *m_pCvar = nullptr;
	bool m_bInverse = false;
};

#endif
