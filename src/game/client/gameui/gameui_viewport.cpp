#include <IBaseUI.h>
#include <IEngineVGui.h>
#include <vgui/ISurface.h>
#include <tier2/tier2.h>
#include "hud.h"
#include "cl_util.h"
#include "client_vgui.h"
#include "gameui_viewport.h"
#include "gameui_test_panel.h"
#include "options/adv_options_dialog.h"

CON_COMMAND(gameui_cl_open_test_panel, "Opens a test panel for client GameUI")
{
	CGameUIViewport::Get()->OpenTestPanel();
}

CGameUIViewport::CGameUIViewport()
    : BaseClass(nullptr, "ClientGameUIViewport")
{
	Assert(!m_sInstance);
	m_sInstance = this;

	SetParent(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL));
	SetScheme(vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/ClientSourceScheme.res", "ClientSourceScheme"));
	SetProportional(false);
	SetSize(0, 0);
}

CGameUIViewport::~CGameUIViewport()
{
	Assert(m_sInstance);
	m_sInstance = nullptr;
}

void CGameUIViewport::PreventEscapeToShow(bool state)
{
	if (state)
	{
		m_bPreventEscape = true;
		m_iDelayedPreventEscapeFrame = 0;
	}
	else
	{
		// PreventEscapeToShow(false) may be called the same frame that ESC was pressed
		// and CGameUIViewport::OnThink won't hide GameUI
		// So the change is delayed by one frame
		m_bPreventEscape = false;
		m_iDelayedPreventEscapeFrame = gHUD.GetFrameCount();
	}
}

void CGameUIViewport::OpenTestPanel()
{
	GetDialog(m_hTestPanel)->Activate();
}

CAdvOptionsDialog *CGameUIViewport::GetOptionsDialog()
{
	return GetDialog(m_hOptionsDialog);
}

void CGameUIViewport::OnThink()
{
	BaseClass::OnThink();

	if (m_bPreventEscape || m_iDelayedPreventEscapeFrame == gHUD.GetFrameCount())
	{
		const char *levelName = gEngfuncs.pfnGetLevelName();

		if (levelName && levelName[0])
		{
			g_pBaseUI->HideGameUI();

			// Hiding GameUI doesn't update the mouse cursor
			g_pVGuiSurface->CalculateMouseVisible();
		}
		else
		{
			// Disconnected from the server while prevent escape is enabled
			// Disable it
			m_bPreventEscape = false;
			m_iDelayedPreventEscapeFrame = 0;
		}
	}
}
