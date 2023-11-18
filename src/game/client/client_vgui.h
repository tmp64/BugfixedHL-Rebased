#ifndef CLIENT_VGUI_H
#define CLIENT_VGUI_H
#include <vgui/VGUI2.h>
#include <IClientVGUI.h>

#define VGUI2_ROOT_DIR "ui/"

class CClientVGUI : public IClientVGUI
{
public:
	virtual void Initialize(CreateInterfaceFn *pFactories, int iNumFactories);
	virtual void Start();
	virtual void SetParent(vgui2::VPANEL parent);
	virtual int UseVGUI1();
	virtual void HideScoreBoard();
	virtual void HideAllVGUIMenu();
	virtual void ActivateClientUI();
	virtual void HideClientUI();
	virtual void Shutdown();

private:
	//! @returns The VGUI propertional scale factor.
	static float GetProportionalScale();
};

#endif
