#ifndef OPTIONS_HUD_ROOT_H
#define OPTIONS_HUD_ROOT_H
#include <vgui_controls/PropertySheet.h>

class CHudSubOptions;
class CHudSubOptionsColors;

class CHudSubOptionsRoot : public vgui2::PropertySheet
{
public:
	DECLARE_CLASS_SIMPLE(CHudSubOptionsRoot, vgui2::PropertySheet);

	CHudSubOptionsRoot(vgui2::Panel *parent);

private:
	CHudSubOptions *m_pOptions = nullptr;
	CHudSubOptionsColors *m_pColors = nullptr;

	MESSAGE_FUNC(OnResetData, "ResetData");
	MESSAGE_FUNC(OnApplyChanges, "ApplyChanges");
};

#endif
