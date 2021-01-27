#ifndef CCVARCOLOR_H
#define CCVARCOLOR_H
#include <vgui_controls/EditablePanel.h>

namespace vgui2
{
class Button;
}
class CColorPicker;

typedef struct cvar_s cvar_t;

//-------------------------------------------------------------
// For cvars that use "RRR GGG BBB" format (like hud_color)
// set cvarName to cvar name and call ResetData() and ApplyChanges()
//
// For cvars that use custom format set cvarName to nullptr
// and use SetInitialColor and GetSelectedColor.
// Make sure to call SetInitialColor again after applying the color.
//-------------------------------------------------------------
class CCvarColor : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CCvarColor, vgui2::EditablePanel);

public:
	CCvarColor(vgui2::Panel *parent, const char *panelName, const char *cvarName, const char *cvarTitle);
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void OnCommand(const char *pCmd);

	void ResetData();
	void ApplyChanges();

	void SetInitialColor(Color c);
	Color GetSelectedColor();

private:
	cvar_t *m_pCvar = nullptr;
	vgui2::Panel *m_pPreview = nullptr;
	vgui2::Button *m_pBtn = nullptr;
	CColorPicker *m_pColorPicker = nullptr;
	Color m_NewColor;

	MESSAGE_FUNC_PARAMS(OnColorPicked, "ColorPicked", kv);
};

#endif
