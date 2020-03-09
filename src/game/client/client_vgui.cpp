#include <tier0/dbg.h>
#include <tier1/interface.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <IEngineVGui.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui_controls/Controls.h>
#include <vgui/IPanel.h>
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
	char buf[256];
	memset(buf, ' ', sizeof(buf));
	if (offset * 10 >= sizeof(buf))
		offset = sizeof(buf) - 1;
	buf[offset] = '\0';

	int wide, tall;
	g_pVGuiPanel->GetSize(panel, wide, tall);

	bool bIsVisible = g_pVGuiPanel->IsVisible(panel) && bParentVisible;
	Color color = console::GetColor();
	if (bParentVisible)
	{
		if (bIsVisible)
			color = ConColor::Green;
		else
			color = ConColor::Red;
	}

	ConPrintf(color, "%s%s [%s %d x %d]\n", buf,
	    g_pVGuiPanel->GetName(panel),
	    g_pVGuiPanel->GetClassName(panel),
	    wide, tall);

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

vgui2::HScheme LoadSchemeFromFile(const char *fileNameOrig, const char *tag)
{
	char fileName[256];
	Q_StripExtension(fileNameOrig, fileName, sizeof(fileName));

	char fileNameComp[256];
	snprintf(fileNameComp, sizeof(fileNameComp), "%s_compiled.res", fileName);

	// Get orig file modification date
	int origModDate = g_pFullFileSystem->GetFileTime(fileNameOrig);

	// Try to open compiled file
	bool bNeedRecompile = false;
	KeyValues *compiled = new KeyValues("Scheme");
	if (compiled->LoadFromFile(g_pFullFileSystem, fileNameComp))
	{
		// Check modification date
		int modDate = compiled->GetInt("OrigModDate");
		if (origModDate == 0 || modDate == 0 || origModDate != modDate)
			bNeedRecompile = true;
	}
	else
	{
		// Failed to open
		bNeedRecompile = true;
	}
	compiled->deleteThis();

	if (bNeedRecompile)
	{
		// Load original file
		KeyValuesAD orig(new KeyValues("Scheme"));
		if (orig->LoadFromFile(g_pFullFileSystem, fileNameOrig))
		{
			// Set modification date
			orig->SetInt("OrigModDate", origModDate);

			// Save new file
			orig->SaveToFile(g_pFullFileSystem, fileNameComp);
		}
		else
		{
			Error("Failed to open scheme file: %s\n", fileNameOrig);
			return 0;
		}
	}

	// Load compiled file
	return vgui2::scheme()->LoadSchemeFromFile(fileNameComp, tag);
}
