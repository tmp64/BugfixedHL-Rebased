#include <KeyValues.h>
#include <vgui_controls/AnimationController.h>
#include <IEngineVGui.h>
#include <client_steam_context.h>

#include "client_viewport.h"
#include "client_vgui.h"

#include <demo_api.h>
#include <pm_shared.h>
#include <keydefs.h>
#include "parsemsg.h"
#include "hud.h"
#include "hud/text_message.h"
#include "hud/spectator.h"
#include "cl_util.h"

#include "score_panel.h"
#include "client_motd.h"
#include "spectator_panel.h"
#include "hud_health.h"
#include "hud_battery.h"
#include "hud_ammo.h"
#include "hud_ammo_secondary.h"
#include "team_menu.h"
#include "command_menu.h"

// FIXME: Move it to hud.cpp
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

extern ConVar hud_scoreboard_mousebtn;

CClientViewport *g_pViewport = nullptr;

CON_COMMAND(hud_reloadscheme, "Reloads hud layout and animation scripts.")
{
	g_pViewport->ReloadScheme(nullptr);
	g_pViewport->ReloadLayout();
}

CClientViewport::CClientViewport()
    : BaseClass(nullptr, "CClientViewport")
{
	Assert(!g_pViewport);
	g_pViewport = this;

	SetParent(g_pEngineVGui->GetPanel(PANEL_CLIENTDLL));

	SetSize(10, 10); // Quiet "parent not sized yet" spew
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetProportional(true);

	// create our animation controller
	m_pAnimController = new vgui2::AnimationController(this);
	m_pAnimController->SetProportional(true);

	// Load scheme
	ReloadScheme(VGUI2_ROOT_DIR "resource/ClientScheme.res");

	// Hide viewport
	HideClientUI();
}

void CClientViewport::Start()
{
	CreateDefaultPanels();
}

bool CClientViewport::LoadHudAnimations()
{
	const char *HUDANIMATION_MANIFEST_FILE = VGUI2_ROOT_DIR "scripts/hudanimations_manifest.txt";
	KeyValues *manifest = new KeyValues(HUDANIMATION_MANIFEST_FILE);
	if (!manifest->LoadFromFile(g_pFullFileSystem, HUDANIMATION_MANIFEST_FILE))
	{
		manifest->deleteThis();
		return false;
	}

	bool bClearScript = true;

	// Load each file defined in the text
	for (KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey())
	{
		if (!Q_stricmp(sub->GetName(), "file"))
		{
			// Add it
			if (m_pAnimController->SetScriptFile(GetVPanel(), sub->GetString(), bClearScript) == false)
			{
				Assert(0);
			}

			bClearScript = false;
			continue;
		}
	}

	manifest->deleteThis();
	return true;
}

void CClientViewport::ReloadScheme(const char *fromFile)
{
	// See if scheme should change
	if (fromFile != NULL)
	{
		// "ui/resource/ClientScheme.res"
		vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile(fromFile, "ClientScheme");

		SetScheme(scheme);
		SetProportional(true);
		m_pAnimController->SetScheme(scheme);
	}

	// Force a reload
	if (LoadHudAnimations() == false)
	{
		// Fall back to just the main
		if (m_pAnimController->SetScriptFile(GetVPanel(), VGUI2_ROOT_DIR "scripts/HudAnimations.txt", true) == false)
		{
			Assert(!("Failed to load ui/scripts/HudAnimations.txt"));
		}
	}

	SetProportional(true);

	InvalidateLayout(true, true);
}

