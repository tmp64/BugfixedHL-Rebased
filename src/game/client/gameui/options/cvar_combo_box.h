#ifndef CVAR_COMBO_BOX_H
#define CVAR_COMBO_BOX_H
#include <vgui_controls/ComboBox.h>
#include <cvardef.h>

class CCVarComboBox : public vgui2::ComboBox
{
	DECLARE_CLASS_SIMPLE(CCVarComboBox, vgui2::ComboBox);

public:
	CCVarComboBox(vgui2::Panel *parent, const char *panelName, const char *cvarName);

	//! Adds an item to the combo box.
	//! @param	itemText	Disaply text. Can be localized.
	//! @param	cvarValue	CVar value.
	//! @returns Item ID.
	int AddItem(const char *itemText, const char *cvarValue);

	void ResetData();
	void ApplyChanges();

private:
	cvar_t *m_pCvar = nullptr;
};

#endif
