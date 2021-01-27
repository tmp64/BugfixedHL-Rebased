#include <string>
#include <vgui/ILocalize.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/URLLabel.h>
#include <KeyValues.h>
#include <appversion.h>
#include <bhl_urls.h>
#include "client_vgui.h"
#include "options_about.h"
#include "cvar_check_button.h"

#if USE_UPDATER
#include "updater/update_checker.h"
#endif

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
	m_pUpdate2Label = new vgui2::Label(this, "Update2Lable", "#BHL_AdvOptions_About_NewUpdate2");
	m_pCheckUpdatesButton = new vgui2::Button(this, "CheckUpdatesButton", "#BHL_AdvOptions_About_Check", this, "CheckUpd");
	m_pChangelogButton = new vgui2::Button(this, "ChangelogButton", "#BHL_AdvOptions_About_Changelog", this, "Changelog");

	m_pGitHubLink = new vgui2::URLLabel(this, "GitHubLink", "#BHL_AdvOptions_About_GitHub", "URL goes here");
	m_pAghlLink = new vgui2::URLLabel(this, "AghlLink", "#BHL_AdvOptions_About_AGHL", "URL goes here");

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
	m_pUpdate2Label->SetFgColor(m_GreenColor);
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
	m_pUpdate2Label->SetVisible(false);
	m_pCheckUpdatesButton->SetEnabled(false);
	m_pChangelogButton->SetEnabled(false);
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
			m_pUpdate2Label->SetVisible(true);
			m_pChangelogButton->SetEnabled(true);
		}
		else
		{
			m_pUpdateLabel->SetVisible(false);
			m_pUpdate2Label->SetVisible(false);
			m_pChangelogButton->SetEnabled(false);
		}
	}
	else
	{
		m_pLatestVerLabel->SetText("#BHL_AdvOptions_About_Unknown");
		m_pUpdateLabel->SetVisible(false);
		m_pUpdate2Label->SetVisible(false);
		m_pChangelogButton->SetEnabled(false);
	}
#endif
}
