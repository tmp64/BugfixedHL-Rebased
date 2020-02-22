#ifndef CLIENT_VGUI_H
#define CLIENT_VGUI_H
#include <IClientVGUI.h>

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
};

#endif