void CClientViewport::ReloadLayout()
{
	// reload the .res file from disk
	LoadControlSettings(VGUI2_ROOT_DIR "scripts/HudLayout.res");

	InvalidateLayout(true, true);

	// Load custom positions for the vanilla HUD to avoid overlapping with the new HUD
	KeyValues *layoutKV = new KeyValues("HudLayout");
	if (layoutKV->LoadFromFile(g_pFullFileSystem, VGUI2_ROOT_DIR "scripts/HudLayoutBase.res"))
	{
		KeyValues *vanillaKV = layoutKV->FindKey("HudVanilla");
		if (vanillaKV)
		{
			const char* szStatusBarY = vanillaKV->GetString("statusbar_ypos", "0");
			const char* szAmmoHistoryY = vanillaKV->GetString("ammohistory_ypos", "0");

			ComputePos(szStatusBarY, m_iStatusBarYPos, 0, GetTall(), true);
			ComputePos(szAmmoHistoryY, m_iAmmoHistoryYPos, 0, GetTall(), true);
		}

	}

	m_pHudAmmoPanel->LoadControlSettings(VGUI2_ROOT_DIR "resource/HudAmmo.res");
	m_pHudAmmoPanel->InvalidateLayout(true, true);

	m_pHudHealthPanel->LoadControlSettings(VGUI2_ROOT_DIR "resource/HudHealth.res");
	m_pHudHealthPanel->InvalidateLayout(true, true);

	m_pHudBatteryPanel->LoadControlSettings(VGUI2_ROOT_DIR "resource/HudBattery.res");
	m_pHudBatteryPanel->InvalidateLayout(true, true);

	m_pHudAmmoSecondaryPanel->LoadControlSettings(VGUI2_ROOT_DIR "resource/HudAmmoSecondary.res");
	m_pHudAmmoSecondaryPanel->InvalidateLayout(true, true);
}

void CClientViewport::CreateDefaultPanels()
{
	AddNewPanel(m_pScorePanel = new CScorePanel());
	AddNewPanel(m_pMOTD = new CClientMOTD());
	AddNewPanel(m_pSpectatorPanel = new CSpectatorPanel());
	AddNewPanel(m_pHudHealthPanel = new CHudHealthPanel());
	AddNewPanel(m_pHudBatteryPanel = new CHudBatteryPanel());
	AddNewPanel(m_pHudAmmoPanel = new CHudAmmoPanel());
	AddNewPanel(m_pHudAmmoSecondaryPanel = new CHudAmmoSecondaryPanel());
	AddNewPanel(m_pTeamMenu = new CTeamMenu());
	AddNewPanel(m_pCommandMenu = new CCommandMenu());
}

void CClientViewport::AddNewPanel(IViewportPanel *panel)
{
	m_Panels.push_back(panel);
	panel->SetParent(GetVPanel());
	dynamic_cast<vgui2::Panel *>(panel)->MakeReadyForUse();
}

void CClientViewport::ActivateClientUI()
{
	SetMouseInputEnabled(true);
}

void CClientViewport::HideClientUI()
{
	// Hide command menu when GameUI is opened
	if (m_pCommandMenu && m_pCommandMenu->IsVisible())
		m_pCommandMenu->ShowPanel(false);
}

void CClientViewport::VidInit()
{
	// Reset all panels when connecting to a server
	for (IViewportPanel *pPanel : m_Panels)
		pPanel->Reset();
}

bool CClientViewport::KeyInput(int down, int keynum, const char *pszCurrentBinding)
{
	if (down)
	{
		if (IsScoreBoardVisible())
		{
			if (!m_pScorePanel->IsMouseInputEnabled())
			{
				if ((keynum == K_MOUSE1 && hud_scoreboard_mousebtn.GetInt() == 1) || (keynum == K_MOUSE2 && hud_scoreboard_mousebtn.GetInt() == 2))
				{
					m_pScorePanel->EnableMousePointer(true);
					return 0;
				}
			}
			else
			{
				// Ignore mouse input so it can be handled by VGUI
				if (keynum == K_MOUSE1 || keynum == K_MOUSE2 || keynum == K_MWHEELDOWN || keynum == K_MWHEELUP)
				{
					return 0;
				}
			}
		}
		else
		{
			// Enter gets out of Spectator Mode by bringing up the Team Menu
			if (m_iUser1)
			{
				if (keynum == K_ENTER || keynum == K_KP_ENTER)
				{
					ShowVGUIMenu(MENU_TEAM);
					return 0;
				}
			}
		}
	}
	else
	{
	}

	return 1;
}

