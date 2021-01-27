#include <IEngineVgui.h>
#include <vgui_controls/PropertySheet.h>
#include "adv_options_dialog.h"
#include "client_vgui.h"
#include "gameui/gameui_viewport.h"
#include "IBaseUI.h"
#include "hud.h"
#include "cl_util.h"

#include "options_hud.h"
#include "options_chat.h"
#include "options_crosshair.h"
#include "options_scoreboard.h"
#include "options_general.h"
#include "options_about.h"

CON_COMMAND(gameui_cl_open_adv_options, "Opens Advanced Options dialog")
{
	// Since this command is called from game menu using "engine gameui_cl_open_adv_options"
	// GameUI will hide itself and show the game.
	// We need to show it again and after that activate CAdvOptionsDialog
	// Otherwise it may be hidden by the dev console
	gHUD.CallOnNextFrame([]() {
		CGameUIViewport::Get()->GetOptionsDialog()->Activate();
	});
	g_pBaseUI->ActivateGameUI();
}

CAdvOptionsDialog::CAdvOptionsDialog(vgui2::Panel *pParent)
    : BaseClass(pParent, "AdvOptionsDialog")
{
	SetBounds(0, 0, 512, 406);
	SetSizeable(false);
	SetDeleteSelfOnClose(true);

	SetTitle("#BHL_AdvOptions", true);

	AddPage(new CGeneralSubOptions(this), "#BHL_AdvOptions_General");
	AddPage(new CHudSubOptions(this), "#BHL_AdvOptions_HUD");
	AddPage(new CChatSubOptions(this), "#BHL_AdvOptions_Chat");
	AddPage(new CScoreboardSubOptions(this), "#BHL_AdvOptions_Scores");
	AddPage(new CCrosshairSubOptions(this), "#BHL_AdvOptions_Cross");
	AddPage(new CAboutSubOptions(this), "#BHL_AdvOptions_About");

	SetApplyButtonVisible(true);
	EnableApplyButton(true);
	GetPropertySheet()->SetTabWidth(84);
	MoveToCenterOfScreen();
}

void CAdvOptionsDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "Apply"))
	{
		BaseClass::OnCommand("Apply");
		EnableApplyButton(true);	// Apply should always be enabled
	}
	else
		BaseClass::OnCommand(command);
}
