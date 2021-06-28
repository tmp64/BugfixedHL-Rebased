#ifndef VGUI_COMMAND_MENU_H
#define VGUI_COMMAND_MENU_H
#include <string>
#include <vector>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>
#include "IViewportPanel.h"

class CCommandMenu : public vgui2::Menu, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CCommandMenu, vgui2::Menu);

	CCommandMenu();
	bool ReloadMenu();
	void SlotInput(int iSlot);
	void UpdateMouseInputEnabled(bool state);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void OnCommand(const char *cmd) override;

	//IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

private:
	template <typename T>
	inline void RecursiveChangeMenu(vgui2::Menu *pMenu, T func)
	{
		func(pMenu);

		for (int i = 0; i < pMenu->GetItemCount(); i++)
		{
			vgui2::MenuItem *pItem = pMenu->GetMenuItem(i);

			if (pItem->GetMenu())
				RecursiveChangeMenu(pItem->GetMenu(), func);
		}
	}

	void UpdateMenuItemHeight(int height);
	void RecursiveLoadItems(char *&pfile, vgui2::Menu *pParentMenu, std::string basePath);
};

#endif
