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

EXPOSE_SINGLE_INTERFACE(CClientVGUI, IClientVGUI, ICLIENTVGUI_NAME);

void CClientVGUI::Initialize(CreateInterfaceFn *pFactories, int iNumFactories)
{
	ConnectTier1Libraries(pFactories, iNumFactories);
	ConnectTier2Libraries(pFactories, iNumFactories);

	if (!vgui2::VGui_InitInterfacesList("CLIENT", pFactories, iNumFactories))
	{
		Error("Failed to intialize VGUI2\n");
		Assert(false);
	}

	// Add language files
	g_pVGuiLocalize->AddFile(g_pFullFileSystem, VGUI2_ROOT_DIR "resource/language/bugfixedhl_%language%.txt");

	new CClientViewport();
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

static void DumpPanel(vgui2::VPANEL panel, int offset, bool bParentVisible)
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
	if (bParentVisible)
	{
		if (bIsVisible)
			color = ConColor::Green;
		else
			color = ConColor::Red;
	}

	char flags[32];
	snprintf(flags, sizeof(flags), "%s%s%s",
	    g_pVGuiPanel->IsKeyBoardInputEnabled(panel) ? "K" : "",
	    g_pVGuiPanel->IsMouseInputEnabled(panel) ? "M" : "",
	    g_pVGuiPanel->IsPopup(panel) ? "P" : "");

	ConPrintf(color, "%s%s [%s %d x %d] @ (%d; %d) [%s]\n", indent,
	    g_pVGuiPanel->GetName(panel),
	    g_pVGuiPanel->GetClassName(panel),
	    wide, tall, x, y, flags);

	int count = g_pVGuiPanel->GetChildCount(panel);
	for (int i = 0; i < count; i++)
	{
		DumpPanel(g_pVGuiPanel->GetChild(panel, i), offset + 1, bIsVisible);
	}
}

CON_COMMAND(vgui_dumptree, "Dumps VGUI2 panel tree for debugging.")
{
	ConPrintf("Green - visible\nRed - hidden\n\n");
	DumpPanel(g_pEngineVGui->GetPanel(PANEL_ROOT), 0, true);
}
