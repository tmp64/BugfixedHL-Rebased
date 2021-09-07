#include <string>
#include <vector>
#include <FileSystem.h>
#include <tier1/strtools.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/URLLabel.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/RichText.h>
#include <KeyValues.h>
#include <appversion.h>
#include <bhl_urls.h>
#include "client_vgui.h"
#include "options_about.h"
#include "cvar_check_button.h"

#if USE_UPDATER
#include "updater/update_checker.h"
#endif

class COptionsOSSCreditsDialog : public vgui2::Frame
{
public:
	DECLARE_CLASS_SIMPLE(COptionsOSSCreditsDialog, vgui2::Frame);

	COptionsOSSCreditsDialog(vgui2::Panel *pParent, const char *name)
	    : BaseClass(pParent, name)
	{
		SetTitle("#BHL_AdvOptions_About_OSSCredits", true);
		SetSize(640, 480);
		m_pText = new vgui2::RichText(this, "Text");

		// Fill m_pText with file contents
		FileHandle_t fh = g_pFullFileSystem->Open(VGUI2_ROOT_DIR "resource/open_source_software.txt", "r");

		if (fh == FILESYSTEM_INVALID_HANDLE)
		{
			Error("Failed to open open_source_software.txt");
			m_pText->SetText("Failed to open open_source_software.txt");
			return;
		}

		int size = g_pFullFileSystem->Size(fh);
		std::vector<char> buf(size + 1);
		size = g_pFullFileSystem->Read(buf.data(), size, fh);
		buf[size] = '\0';

		std::vector<wchar_t> wbuf(size + 1);
		Q_UTF8ToWString(buf.data(), wbuf.data(), wbuf.size() * sizeof(wchar_t));
		m_pText->SetText(wbuf.data());
	}

	virtual void PerformLayout() override
	{
		BaseClass::PerformLayout();

		int x, y, wide, tall;
		GetClientArea(x, y, wide, tall);
		m_pText->SetBounds(x, y, wide, tall);
	}

private:
	vgui2::RichText *m_pText = nullptr;
};

class COptionsURLButton : public vgui2::URLLabel
{
public:
	DECLARE_CLASS_SIMPLE(COptionsURLButton, vgui2::URLLabel);

	COptionsURLButton(vgui2::Panel *pParent, const char *name, const char *text)
	    : BaseClass(pParent, name, text, "")
	{
	}

	virtual void OnMousePressed(vgui2::MouseCode code) override
	{
		if (code == vgui2::MOUSE_LEFT)
		{
			if (!m_pDialog)
				m_pDialog = new COptionsOSSCreditsDialog(this, "OSSCreditsDialog");

			m_pDialog->MoveToCenterOfScreen();
			m_pDialog->Activate();
		}
	}

private:
	COptionsOSSCreditsDialog *m_pDialog = nullptr;
};

CAboutSubOptions::CAboutSubOptions(vgui2::Panel *parent)
    : BaseClass(parent, "AboutSubOptions")
{
	SetSize(100, 100); // Silence "parent not sized yet" warning

	m_pBHLLabel = new vgui2::Label(this, "BHLLabel", "#BHL_AdvOptions_About_BHL");
	m_pVerTextLabel = new vgui2::Label(this, "VerTextLabel", "#BHL_AdvOptions_About_Version");
	m_pVerLabel = new vgui2::Label(this, "VerLabel", "?");

	m_pLatestVerTextLabel = new vgui2::Label(this, "LatestVerTextLabel", "#BHL_AdvOptions_About_NewVersion");
	m_pLatestVerLabel = new vgui2::Label(this, "LatestVerLabel", "#BHL_AdvOptions_About_NoUpdater");
	m_pUpdateLabel = new vgui2::Label(this, "UpdateLable", "#BHL_AdvOptions_About_NewUpdate");
	m_pCheckUpdatesButton = new vgui2::Button(this, "CheckUpdatesButton", "#BHL_AdvOptions_About_Check", this, "CheckUpd");

	m_pGitHubLink = new vgui2::URLLabel(this, "GitHubLink", "#BHL_AdvOptions_About_GitHub", "URL goes here");
	m_pAghlLink = new vgui2::URLLabel(this, "AghlLink", "#BHL_AdvOptions_About_AGHL", "URL goes here");
	m_pOSSCredits = new COptionsURLButton(this, "OSSCredits", "#BHL_AdvOptions_About_OSSCredits");

#if USE_UPDATER
	m_pAutoCheck = new CCvarCheckButton(this, "AutoCheck", "#BHL_AdvOptions_About_AutoCheck", "cl_check_for_updates");
	m_GameVer = CUpdateChecker::Get().GetCurVersion();
#else
	m_GameVer = CGameVersion(APP_VERSION);
#endif

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/AboutSubOptions.res");
}

