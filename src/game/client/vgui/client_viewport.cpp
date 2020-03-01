#include <KeyValues.h>
#include <vgui_controls/AnimationController.h>
#include <IEngineVGui.h>

#include "client_viewport.h"
#include "client_vgui.h"
#include "hud.h"

// FIXME: Move it to hud.cpp
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

CClientViewport *gViewPort = nullptr;

CClientViewport::CClientViewport()
    : BaseClass(nullptr, "CClientViewport")
{
	Assert(!gViewPort);
	gViewPort = this;

	SetParent(g_pEngineVGui->GetPanel(PANEL_CLIENTDLL));

	SetSize(10, 10); // Quiet "parent not sized yet" spew
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	// create our animation controller
	m_pAnimController = new vgui2::AnimationController(this);
	m_pAnimController->SetProportional(true);

	// Load scheme
	ReloadScheme(VGUI2_ROOT_DIR "resource/ClientScheme.res");

	// Hide viewport
	HideClientUI();
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
		vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile(fromFile, "HudScheme");

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

	// reload the .res file from disk
	LoadControlSettings(VGUI2_ROOT_DIR "scripts/HudLayout.res");

	InvalidateLayout(true, true);
}

void CClientViewport::ActivateClientUI()
{
	SetVisible(true);
}

void CClientViewport::HideClientUI()
{
	SetVisible(false);
}

//-------------------------------------------------------
// Viewport messages
//-------------------------------------------------------
void CClientViewport::MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_Feign(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_BuildSt(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_RandomPC(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_SpecFade(const char *pszName, int iSize, void *pbuf)
{
}

void CClientViewport::MsgFunc_ResetFade(const char *pszName, int iSize, void *pbuf)
{
}

//-------------------------------------------------------
// TeamFortressViewport stubs
//-------------------------------------------------------
void CClientViewport::UpdateCursorState()
{
}

void CClientViewport::ShowCommandMenu(int menuIndex)
{
}

void CClientViewport::HideCommandMenu()
{
}

void CClientViewport::InputSignalHideCommandMenu()
{
}

void CClientViewport::InputPlayerSpecial(void)
{
}

bool CClientViewport::SlotInput(int iSlot)
{
	return false;
}

bool CClientViewport::AllowedToPrintText(void)
{
	return true;
}

void CClientViewport::DeathMsg(int killer, int victim)
{
}

void CClientViewport::GetAllPlayersInfo(void)
{
	for (int i = 1; i < MAX_PLAYERS; i++)
	{
		gEngfuncs.pfnGetPlayerInfo(i, &g_PlayerInfoList[i]);
	}
}

bool CClientViewport::IsScoreBoardVisible(void)
{
	return false;
}

void CClientViewport::ShowScoreBoard(void)
{
}

void CClientViewport::HideScoreBoard(void)
{
}

void CClientViewport::UpdateSpectatorPanel()
{
}

int CClientViewport::KeyInput(int down, int keynum, const char *pszCurrentBinding)
{
	return 1;
}
