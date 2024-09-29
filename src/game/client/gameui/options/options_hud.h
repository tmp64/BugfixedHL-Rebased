#ifndef CHUDSUBOPTIONS_H
#define CHUDSUBOPTIONS_H
#include <vgui_controls/PropertyPage.h>
#include "gameui/options/cvar_combo_box.h"

namespace vgui2
{
class Label;
class Slider;
}

class CCvarTextEntry;
class CCvarCheckButton;

class CHudSubOptions : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CHudSubOptions, vgui2::PropertyPage);

public:
	CHudSubOptions(vgui2::Panel *parent);

	void OnResetData() override;
	void OnApplyChanges() override;

private:
	vgui2::Label *m_pOpacityLabel = nullptr;
	vgui2::Slider *m_pOpacitySlider = nullptr;
	CCvarTextEntry *m_pOpacityValue = nullptr;

	CCvarCheckButton *m_pRenderCheckbox = nullptr;
	CCvarCheckButton *m_pDimCheckbox = nullptr;
	CCvarCheckButton *m_pWeaponSpriteCheckbox = nullptr;
	CCvarCheckButton *m_pMenuFKeys = nullptr;
	CCvarCheckButton *m_pCenterIdCvar = nullptr;
	CCvarCheckButton *m_pRainbowCvar = nullptr;

	CCvarCheckButton *m_pSpeedCheckbox = nullptr;
	CCvarCheckButton *m_pSpeedCrossCheckbox = nullptr;

	CCvarCheckButton *m_pJumpSpeedCheckbox = nullptr;
	CCvarCheckButton *m_pJumpSpeedCrossCheckbox = nullptr;

	CCvarCheckButton *m_pDeathnoticeVGui = nullptr;

	CCVarComboBox *m_pTimerBox = nullptr;
	CCVarComboBox *m_pScaleBox = nullptr;

	MESSAGE_FUNC_PARAMS(OnSliderMoved, "SliderMoved", kv);
	MESSAGE_FUNC_PARAMS(OnCvarTextChanged, "CvarTextChanged", kv);
};

#endif
