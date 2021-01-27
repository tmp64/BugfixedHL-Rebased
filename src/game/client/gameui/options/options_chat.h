#ifndef CCHATSUBOPTIONS_H
#define CCHATSUBOPTIONS_H
#include <vgui_controls/PropertyPage.h>

namespace vgui2
{
class Label;
class ComboBox;
}

class CCvarCheckButton;
class CCvarTextEntry;

class CChatSubOptions : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CChatSubOptions, vgui2::PropertyPage);

public:
	CChatSubOptions(vgui2::Panel *parent);

	virtual void OnResetData();
	virtual void OnApplyChanges();

private:
	vgui2::Label *m_pChatStyleLabel = nullptr;
	vgui2::ComboBox *m_pChatStyleBox = nullptr;
	int m_ChatStyleItems[3 + 1];

	vgui2::Label *m_pVgui2Label = nullptr;

	CCvarTextEntry *m_pTime = nullptr;
	vgui2::Label *m_pTimeLabel = nullptr;

	CCvarCheckButton *m_pMuteAllComms = nullptr;
	vgui2::Label *m_pMuteAllCommsLabel = nullptr;
};

#endif
