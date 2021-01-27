#ifndef CSCOREBOARDSUBOPTIONS_H
#define CSCOREBOARDSUBOPTIONS_H
#include <vgui_controls/PropertyPage.h>

namespace vgui2
{
class Label;
class ComboBox;
}

class CCvarCheckButton;
class CCvarTextEntry;

class CScoreboardSubOptions : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CScoreboardSubOptions, vgui2::PropertyPage);

public:
	CScoreboardSubOptions(vgui2::Panel *parent);

	virtual void OnResetData();
	virtual void OnApplyChanges();

private:
	CCvarCheckButton *m_pShowAvatars = nullptr;
	CCvarCheckButton *m_pShowSteamId = nullptr;
	CCvarCheckButton *m_pShowPacketLoss = nullptr;

	CCvarCheckButton *m_pShowEff = nullptr;
	vgui2::Label *m_pEffTypeLabel = nullptr;
	vgui2::ComboBox *m_pEffTypeBox = nullptr;
	int m_EffTypeItems[2];

	vgui2::Label *m_pMouseLabel = nullptr;
	vgui2::ComboBox *m_pMouseBox = nullptr;
	int m_MouseItems[3];

	vgui2::Label *m_pSizeLabel = nullptr;
	vgui2::ComboBox *m_pSizeBox = nullptr;
	int m_SizeItems[3];

	void ApplyEffType();
	void ApplyMouse();
	void ApplySize();
};

#endif