CAboutSubOptions::~CAboutSubOptions()
{
}

void CAboutSubOptions::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_pUpdateLabel->SetFgColor(m_GreenColor);
}

void CAboutSubOptions::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pGitHubLink->SetURL(BHL_GITHUB_URL);
	m_pAghlLink->SetURL(BHL_FORUM_URL);

	auto fnPositionAfter = [](vgui2::Label *left, vgui2::Label *right) {
		int x, y, wide, tall;
		left->GetPos(x, y);
		left->GetContentSize(wide, tall);
		left->SetWide(wide);
		right->SetPos(x + wide, y);
	};

	fnPositionAfter(m_pVerTextLabel, m_pVerLabel);
	fnPositionAfter(m_pLatestVerTextLabel, m_pLatestVerLabel);

	UpdateControls();
}

void CAboutSubOptions::OnCommand(const char *pCmd)
{
	if (!strcmp(pCmd, "CheckUpd"))
	{
#if USE_UPDATER
		CUpdateChecker::Get().CheckForUpdates();
#endif
	}
	else
		BaseClass::OnCommand(pCmd);
}

void CAboutSubOptions::OnResetData()
{
#if USE_UPDATER
	m_pAutoCheck->ResetData();
#endif
}

void CAboutSubOptions::OnApplyChanges()
{
#if USE_UPDATER
	m_pAutoCheck->ApplyChanges();
#endif
}

void CAboutSubOptions::UpdateControls()
{
	char buf[128];
	wchar_t wbuf[128];

	// Update client's version
	{
		snprintf(buf, sizeof(buf), "%d.%d.%d", m_GameVer.GetMajor(), m_GameVer.GetMinor(), m_GameVer.GetPatch());
		std::string gameVer = buf;
		if (m_GameVer.GetTag(buf, sizeof(buf)))
		{
			gameVer += "-";
			gameVer += buf;
		}

		buf[0] = '\0';
		gameVer += " (";

		m_GameVer.GetBranch(buf, sizeof(buf));
		gameVer += buf;
		gameVer += ".";

		m_GameVer.GetCommitHash(buf, sizeof(buf));
		gameVer += buf;

		if (m_GameVer.IsDirtyBuild())
			gameVer += ".m";

		gameVer += ")";

#ifdef _DEBUG
		gameVer += " [Debug build]";
#endif

		g_pVGuiLocalize->ConvertANSIToUnicode(gameVer.c_str(), wbuf, sizeof(wbuf));
		m_pVerLabel->SetText(wbuf);
	}

#if !USE_UPDATER
	m_pLatestVerLabel->SetText("#BHL_AdvOptions_About_NoUpdater");
	m_pUpdateLabel->SetVisible(false);
	m_pCheckUpdatesButton->SetEnabled(false);
#else
	m_pCheckUpdatesButton->SetEnabled(true);

	CGameVersion latestVer = CUpdateChecker::Get().GetLatestVersion();
	if (latestVer.IsValid())
	{
		snprintf(buf, sizeof(buf), "%d.%d.%d", latestVer.GetMajor(), latestVer.GetMinor(), latestVer.GetPatch());
		std::string strVer = buf;
		if (latestVer.GetTag(buf, sizeof(buf)))
		{
			strVer += "-";
			strVer += buf;
		}

		g_pVGuiLocalize->ConvertANSIToUnicode(strVer.c_str(), wbuf, sizeof(wbuf));
		m_pLatestVerLabel->SetText(wbuf);

		if (latestVer > m_GameVer)
		{
			m_pUpdateLabel->SetVisible(true);
		}
		else
		{
			m_pUpdateLabel->SetVisible(false);
		}
	}
	else
	{
		m_pLatestVerLabel->SetText("#BHL_AdvOptions_About_Unknown");
		m_pUpdateLabel->SetVisible(false);
	}
#endif
}
