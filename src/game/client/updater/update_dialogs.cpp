#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "update_dialogs.h"
#include "gameui/gameui_viewport.h"
#include "client_vgui.h"
#include "update_checker.h"

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
		// TODO: Call updater
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
