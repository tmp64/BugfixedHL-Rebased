#ifndef CADVOPTIONSDIALOG_H
#define CADVOPTIONSDIALOG_H

#include <vgui/VGUI2.h>
#include <vgui_controls/PropertyDialog.h>

class CAdvOptionsDialog : public vgui2::PropertyDialog
{
public:
	DECLARE_CLASS_SIMPLE(CAdvOptionsDialog, vgui2::PropertyDialog);

	CAdvOptionsDialog(vgui2::Panel *pParent);

	virtual void OnCommand(const char *command);
};

#endif
