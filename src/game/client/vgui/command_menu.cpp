#include <stdexcept>
#include <tier1/strtools.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include "command_menu.h"
#include "hud.h"
#include "cl_util.h"
#include "viewport_panel_names.h"

ConVar hud_cmdmenu_item_height("hud_cmdmenu_item_height", "22", FCVAR_BHL_ARCHIVE, "Height of command menu items");
ConVar hud_cmdmenu_noexec("hud_cmdmenu_noexec", "0", FCVAR_BHL_ARCHIVE, "Don't run any command menu commands, print to the console instead");

CCommandMenu::CCommandMenu()
    : BaseClass(nullptr, VIEWPORT_PANEL_COMMAND_MENU)
{
	SetProportional(true);
	UpdateMouseInputEnabled(false);
	SetKeyBoardInputEnabled(false);

	ReloadMenu();
}

bool CCommandMenu::ReloadMenu()
{
	DeleteAllItems();

	KeyValuesAD kv(new KeyValues("CommandMenu"));
	const char *menuFileName = "";

	if (g_pFullFileSystem->FileExists("commandmenu.txt"))
	{
		// Load commandmenu.txt
		menuFileName = "commandmenu.txt";
		if (!kv->LoadFromFile(g_pFullFileSystem, menuFileName))
		{
			ConPrintf(ConColor::Red, "Command Menu: Failed to parse commandmenu.txt.\n");
			ConPrintf(ConColor::Red, "Command Menu: See commandmenu_default.txt for correct syntax.\n");
			return false;
		}
	}
	else
	{
		// Load commandmenu_default.txt
		menuFileName = "commandmenu_default.txt";
		if (!kv->LoadFromFile(g_pFullFileSystem, menuFileName))
		{
			ConPrintf(ConColor::Red, "Command Menu: Failed to parse commandmenu_default.txt.\n");
			return false;
		}
	}

	bool bOldMouseInput = IsMouseInputEnabled();
	SetMouseInputEnabled(true);

	try
	{
		RecursiveLoadItems(kv, this, kv->GetName());
	}
	catch (const std::exception &e)
	{
		ConPrintf(ConColor::Red, "Command Menu: Failed to parse %s:\n", menuFileName);
		ConPrintf(ConColor::Red, "Command Menu: %s\n", e.what());
		DeleteAllItems();
		SetMouseInputEnabled(bOldMouseInput);
		return false;
	}

	SetMouseInputEnabled(bOldMouseInput);

	return true;
}

void CCommandMenu::SlotInput(int iSlot)
{
	vgui2::Menu *pMenu = this;

	while (pMenu)
	{
		int highlightedItem = pMenu->GetCurrentlyHighlightedItem();

		if (highlightedItem != -1)
		{
			vgui2::MenuItem *pItem = pMenu->GetMenuItem(highlightedItem);
			pMenu = pItem->GetMenu();
		}
		else
		{
			int item = pMenu->GetMenuID(iSlot);
			if (item != -1)
			{
				vgui2::MenuItem *pItem = pMenu->GetMenuItem(item);
				pMenu->SetCurrentlyHighlightedItem(item);

				if (pItem->GetMenu())
				{
					pItem->OpenCascadeMenu();
				}
				else
				{
					pItem->FireActionSignal();
					SetVisible(false);
				}
			}

			return;
		}
	}
}

void CCommandMenu::UpdateMouseInputEnabled(bool state)
{
	RecursiveChangeMenu(this, [state](vgui2::Menu *pMenu) {
		pMenu->SetMouseInputEnabled(state);
	});
}

void CCommandMenu::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CCommandMenu::OnCommand(const char *cmd)
{
	if (!strncmp(cmd, "engine_cmd ", 11))
	{
		const char *engcmd = cmd + 11;

		if (!engcmd[0])
		{
			ConPrintf(ConColor::Red, "Command Menu: Item has empty command.\n");
			return;
		}

		if (hud_cmdmenu_noexec.GetBool())
		{
			ConPrintf("Command Menu: %s\n", engcmd);
		}
		else
		{
			EngineClientCmd(engcmd);
		}
	}
	else
	{
		BaseClass::OnCommand(cmd);
	}
}

const char *CCommandMenu::GetName()
{
	return VIEWPORT_PANEL_COMMAND_MENU;
}

void CCommandMenu::Reset()
{
}

void CCommandMenu::ShowPanel(bool state)
{
	if (state == IsVisible())
		return;

	SetVisible(state);

	if (state)
	{
		SetKeyBoardInputEnabled(false);

		UpdateMenuItemHeight(vgui2::scheme()->GetProportionalScaledValue(hud_cmdmenu_item_height.GetInt()));

		int wide, tall;
		vgui2::surface()->GetScreenSize(wide, tall);
		SetPos(0, (tall - GetTall()) / 2);
	}
}

vgui2::VPANEL CCommandMenu::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CCommandMenu::IsVisible()
{
	return BaseClass::IsVisible();
}

void CCommandMenu::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}

void CCommandMenu::UpdateMenuItemHeight(int height)
{
	RecursiveChangeMenu(this, [height](vgui2::Menu *pMenu) {
		pMenu->SetMenuItemHeight(height);
		pMenu->InvalidateLayout();
	});
}

void CCommandMenu::RecursiveLoadItems(KeyValues *kv, vgui2::Menu *pParentMenu, std::string basePath)
{
	basePath += "/";

	int index = 0;

	for (KeyValues *pKey = kv->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey(), index++)
	{
		std::string path = basePath + pKey->GetName();
		wchar_t itemName[256];

		// Read name
		wchar_t indexChar = L'#';
		if (index >= 0 && index < 9)
			indexChar = L'0' + index + 1;
		else if (index == 9)
			indexChar = L'0';

		const char *name = pKey->GetString("Name", "");
		vgui2::StringIndex_t tokenIdx = vgui2::INVALID_STRING_INDEX;
		if (name[0] == '#' && (tokenIdx = g_pVGuiLocalize->FindIndex(name + 1)) != vgui2::INVALID_STRING_INDEX)
		{
			V_snwprintf(itemName, std::size(itemName), L"%c  %s", indexChar, g_pVGuiLocalize->GetValueByIndex(tokenIdx));
		}
		else
		{
			wchar_t wbuf[256];
			g_pVGuiLocalize->ConvertANSIToUnicode(name, wbuf, sizeof(wbuf));
			V_snwprintf(itemName, std::size(itemName), L"%c  %s", indexChar, wbuf);
		}

		// Read submenu
		KeyValues *pMenu = pKey->FindKey("Menu");

		if (pMenu)
		{
			vgui2::Menu *menu = new vgui2::Menu(nullptr, nullptr);
			menu->SetProportional(IsProportional());
			pParentMenu->AddCascadingMenuItem(pKey->GetName(), itemName, "CascadingMenuItemPressed", this, menu, nullptr);
			RecursiveLoadItems(pMenu, menu, path);
		}
		else
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "engine_cmd %s", pKey->GetString("Command", ""));
			pParentMenu->AddMenuItem(pKey->GetName(), itemName, buf, this, nullptr);
		}
	}
}
