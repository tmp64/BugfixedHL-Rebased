#include <vgui/ILocalize.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RichText.h>
#include "FileSystem.h"
#include "strtools.h"

#include "IGameUIFuncs.h"
#include "client_vgui.h"
#include "client_steam_context.h"

#include "client_motd.h"

using namespace vgui2;

class CClientMOTDHTML : public vgui2::HTML
{
public:
	using vgui2::HTML::HTML;
};

CClientMOTD::CClientMOTD()
    : BaseClass(nullptr, VIEWPORT_PANEL_MOTD)
    , m_bFileWritten(false)
    , m_iScoreBoardKey(0)
{
	//Sanity check.
	Assert(std::size(m_szTempFileName) > strlen("motd_temp.html"));
	V_strcpy_safe(m_szTempFileName, "motd_temp.html");

	SetTitle("", true);
	SetScheme("ClientScheme");
	SetMoveable(false);
	SetProportional(true);
	SetSizeable(false);

	m_pMessage = new vgui2::RichText(this, "TextMessage");

	if (SteamAPI_IsAvailable())
		m_pMessageHtml = new CClientMOTDHTML(this, "Message");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/MOTD.res");
	InvalidateLayout();

	m_pServerName = new vgui2::Label(this, "serverName", "");

	SetVisible(false);
}

CClientMOTD::~CClientMOTD()
{
	RemoveTempFile();
}

void CClientMOTD::SetLabelText(const char *textEntryName, const wchar_t *text)
{
	vgui2::Panel *pChild = FindChildByName(textEntryName);

	if (pChild)
	{
		auto pLabel = dynamic_cast<vgui2::Label *>(pChild);

		if (pLabel)
			pLabel->SetText(text);
	}
}

bool CClientMOTD::IsURL(const char *str)
{
	//TODO: https support
	return strncmp(str, "http://", 7) == 0;
}

void CClientMOTD::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CClientMOTD::OnKeyCodeTyped(vgui2::KeyCode key)
{
	if (key == KEY_PAD_ENTER || key == KEY_ENTER || key == KEY_SPACE)
	{
		OnCommand("okay");
	}
	else
	{
		if (m_iScoreBoardKey != KEY_NONE && m_iScoreBoardKey == key)
		{
			//TODO
			//if( !gViewPort->IsScoreBoardVisible() )
			{
				//g_pViewport->ShowBackGround( false );
				//g_pViewport->ShowScoreBoard();
				//SetVisible( false );
			}
		}
		else
		{
			BaseClass::OnKeyCodeTyped(key);
		}
	}
}

void CClientMOTD::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "okay"))
	{
		RemoveTempFile();
		ShowPanel(false);
	}

	BaseClass::OnCommand(command);
}

void CClientMOTD::Close()
{
	BaseClass::Close();
	// FIXME: m_pViewport->ShowBackGround( false );
}

void CClientMOTD::Activate(const char *title, const char *msg)
{
	m_pMessage->SetVisible(true);

	if (m_pMessageHtml)
		m_pMessageHtml->SetVisible(false);

	BaseClass::Activate();

	SetTitle(title, false);
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	//SetControlString( "serverName", title );

	// Replace CRLF with LF and convert to wide string
	static char buf[8192];
	static wchar_t wbuf[4096];
	V_StrSubst(msg, "\r\n", "\n", buf, sizeof(buf));
	Q_UTF8ToWString(buf, wbuf, sizeof(wbuf), STRINGCONVERT_REPLACE);
	m_pMessage->SetText(wbuf);
}

void CClientMOTD::ActivateHtml(const char *title, const char *msg)
{
	AssertMsg(m_pMessageHtml, "CClientMOTD::ActivateHtml on unsupported client");
	if (!m_pMessageHtml)
		return;

	char localURL[MAX_HTML_FILENAME_LENGTH + 7];

	m_pMessage->SetVisible(false);
	m_pMessageHtml->SetVisible(true);
	BaseClass::Activate();

	SetTitle(title, false);
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	//SetControlString( "serverName", title );

	const char *pszURL = msg;

	if (!IsURL(msg))
	{
		pszURL = nullptr;

		RemoveTempFile();

		if (!strstr(msg, "img src=\"view-source:") && !strstr(msg, "<style>;@/*"))
		{
			FileHandle_t hFile = g_pFullFileSystem->Open(m_szTempFileName, "w+", "GAMECONFIG");

			if (hFile != FILESYSTEM_INVALID_HANDLE)
			{
				g_pFullFileSystem->Write(msg, strlen(msg), hFile);
				g_pFullFileSystem->Close(hFile);

				V_strcpy_safe(localURL, "file:///");

				const size_t uiURLLength = strlen(localURL);
				g_pFullFileSystem->GetLocalPath(m_szTempFileName, localURL + uiURLLength, sizeof(localURL) - uiURLLength);

				pszURL = localURL;
			}
		}
	}

	if (pszURL)
		m_pMessageHtml->OpenURL(pszURL, nullptr);

	if (m_iScoreBoardKey == KEY_NONE)
		m_iScoreBoardKey = g_pGameUIFuncs->GetVGUI2KeyCodeForBind("showscores");
}

void CClientMOTD::Reset()
{
	if (m_pMessageHtml)
		m_pMessageHtml->OpenURL("", nullptr);
	m_pMessage->SetText("");

	RemoveTempFile();

	m_pServerName->SetText("");
}

void CClientMOTD::ShowPanel(bool state)
{
	if (BaseClass::IsVisible() == state)
		return;

	// FIXME: m_pViewport->ShowBackGround( state );

	if (state)
	{
		Reset();

		BaseClass::Activate();
	}
	else
	{
		BaseClass::SetVisible(false);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
	}
}

void CClientMOTD::RemoveTempFile()
{
	if (g_pFullFileSystem->FileExists(m_szTempFileName))
	{
		g_pFullFileSystem->RemoveFile(m_szTempFileName, "GAMECONFIG");
	}
}
