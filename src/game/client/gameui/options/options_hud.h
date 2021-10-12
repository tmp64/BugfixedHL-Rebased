#ifndef CHUDSUBOPTIONS_H
#define CHUDSUBOPTIONS_H
#include <vgui_controls/PropertyPage.h>

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

	void PerformLayout() override;
	void OnResetData() override;
	void OnApplyChanges() override;

private:
	vgui2::Label *m_pOpacityLabel = nullptr;
	vgui2::Slider *m_pOpacitySlider = nullptr;
	CCvarTextEntry *m_pOpacityValue = nullptr;

	CCvarCheckButton *m_pRenderCheckbox = nullptr;
	CCvarCheckButton *m_pDimCheckbox = nullptr;
	CCvarCheckButton *m_pWeaponSpriteCheckbox = nullptr;
	CCvarCheckButton *m_pViewmodelCheckbox = nullptr;
	CCvarCheckButton *m_pCenterIdCvar = nullptr;
	CCvarCheckButton *m_pRainbowCvar = nullptr;

	CCvarCheckButton *m_pSpeedCheckbox = nullptr;
	CCvarCheckButton *m_pSpeedCrossCheckbox = nullptr;

	CCvarCheckButton *m_pJumpSpeedCheckbox = nullptr;
	CCvarCheckButton *m_pJumpSpeedCrossCheckbox = nullptr;

	vgui2::Label *m_pTimerLabel = nullptr;
	vgui2::ComboBox *m_pTimerBox = nullptr;
	int m_TimerItems[4];

	void TimerResetData();
	void TimerApplyChanges();

	MESSAGE_FUNC_PARAMS(OnSliderMoved, "SliderMoved", kv);
	MESSAGE_FUNC_PARAMS(OnCvarTextChanged, "CvarTextChanged", kv);
};

#endif
