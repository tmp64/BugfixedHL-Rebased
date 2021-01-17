#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "update_dialogs.h"
#include "gameui/gameui_viewport.h"
#include "client_vgui.h"
#include "update_checker.h"
#include "update_installer.h"

extern ConVar cl_check_for_updates;

static const char *VerToString(const CGameVersion &ver)
{
	static std::string str;
	int major, minor, patch;
	ver.GetVersion(major, minor, patch);

	str = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);

	char tag[32];
	if (ver.GetTag(tag, sizeof(tag)))
		str += std::string("-") + tag;

	return str.c_str();
}

//-------------------------------------------------------------
// CUpdateNotificationDialog
//-------------------------------------------------------------
CUpdateNotificationDialog::CUpdateNotificationDialog()
    : BaseClass(CGameUIViewport::Get(), "UpdateNotificationDialog")
{
	SetDeleteSelfOnClose(true);
	SetMinimumSize(350, 290);

	m_pOldVersionLabel = new vgui2::Label(this, "OldVersionLabel", "0.0.0");
	m_pOldVersionTextLabel = new vgui2::Label(this, "OldVersionTextLabel", "#BHL_Update_Notif_OldVer");
	m_pNewVersionLabel = new vgui2::Label(this, "NewVersionLabel", "0.0.0");
	m_pNewVersionTextLabel = new vgui2::Label(this, "NewVersionTextLabel", "#BHL_Update_Notif_NewVer");
	m_pChangelog = new vgui2::RichText(this, "Changelog");
	m_pUpdatesCheckBtn = new vgui2::CheckButton(this, "UpdatesCheckBtn", "#BHL_Update_Notif_AutoCheck");
	m_pCloseBtn = new vgui2::Button(this, "CloseBtn", "#Close", this, "Close");
	m_pUpdateBtn = new vgui2::Button(this, "UpdateBtn", "#BHL_Update_Notif_Install", this, "Update");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/updater/UpdateNotificationDialog.res");
	MoveToCenterOfScreen();
}

void CUpdateNotificationDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	constexpr int BOTTOM_OFFSET = 8;
	constexpr int MIDDLE_OFFSET = 8;
	constexpr int BTN_SEPARATOR = 4;
	int btnTall = m_pCloseBtn->GetTall();
	int checkTall = m_pUpdatesCheckBtn->GetTall();

	// Position version labels
	{
		int x, y, maxPosX = 0;
		m_pOldVersionTextLabel->GetPos(x, y);
		maxPosX = max(maxPosX, x + m_pOldVersionTextLabel->GetWide());
		m_pNewVersionTextLabel->GetPos(x, y);
		maxPosX = max(maxPosX, x + m_pNewVersionTextLabel->GetWide());

		m_pOldVersionLabel->SetPos(maxPosX, m_pOldVersionTextLabel->GetYPos());
		m_pNewVersionLabel->SetPos(maxPosX, m_pNewVersionTextLabel->GetYPos());
	}

	// Resize change log to fill almost whole frame
	m_pChangelog->SetSize(GetWide() - m_pChangelog->GetXPos() * 2, GetTall() - m_pChangelog->GetYPos() - BOTTOM_OFFSET - btnTall - checkTall - 2 * MIDDLE_OFFSET);

	// Position buttons in a row after the changelog
	{
		int x, y;
		m_pChangelog->GetPos(x, y);
		y += m_pChangelog->GetTall() + MIDDLE_OFFSET;

		m_pUpdatesCheckBtn->SizeToContents();
		m_pUpdatesCheckBtn->SetPos(x, y);
		y += m_pUpdatesCheckBtn->GetTall() + MIDDLE_OFFSET;

		m_pUpdateBtn->SizeToContents();
		m_pUpdateBtn->SetPos(x, y);
		x += m_pUpdateBtn->GetWide() + BTN_SEPARATOR;

		m_pCloseBtn->SizeToContents();
		m_pCloseBtn->SetPos(x, y);
		x += m_pCloseBtn->GetWide() + BTN_SEPARATOR;
	}
}

