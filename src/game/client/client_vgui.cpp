#include <tier0/dbg.h>
#include <tier1/interface.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <IEngineVGui.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui/IPanel.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#include <convar.h>
#include "console.h"
#include "client_vgui.h"
#include "vgui/client_viewport.h"
#include "gameui/gameui_viewport.h"

EXPOSE_SINGLE_INTERFACE(CClientVGUI, IClientVGUI, ICLIENTVGUI_NAME);

namespace vgui2
{

HScheme VGui_GetDefaultScheme()
{
	return 0;
}

}

void CClientVGUI::Initialize(CreateInterfaceFn *pFactories, int iNumFactories)
{
	ConnectTier1Libraries(pFactories, iNumFactories);
	ConnectTier2Libraries(pFactories, iNumFactories);

	if (!vgui2::VGui_InitInterfacesList("CLIENT", pFactories, iNumFactories))
	{
		Error("Failed to intialize VGUI2\n");
		Assert(false);
	}

	// Override proportional scale
	// The Anniverssary Update changed the base resolution from 640x480 to 1280x720.
	// This causes all old UI to be down-scaled.
	vgui2::VGui_SetProportionalBaseCallback(&GetProportionalBase);

	// Add language files
	g_pVGuiLocalize->AddFile(g_pFullFileSystem, VGUI2_ROOT_DIR "resource/language/bugfixedhl_%language%.txt");

	new CClientViewport();
	new CGameUIViewport();
}

void CClientVGUI::Start()
{
	g_pViewport->Start();
}

void CClientVGUI::SetParent(vgui2::VPANEL parent)
{
}

int CClientVGUI::UseVGUI1()
{
	return false;
}

void CClientVGUI::HideScoreBoard()
{
	g_pViewport->HideScoreBoard();
}

void CClientVGUI::HideAllVGUIMenu()
{
	g_pViewport->HideAllVGUIMenu();
}

void CClientVGUI::ActivateClientUI()
{
	g_pViewport->ActivateClientUI();
}

void CClientVGUI::HideClientUI()
{
	g_pViewport->HideClientUI();
}

void CClientVGUI::Shutdown()
{
	// Warning! Only called for CS & CZ
	// Do not use!
}

void CClientVGUI::GetProportionalBase(int &wide, int &tall)
{
	// Pre-HL25 values
	wide = 640;
	tall = 480;
}

static void DumpPanel(vgui2::VPANEL panel, int offset, bool bParentVisible, bool bDumpAll)
{
	constexpr int INDENT_WIDTH = 4;
	char indent[256];

	memset(indent, ' ', sizeof(indent));
	if (offset * INDENT_WIDTH >= sizeof(indent))
		offset = (sizeof(indent) - 1) / INDENT_WIDTH;
	indent[offset * INDENT_WIDTH] = '\0';

	int wide, tall, x, y;
	g_pVGuiPanel->GetSize(panel, wide, tall);
	g_pVGuiPanel->GetPos(panel, x, y);

	bool bIsVisible = g_pVGuiPanel->IsVisible(panel) && bParentVisible;
	Color color = console::GetColor();
	const char *visibleStr = "";
	if (bParentVisible)
	{
		if (bIsVisible)
		{
			color = ConColor::Green;
			visibleStr = " [Vis]";
		}
		else
		{
			color = ConColor::Red;
		}
	}

	char flags[32];
	snprintf(flags, sizeof(flags), "%s%s%s",
	    g_pVGuiPanel->IsKeyBoardInputEnabled(panel) ? "K" : "",
	    g_pVGuiPanel->IsMouseInputEnabled(panel) ? "M" : "",
	    g_pVGuiPanel->IsPopup(panel) ? "P" : "");

	const char *name = g_pVGuiPanel->GetName(panel);
	if (!name)
		name = "< null name >";
	else if (!name[0])
		name = "< no name >";

	ConPrintf(color, "%s%s [%s %d x %d] @ (%d; %d) [%s]%s\n", indent,
	    name,
	    g_pVGuiPanel->GetClassName(panel),
	    wide, tall, x, y, flags, visibleStr);

	if (bIsVisible || bDumpAll)
	{
		int count = g_pVGuiPanel->GetChildCount(panel);
		for (int i = 0; i < count; i++)
		{
			DumpPanel(g_pVGuiPanel->GetChild(panel, i), offset + 1, bIsVisible, bDumpAll);
		}
	}
}

CON_COMMAND(vgui_dumptree, "Dumps VGUI2 panel tree for debugging.")
{
	ConPrintf("Usage:\n");
	ConPrintf("    vgui_dumptree - show list of visible panels\n");
	ConPrintf("    vgui_dumptree all - show list of all panels\n");
	ConPrintf("Color coding:\n");
	ConPrintf("    Green - visible\n");
	ConPrintf("    Red - hidden\n");
	ConPrintf("\n");

	bool bDumpAll = ConCommand::ArgC() > 1 && !strcmp(ConCommand::ArgV(1), "all");
	DumpPanel(g_pEngineVGui->GetPanel(PANEL_ROOT), 0, true, bDumpAll);
}
