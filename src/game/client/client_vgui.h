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
};

/**
 * Since Source 2007 KeyValues support platform conditionals:
 *     "name" "Tahoma"  [!$OSX]
 *     "name" "Verdana" [$OSX]
 * GoldSource, however, does not, so schemes containing them will be parsed incorrectly.
 *
 * This LoadSchemeFromFile will load fileName, parse conditionals and save the file
 * as filename_compiled.res and load it with vgui2::scheme()->LoadSchemeFromFile.
 * 
 * It will only do that if file was changed since last time it was compiled.
 */
vgui2::HScheme LoadSchemeFromFile(const char *fileName, const char *tag);

#endif