void CUpdateNotificationDialog::OnCommand(const char *pszCmd)
{
	if (!strcmp(pszCmd, "Update"))
	{
		CUpdateInstaller::Get().StartUpdate();
		Close();
	}
	else
	{
		BaseClass::OnCommand(pszCmd);
	}
}

void CUpdateNotificationDialog::Activate()
{
	m_pOldVersionLabel->SetText(VerToString(CUpdateChecker::Get().GetCurVersion()));
	m_pNewVersionLabel->SetText(VerToString(CUpdateChecker::Get().GetLatestVersion()));

	const std::string &changelog = CUpdateChecker::Get().GetChangelog();
	std::vector<wchar_t> wbuf(changelog.size() + 1);
	Q_UTF8ToWString(changelog.c_str(), wbuf.data(), wbuf.size() * sizeof(wchar_t), STRINGCONVERT_REPLACE);
	m_pChangelog->SetText(wbuf.data());

	m_pUpdatesCheckBtn->SetSelected(cl_check_for_updates.GetBool());

	BaseClass::Activate();
}

void CUpdateNotificationDialog::OnClose()
{
	BaseClass::OnClose();
	cl_check_for_updates.SetValue(m_pUpdatesCheckBtn->IsSelected());
}

//-------------------------------------------------------------
// CUpdateDownloadStatusDialog
//-------------------------------------------------------------
CUpdateDownloadStatusDialog::CUpdateDownloadStatusDialog()
    : BaseClass(CGameUIViewport::Get(), "UpdateDownloadStatusDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetCloseButtonVisible(false);

	LoadControlSettings(VGUI2_ROOT_DIR "resource/updater/UpdateDownloadStatusDialog.res");
	MoveToCenterOfScreen();

	m_pProgress = FindControl<vgui2::ProgressBar>("Progress");
	m_pProgressLabel = FindControl<vgui2::Label>("ProgressLabel");
}

void CUpdateDownloadStatusDialog::OnThink()
{
	if (m_pStatus.get())
	{
		size_t dl = m_pStatus->iSize;
		size_t total = m_pStatus->iTotalSize;

		if (total == 0)
		{
			m_pProgress->SetProgress(0);
			m_pProgressLabel->SetText((FormatMemSize(dl) + "...").c_str());
		}
		else
		{
			char buf[128];
			double precent = (double)dl / total;
			snprintf(buf, sizeof(buf), "%.f%% - %s / %s", precent * 100,
			    FormatMemSize(dl).c_str(), FormatMemSize(total).c_str());
			m_pProgress->SetProgress((float)precent);
			m_pProgressLabel->SetText(buf);
		}
	}
}

void CUpdateDownloadStatusDialog::OnCommand(const char *pszCmd)
{
	if (!strcmp(pszCmd, "CancelDownload"))
	{
		CUpdateInstaller::Get().CancelInstallation();
	}
	else
	{
		BaseClass::OnCommand(pszCmd);
	}
}

void CUpdateDownloadStatusDialog::SetStatus(std::shared_ptr<CHttpClient::DownloadStatus> &pStatus)
{
	m_pStatus = pStatus;
}

std::string CUpdateDownloadStatusDialog::FormatMemSize(size_t iTotalMem)
{
	char buf[128];

	if (iTotalMem < 1024)
	{
		snprintf(buf, sizeof(buf), "%1 B", iTotalMem);
	}
	else if (iTotalMem < 1024L * 1024L)
	{
		double fMem = iTotalMem / 1024.0;
		snprintf(buf, sizeof(buf), "%.2f KiB", fMem);
	}
	else if (iTotalMem < 1024L * 1024L * 1024L)
	{
		double fMem = iTotalMem / 1024.0 / 1024.0;
		snprintf(buf, sizeof(buf), "%.2f MiB", fMem);
	}
	else /* if (iTotalMem < 1024L * 1024L * 1024L * 1024L)*/
	{
		double fMem = iTotalMem / 1024.0 / 1024.0 / 1024.0;
		snprintf(buf, sizeof(buf), "%.2f GiB", fMem);
	}

	return buf;
}