void CClientViewport::OnThink()
{
	// Fill the whole screen
	int wide, tall;
	int rootWide, rootTall;
	GetSize(wide, tall);
	g_pVGuiPanel->GetSize(GetVParent(), rootWide, rootTall);

	if (wide != rootWide || tall != rootTall)
	{
		SetBounds(0, 0, rootWide, rootTall);
		ReloadScheme(nullptr);
		ReloadLayout();
	}

	BaseClass::OnThink();

	m_pAnimController->UpdateAnimations(gEngfuncs.GetClientTime());

	// See if the Spectator Menu needs to be updated
	if ((g_iUser1 != m_iUser1 || g_iUser2 != m_iUser2) || (m_flSpectatorPanelLastUpdated < gHUD.m_flTime))
	{
		UpdateSpectatorPanel();
	}
}

void CClientViewport::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	gHUD.ApplyViewportSchemeSettings(pScheme);
}

void CClientViewport::ShowVGUIMenu(int iMenu)
{
	// Don't open menus in demo playback
	if (gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	// Don't open any menus except the MOTD during intermission
	// MOTD needs to be accepted because it's sent down to the client
	// after map change, before intermission's turned off
	if (gHUD.m_iIntermission && iMenu != MENU_MOTD)
		return;

	switch (iMenu)
	{
	case MENU_TEAM:
		m_pTeamMenu->Activate();
		break;
	case MENU_MOTD:
		m_pMOTD->Activate(m_szServerName, m_szMOTD);
		break;
	case MENU_HTML_MOTD:
		m_pMOTD->ActivateHtml(m_szServerName, m_szMOTD);
		break;

	case MENU_CLASSHELP:
	case MENU_SPECHELP:
	case MENU_CLASS:
		ConPrintf(ConColor::Yellow, "Warning: attempted to show TFC VGUI menu in HL: %d\n", iMenu);
		break;
	default:
		ConPrintf(ConColor::Red, "Error: attempted to show unknown VGUI menu: %d\n", iMenu);
		Assert(!("Invalid VGUI menu"));
		break;
	}
}

void CClientViewport::HideAllVGUIMenu()
{
	for (IViewportPanel *pPanel : m_Panels)
	{
		if (pPanel->IsVisible())
			pPanel->ShowPanel(false);
	}
}

bool CClientViewport::IsScoreBoardVisible()
{
	return m_pScorePanel->IsVisible();
}

void CClientViewport::ShowScoreBoard()
{
	if (gEngfuncs.GetMaxClients() > 1)
	{
		m_pScorePanel->ShowPanel(true);
	}
}

void CClientViewport::HideScoreBoard()
{
	// Prevent removal of scoreboard during intermission
	if (gHUD.m_iIntermission)
		return;

	m_pScorePanel->ShowPanel(false);
}

void CClientViewport::ShowHealthPanel()
{
	m_pHudHealthPanel->ShowPanel(true);
}

void CClientViewport::HideHealthPanel()
{
	m_pHudHealthPanel->ShowPanel(false);
}

void CClientViewport::IsHealthPanelVisible()
{
	if (m_pHudHealthPanel->IsVisible())
	{
		m_pHudHealthPanel->ShowPanel(false);
	}
	else
	{
		m_pHudHealthPanel->ShowPanel(true);
	}
}

void CClientViewport::UpdateHealthPanel(int health)
{
	m_pHudHealthPanel->UpdateHealthPanel(health);
}

void CClientViewport::ShowBatteryPanel()
{
	m_pHudBatteryPanel->ShowPanel(true);
}

void CClientViewport::HideBatteryPanel()
{
	m_pHudBatteryPanel->ShowPanel(false);
}

void CClientViewport::IsBatteryPanelVisible()
{
	if (m_pHudBatteryPanel->IsVisible())
	{
		m_pHudBatteryPanel->ShowPanel(false);
	}
	else
	{
		m_pHudBatteryPanel->ShowPanel(true);
	}
}

void CClientViewport::UpdateBatteryPanel(int amount)
{
	m_pHudBatteryPanel->UpdateBatteryPanel(amount);
}

void CClientViewport::ShowAmmoPanel()
{
	m_pHudAmmoPanel->ShowPanel(true);
}

void CClientViewport::HideAmmoPanel()
{
	m_pHudAmmoPanel->ShowPanel(false);
}

void CClientViewport::IsAmmoPanelVisible()
{
	if (m_pHudAmmoPanel->IsVisible())
	{
		m_pHudAmmoPanel->ShowPanel(false);
	}
	else
	{
		m_pHudAmmoPanel->ShowPanel(true);
	}
}

// Secondary ammo panel methods
void CClientViewport::ShowAmmoSecondaryPanel()
{
	m_pHudAmmoSecondaryPanel->ShowPanel(true);
}

void CClientViewport::HideAmmoSecondaryPanel()
{
	m_pHudAmmoSecondaryPanel->ShowPanel(false);
}

void CClientViewport::IsAmmoSecondaryPanelVisible()
{
	if (m_pHudAmmoSecondaryPanel->IsVisible())
	{
		m_pHudAmmoSecondaryPanel->ShowPanel(false);
	}
	else
	{
		m_pHudAmmoSecondaryPanel->ShowPanel(true);
	}
}

void CClientViewport::UpdateAmmoSecondaryPanel(WEAPON *pWeapon, int maxClip, int ammo1, int ammo2)
{
	m_pHudAmmoSecondaryPanel->UpdateAmmoSecondaryPanel(pWeapon, maxClip, ammo1, ammo2);
}

void CClientViewport::UpdateAmmoPanel(WEAPON *pWeapon, int maxClip, int ammo1, int ammo2)
{
	m_pHudAmmoPanel->UpdateAmmoPanel(pWeapon, maxClip, ammo1, ammo2);
}

int CClientViewport::GetAmmoHistoryYPos()
{
	return m_iAmmoHistoryYPos;
}

int CClientViewport::GetStatusBarYPos()
{
	return m_iStatusBarYPos;
}

void CClientViewport::UpdateSpectatorPanel()
{
	m_iUser1 = g_iUser1;
	m_iUser2 = g_iUser2;
	m_iUser3 = g_iUser3;

	if (g_iUser1 && hud_draw.GetFloat() > 0 && !gHUD.m_iIntermission)
	{
		// check if spectator combinations are still valid
		CHudSpectator::Get()->CheckSettings();

		if (!m_pSpectatorPanel->IsVisible())
		{
			m_pSpectatorPanel->ShowPanel(true); // show spectator panel, but
		}

		int player = 0;

		// check if we're locked onto a target, show the player's name
		if ((g_iUser2 > 0) && (g_iUser2 <= gEngfuncs.GetMaxClients()) && (g_iUser1 != OBS_ROAMING))
		{
			player = g_iUser2;
		}

		// special case in free map and inset off, don't show names
		if ((g_iUser1 != OBS_MAP_FREE && g_iUser1 != OBS_ROAMING || CHudSpectator::Get()->m_pip->value) && player && GetPlayerInfo(player)->Update()->IsConnected())
		{
			m_pSpectatorPanel->UpdateSpectatingPlayer(player);
		}
		else
		{
			// Hide player info panel
			m_pSpectatorPanel->UpdateSpectatingPlayer(0);
		}
	}
	else
	{
		if (m_pSpectatorPanel->IsVisible())
		{
			m_pSpectatorPanel->SetVisible(false);
		}
	}

	m_flSpectatorPanelLastUpdated = gHUD.m_flTime + 0.5; // next update interval
}

void CClientViewport::ShowCommandMenu()
{
	if (m_pCommandMenu->IsVisible())
		return;

	m_flMenuOpenTime = gHUD.m_flTime;
	m_bMenuIsKeyTapped = false;
	m_pCommandMenu->ShowPanel(true);
	m_pCommandMenu->UpdateMouseInputEnabled(true);
}

void CClientViewport::HideCommandMenu()
{
	m_flMenuOpenTime = 0;
	m_pCommandMenu->ShowPanel(false);
}

void CClientViewport::InputSignalHideCommandMenu()
{
	// if they've just tapped the command menu key, leave it open
	if (m_pCommandMenu->IsVisible() && (m_flMenuOpenTime + COMMAND_MENU_TAP_DELAY) > gHUD.m_flTime)
	{
		m_bMenuIsKeyTapped = true;
		m_pCommandMenu->UpdateMouseInputEnabled(false);
		return;
	}

	HideCommandMenu();
}

bool CClientViewport::SlotInput(int iSlot)
{
	// iSlot is int in [0; 9]
	// Note that
	// "slot1" == 0
	// "slot2" == 1
	// ...
	// "slot9" == 8
	// "slot0" == 9

	if (m_pCommandMenu->IsVisible())
	{
		m_pCommandMenu->SlotInput(iSlot);
		return true;
	}

	return false;
}

void CClientViewport::GetAllPlayersInfo(void)
{
	m_iNumberOfNonEmptyTeamPlayers = 0;

	for (int i = 1; i < MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i)->Update();

		if (pi->IsConnected() && pi->GetTeamName()[0] != '\0')
			m_iNumberOfNonEmptyTeamPlayers++;
	}
}

