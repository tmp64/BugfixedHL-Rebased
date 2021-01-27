#ifndef CCROSSHAIRSUBOPTIONS_H
#define CCROSSHAIRSUBOPTIONS_H
#include <vgui_controls/PropertyPage.h>

namespace vgui2
{
class Label;
class ComboBox;
class ImagePanel;
}

class CCvarCheckButton;
class CCvarTextEntry;
class CCvarColor;
class CCrosshairPreview;

typedef struct cvar_s cvar_t;

class CCrosshairSubOptions : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CCrosshairSubOptions, vgui2::PropertyPage);

public:
	CCrosshairSubOptions(vgui2::Panel *parent);
	~CCrosshairSubOptions();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	virtual void OnResetData();
	virtual void OnApplyChanges();

private:
	CCvarCheckButton *m_pEnableCvar = nullptr;
	vgui2::Label *m_pColorLabel = nullptr;
	CCvarColor *m_pColorCvar = nullptr;
	vgui2::Label *m_pGapLabel = nullptr;
	CCvarTextEntry *m_pGapCvar = nullptr;
	vgui2::Label *m_pSizeLabel = nullptr;
	CCvarTextEntry *m_pSizeCvar = nullptr;
	vgui2::Label *m_pThicknessLabel = nullptr;
	CCvarTextEntry *m_pThicknessCvar = nullptr;
	vgui2::Label *m_pOutlineThicknessLabel = nullptr;
	CCvarTextEntry *m_pOutlineThicknessCvar = nullptr;
	CCvarCheckButton *m_pDotCvar = nullptr;
	CCvarCheckButton *m_pTCvar = nullptr;

	vgui2::ImagePanel *m_pPreviewPanel = nullptr;
	CCrosshairPreview *m_pPreviewImage = nullptr;

	cvar_t *m_pColors[3];

	MESSAGE_FUNC_PARAMS(OnColorPicked, "ColorPicked", kv);
	MESSAGE_FUNC_PARAMS(OnCvarTextChanged, "CvarTextChanged", kv);
	MESSAGE_FUNC_PARAMS(OnCheckButtonChecked, "CheckButtonChecked", kv);
};

#endif
