#ifndef CGENERALSUBOPTIONS_H
#define CGENERALSUBOPTIONS_H
#include <vgui_controls/PropertyPage.h>

namespace vgui2
{
	class Label;
	class Slider;
}

class CCvarTextEntry;
class CCvarColor;
class CCvarCheckButton;

class CGeneralSubOptions : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CGeneralSubOptions, vgui2::PropertyPage);

public:
	CGeneralSubOptions(vgui2::Panel *parent);

	virtual void PerformLayout();
	virtual void OnResetData();
	virtual void OnApplyChanges();

private:
	vgui2::Label *m_pFovLabel = nullptr;
	vgui2::Slider *m_pFovSlider = nullptr;
	CCvarTextEntry *m_pFovValue = nullptr;

	vgui2::CheckButton *m_pRawInput = nullptr;
	vgui2::Label *m_pRawInputLabel = nullptr;
	CCvarCheckButton *m_pKillSnd = nullptr;
	vgui2::Label *m_pKillSndLabel = nullptr;
	CCvarCheckButton *m_pMOTD = nullptr;
	vgui2::Label *m_pMOTDLabel = nullptr;

	CCvarCheckButton *m_pAutoDemo = nullptr;
	vgui2::Label *m_pAutoDemoLabel = nullptr;
	CCvarTextEntry *m_pKeepFor = nullptr;
	vgui2::Label *m_pKeepForLabel = nullptr;
	vgui2::Label *m_pDaysLabel = nullptr;

	MESSAGE_FUNC_PARAMS(OnSliderMoved, "SliderMoved", kv);
	MESSAGE_FUNC_PARAMS(OnCvarTextChanged, "CvarTextChanged", kv);

	void SetRawInputVal(bool state);
	bool GetRawInputVal();
	const char *GetRawInputText();
};

#endif