const char *CClientViewport::GetServerName()
{
	return m_szServerName;
}

void CClientViewport::UpdateOnPlayerInfo(int client)
{
	m_pScorePanel->UpdateOnPlayerInfo(client);
}

//-------------------------------------------------------
// Viewport messages
//-------------------------------------------------------
void CClientViewport::MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf)
{
	// TODO: Class menu
}

void CClientViewport::MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iNumberOfTeams = READ_BYTE();

	for (int i = 0; i < m_iNumberOfTeams; i++)
	{
		// Throw away invalid team numbers
		if (m_iNumberOfTeams > MAX_TEAMS)
		{
			READ_STRING();
			continue;
		}

		int teamNum = i + 1;
		CTeamInfo *ti = GetTeamInfo(teamNum);

		CHudTextMessage::LocaliseTextString(READ_STRING(), ti->m_DisplayName, sizeof(ti->m_DisplayName));

		// Parse the model and remove any %'s
		for (char *c = ti->m_DisplayName; *c != 0; c++)
		{
			// Replace it with a space
			if (*c == '%')
				*c = ' ';
		}
	}

	// Update the Team Menu
	if (m_pTeamMenu)
		m_pTeamMenu->Update();
}

void CClientViewport::MsgFunc_Feign(const char *pszName, int iSize, void *pbuf)
{
	// TFC: Spy disguise
}

