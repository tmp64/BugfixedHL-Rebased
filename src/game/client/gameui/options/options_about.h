#ifndef CABOUTSUBOPTIONS_H
#define CABOUTSUBOPTIONS_H
#include <vgui_controls/PropertyPage.h>
#include "CGameVersion.h"

namespace vgui2
{
class Label;
class Button;
class URLLabel;
}

class CCvarCheckButton;
class COptionsURLButton;

class CAboutSubOptions : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CAboutSubOptions, vgui2::PropertyPage);

public:
	CAboutSubOptions(vgui2::Panel *parent);
	~CAboutSubOptions();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void OnCommand(const char *pCmd);

	virtual void OnResetData();
	virtual void OnApplyChanges();

private:
	vgui2::Label *m_pBHLLabel = nullptr;
	vgui2::Label *m_pVerTextLabel = nullptr;
	vgui2::Label *m_pVerLabel = nullptr;

	vgui2::Label *m_pLatestVerTextLabel = nullptr;
	vgui2::Label *m_pLatestVerLabel = nullptr;
	vgui2::Label *m_pUpdateLabel = nullptr;
	vgui2::Button *m_pCheckUpdatesButton = nullptr;

	CCvarCheckButton *m_pAutoCheck = nullptr;
	vgui2::URLLabel *m_pGitHubLink = nullptr;
	vgui2::URLLabel *m_pAghlLink = nullptr;
	COptionsURLButton *m_pOSSCredits = nullptr;

	Color m_GreenColor = Color(0, 255, 0, 255);
	CGameVersion m_GameVer;
	int m_iCallbackId = -1;

	void UpdateControls();
};

#endif
