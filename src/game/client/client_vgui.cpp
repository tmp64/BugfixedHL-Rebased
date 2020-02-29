#include <tier0/dbg.h>
#include <tier1/interface.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <vgui_controls/Controls.h>
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
}

void CClientVGUI::HideAllVGUIMenu()
{
}

void CClientVGUI::ActivateClientUI()
{
}

void CClientVGUI::HideClientUI()
{
}

void CClientVGUI::Shutdown()
{
	// Warning! Only called for CS & CZ
	// Do not use!
}