void CClientViewport::MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf)
{
	// TFC: Something
}

void CClientViewport::MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iMenu = READ_BYTE();

	// Bring up the menu
	ShowVGUIMenu(iMenu);
}

void CClientViewport::MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf)
{
	if (m_iGotAllMOTD)
		m_szMOTD[0] = 0;

	BEGIN_READ(pbuf, iSize);

	m_iGotAllMOTD = READ_BYTE();

	int roomInArray = sizeof(m_szMOTD) - strlen(m_szMOTD) - 1;

	strncat(m_szMOTD, READ_STRING(), roomInArray >= 0 ? roomInArray : 0);
	m_szMOTD[sizeof(m_szMOTD) - 1] = '\0';

	// don't show MOTD for HLTV spectators
	if (m_iGotAllMOTD && !gEngfuncs.IsSpectateOnly())
	{
		ShowVGUIMenu(MENU_MOTD);
	}
}

void CClientViewport::MsgFunc_HtmlMOTD(const char *pszName, int iSize, void *pbuf)
{
	if (m_iGotAllMOTD)
		m_szMOTD[0] = 0;

	BEGIN_READ(pbuf, iSize);

	m_iGotAllMOTD = READ_BYTE();

	int roomInArray = sizeof(m_szMOTD) - strlen(m_szMOTD) - 1;

	strncat(m_szMOTD, READ_STRING(), roomInArray >= 0 ? roomInArray : 0);
	m_szMOTD[sizeof(m_szMOTD) - 1] = '\0';

	// don't show MOTD for HLTV spectators
	if (m_iGotAllMOTD && !gEngfuncs.IsSpectateOnly())
	{
		if (SteamAPI_IsAvailable() && gHUD.IsHTMLEnabled())
			ShowVGUIMenu(MENU_HTML_MOTD);
		else
			ShowVGUIMenu(MENU_MOTD);
	}
}