//-------------------------------------------------------------
// CUpdateFileProgressDialog
//-------------------------------------------------------------
CUpdateFileProgressDialog::CUpdateFileProgressDialog()
    : BaseClass(CGameUIViewport::Get(), "UpdateFileProgressDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetCloseButtonVisible(false);

	LoadControlSettings(VGUI2_ROOT_DIR "resource/updater/UpdateFileProgressDialog.res");
	MoveToCenterOfScreen();

	m_pProgress = FindControl<vgui2::ProgressBar>("Progress");
	m_pProgressLabel = FindControl<vgui2::Label>("ProgressLabel");
	m_pFileNameLabel = FindControl<vgui2::Label>("FileNameLabel");
	m_pCancelButton = FindControl<vgui2::Button>("CancelButton");
}

void CUpdateFileProgressDialog::OnCommand(const char *pszCmd)
{
	if (!strcmp(pszCmd, "Cancel"))
	{
		CUpdateInstaller::Get().CancelInstallation();
	}
	else
	{
		BaseClass::OnCommand(pszCmd);
	}
}

void CUpdateFileProgressDialog::UpdateProgress(const UpdateFileProgress &pr)
{
	double progress;

	if (pr.iTotalFiles == 0)
	{
		progress = 0;
	}
	else
	{
		progress = (double)pr.iFinishedFiles / pr.iTotalFiles;
	}

	m_pProgress->SetProgress((float)progress);

	char buf[128];
	snprintf(buf, sizeof(buf), "%.f%% - %d / %d", progress * 100, pr.iFinishedFiles, pr.iTotalFiles);
	m_pProgressLabel->SetText(buf);

	size_t offset = pr.filename.find('/');
	if (offset == pr.filename.npos)
		offset = 0;
	else
		offset += 1;
	m_pFileNameLabel->SetText(pr.filename.c_str() + offset);
}

void CUpdateFileProgressDialog::SetCancelButtonVisible(bool state)
{
	m_pCancelButton->SetVisible(state);
}

//-------------------------------------------------------------
// CUpdateFileReplaceDialog
//-------------------------------------------------------------
CUpdateFileReplaceDialog::CUpdateFileReplaceDialog()
    : BaseClass(CGameUIViewport::Get(), "UpdateFileReplaceDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable(false);
	SetCloseButtonVisible(false);
	SetTitle("#BHL_Update_FileConflict", true);

	m_pFileNameLabel = new vgui2::Label(this, "FileNameLabel", "File name");
	LoadControlSettings(VGUI2_ROOT_DIR "resource/updater/UpdateFileReplaceDialog.res");
	MoveToCenterOfScreen();
}

void CUpdateFileReplaceDialog::OnCommand(const char *pszCmd)
{
	if (!strcmp(pszCmd, "Replace"))
	{
		CUpdateInstaller::Get().ShowNextReplaceDialog(true, false);
	}
	else if (!strcmp(pszCmd, "ReplaceAll"))
	{
		CUpdateInstaller::Get().ShowNextReplaceDialog(true, true);
	}
	else if (!strcmp(pszCmd, "Keep"))
	{
		CUpdateInstaller::Get().ShowNextReplaceDialog(false, false);
	}
	else if (!strcmp(pszCmd, "KeepAll"))
	{
		CUpdateInstaller::Get().ShowNextReplaceDialog(false, true);
	}
	else if (!strcmp(pszCmd, "Cancel"))
	{
		CUpdateInstaller::Get().CancelInstallation();
	}
	else
	{
		BaseClass::OnCommand(pszCmd);
	}
}

void CUpdateFileReplaceDialog::Activate(const std::string &name)
{
	BaseClass::Activate();
	m_pFileNameLabel->SetText(name.c_str());
}
