#include <vgui/ILocalize.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/RichText.h>
#include "client_vgui.h"
#include "client_viewport.h"
#include "team_menu.h"
#include "viewport_panel_names.h"
#include "hud.h"
#include "cl_util.h"

CTeamMenu::CTeamMenu()
    : BaseClass(nullptr, VIEWPORT_PANEL_TEAM_MENU)
{
	char buf[64];

	SetTitle("#BHL_TeamMenu_Title", true);
	SetMoveable(false);
	SetProportional(true);
	SetSizeable(false);
	SetCloseButtonVisible(false);

	for (int i = 1; i <= MAX_TEAMS_IN_MENU; i++)
	{
		snprintf(buf, sizeof(buf), "TeamButton_%d", i);
		m_pTeamButtons[i] = new vgui2::Button(this, buf, "No Team", this, buf);
	}

	m_pAutoAssignButton = new vgui2::Button(this, "AutoAssignButton", "#BHL_TeamMenu_Auto_Assign", this, "AutoAssign");
	m_pSpectateButton = new vgui2::Button(this, "SpectateButton", "#BHL_TeamMenu_Spectate", this, "Spectate");
	m_pCancelButton = new vgui2::Button(this, "CancelButton", "#BHL_TeamMenu_Cancel", this, "Close");
	m_pBriefingText = new vgui2::RichText(this, "BriefingText");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/TeamMenu.res");
	SetVisible(false);
}

CTeamMenu::~CTeamMenu()
{
}

void CTeamMenu::OnCommand(const char *pCommand)
{
	if (!V_stricmp(pCommand, "AutoAssign"))
	{
		gEngfuncs.pfnServerCmd("jointeam 5");
		ShowPanel(false);
	}
	else if (!V_stricmp(pCommand, "Spectate"))
	{
		gEngfuncs.pfnServerCmd("spectate");
		ShowPanel(false);
	}
	else if (!V_stricmp(pCommand, "Close"))
	{
		ShowPanel(false);
	}
	else if (!V_strnicmp(pCommand, "TeamButton_", 11))
	{
		char c = pCommand[11];
		Assert(c >= '0' && c <= '4');
		char str[] = "jointeam #";
		str[9] = c;
		gEngfuncs.pfnServerCmd(str);
		ShowPanel(false);
	}
}

void CTeamMenu::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code >= vgui2::KEY_1 && code <= vgui2::KEY_9)
	{
		int num = code - vgui2::KEY_0;
		if (num >= 1 && num <= MAX_TEAMS_IN_MENU)
		{
			char cmd[] = "TeamButton_#";
			cmd[11] = '0' + num;
			OnCommand(cmd);
		}
		else if (num == 5)
			m_pAutoAssignButton->FireActionSignal();
		else if (num == 6)
			m_pSpectateButton->FireActionSignal();
		else if (num == 7 && m_pCancelButton->IsVisible())
			m_pCancelButton->FireActionSignal();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

void CTeamMenu::Update()
{
	char buf[128];
	int xpos = m_iBtnX;
	int ypos = m_iBtnY;

	// Set the team buttons
	for (int i = 1; i <= MAX_TEAMS_IN_MENU; i++)
	{
		if (i <= g_pViewport->GetNumberOfTeams())
		{
			snprintf(buf, sizeof(buf), "  %d   %s", i, GetTeamInfo(i)->GetDisplayName());
			m_pTeamButtons[i]->SetText(buf);
			m_pTeamButtons[i]->SetVisible(true);
			m_pTeamButtons[i]->SetPos(xpos, ypos);
			ypos += m_pTeamButtons[i]->GetTall() + m_iBtnSpacing;
		}
		else
		{
			// Hide the button (may be visible from previous maps)
			m_pTeamButtons[i]->SetVisible(false);
		}
	}

	// AutoAssign button
	m_pAutoAssignButton->SetPos(xpos, ypos);
	ypos += m_pAutoAssignButton->GetTall() + m_iBtnSpacing;

	// Spectate button
	m_pSpectateButton->SetPos(xpos, ypos);
	ypos += m_pSpectateButton->GetTall() + m_iBtnSpacing;

	// If the player is already in a team, make the cancel button visible
	if (g_iTeamNumber)
	{
		m_pCancelButton->SetVisible(true);
		m_pCancelButton->SetPos(xpos, ypos);
		ypos += m_pCancelButton->GetTall() + m_iBtnSpacing;
	}
	else
	{
		m_pCancelButton->SetVisible(false);
	}

	// Set the Map Title
	if (!m_bUpdatedMapName)
	{
		const char *level = gEngfuncs.pfnGetLevelName();
		if (level && level[0])
		{
			char sz[256];
			char szTitle[256];
			char *ch;

			// Update the map briefing
			V_strcpy_safe(sz, level);
			ch = strchr(sz, '.');
			*ch = '\0';
			strcat(sz, ".txt");
			char *pfile = (char *)gEngfuncs.COM_LoadFile(sz, 5, NULL);
			if (pfile)
			{
				static char buf[4096];
				static wchar_t wbuf[4096];

				// Replace \r\n with \n
				V_StrSubst(pfile, "\r\n", "\n", buf, sizeof(buf));

				// Convert to Unicode to fix 1024 char limit
				g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));

				m_pBriefingText->SetText(wbuf);
				gEngfuncs.COM_FreeFile(pfile);
			}
			else
			{
				m_pBriefingText->SetText("#BHL_TeamMenu_NoMapInfo");
			}

			m_bUpdatedMapName = true;
		}
	}
}

void CTeamMenu::Activate()
{
	Update();
	ShowPanel(true);
}

const char *CTeamMenu::GetName()
{
	return VIEWPORT_PANEL_TEAM_MENU;
}

void CTeamMenu::Reset()
{
	m_bUpdatedMapName = false;
}

void CTeamMenu::ShowPanel(bool state)
{
	if (state != IsVisible())
	{
		SetVisible(state);
	}
}

vgui2::VPANEL CTeamMenu::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CTeamMenu::IsVisible()
{
	return BaseClass::IsVisible();
}

void CTeamMenu::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}