void CClientViewport::MsgFunc_BuildSt(const char *pszName, int iSize, void *pbuf)
{
	// TFC: Build State
}

void CClientViewport::MsgFunc_RandomPC(const char *pszName, int iSize, void *pbuf)
{
	// TFC: Random Player Class
}

void CClientViewport::MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	strncpy(m_szServerName, READ_STRING(), MAX_SERVERNAME_LENGTH);
	m_szServerName[MAX_SERVERNAME_LENGTH - 1] = 0;

	m_pScorePanel->UpdateServerName();
}

void CClientViewport::MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	short cl = READ_BYTE();
	short frags = READ_SHORT();
	short deaths = READ_SHORT();
	short playerclass = READ_SHORT();
	short teamnumber = READ_SHORT();

	if (cl > 0 && cl <= MAX_PLAYERS)
	{
		CPlayerInfo *info = GetPlayerInfo(cl)->Update();
		info->m_ExtraInfo.frags = frags;
		info->m_ExtraInfo.deaths = deaths;
		info->m_ExtraInfo.playerclass = playerclass;
		info->m_ExtraInfo.teamnumber = clamp(teamnumber, 0, MAX_TEAMS);

		UpdateOnPlayerInfo(cl);
	}
}

void CClientViewport::MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	char *TeamName = READ_STRING();
	int frags = READ_SHORT();
	int deaths = READ_SHORT();

	// find the team matching the display name
	int i;
	for (i = 1; i <= MAX_TEAMS; i++)
	{
		if (!Q_stricmp(TeamName, GetTeamInfo(i)->m_DisplayName))
			break;
	}

	if (i > MAX_TEAMS)
		return;

	// use this new score data instead of combined player scores
	CTeamInfo *ti = GetTeamInfo(i);
	ti->m_bScoreOverriden = true;
	ti->m_iFrags = frags;
	ti->m_iDeaths = deaths;
}

void CClientViewport::MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	short cl = READ_BYTE();

	if (cl > 0 && cl <= MAX_PLAYERS)
	{
		// set the players team
		CPlayerInfo *pi = GetPlayerInfo(cl)->Update();
		strncpy(pi->m_ExtraInfo.teamname, READ_STRING(), MAX_TEAM_NAME);
		UpdateOnPlayerInfo(cl);
	}
}

void CClientViewport::MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	short cl = READ_BYTE();
	if (cl > 0 && cl <= MAX_PLAYERS)
	{
		GetPlayerInfo(cl)->m_bIsSpectator = !!READ_BYTE();
	}
}

void CClientViewport::MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iAllowSpectators = READ_BYTE();
}

//-------------------------------------------------------
// TeamFortressViewport stubs
//-------------------------------------------------------
void CClientViewport::InputPlayerSpecial(void)
{
	// Originl purpose: activate the player special ability.
	// Only used in TFC.
	// In HL it just sends _special command to the server... using EngineClientCmd.
	// This used to allow people to alias it to create scripts that run every frame (e.g. for bhop).
	// Valve banned aliasing of _special it in the engine.
	EngineClientCmd("_special");
}

bool CClientViewport::AllowedToPrintText(void)
{
	return true;
}

void CClientViewport::DeathMsg(int killer, int victim)
{
	m_pScorePanel->DeathMsg(killer, victim);
}
