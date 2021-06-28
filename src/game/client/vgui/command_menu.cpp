#include <stdexcept>
#include <memory>
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

constexpr char COMMAND_MENU_FILE[] = "commandmenu.txt";
constexpr char COMMAND_MENU_DEFAULT_FILE[] = "commandmenu_default.txt";

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

	const char *pszFileName = nullptr;

	if (g_pFullFileSystem->FileExists(COMMAND_MENU_FILE))
	{
		pszFileName = COMMAND_MENU_FILE;
	}
	else if (g_pFullFileSystem->FileExists(COMMAND_MENU_DEFAULT_FILE))
	{
		pszFileName = COMMAND_MENU_DEFAULT_FILE;
	}
	else
	{
		ConPrintf(ConColor::Red, "Command Menu: Neither %s nor %s were found.\n", COMMAND_MENU_FILE, COMMAND_MENU_DEFAULT_FILE);
		return false;
	}

	std::unique_ptr<char, void (*)(void *)> pFile(
	    reinterpret_cast<char *>(gEngfuncs.COM_LoadFile(pszFileName, 5, NULL)),
	    gEngfuncs.COM_FreeFile);

	bool bOldMouseInput = IsMouseInputEnabled();
	SetMouseInputEnabled(true);

	try
	{
		char *pfile = pFile.get();
		RecursiveLoadItems(pfile, this, "");
	}
	catch (const std::exception &e)
	{
		ConPrintf(ConColor::Red, "Command Menu: Failed to parse %s:\n", pszFileName);
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

void CCommandMenu::RecursiveLoadItems(char *&pfile, vgui2::Menu *pParentMenu, std::string basePath)
{
	basePath += "/";
	char token[1024];

	int index = 0;

	pfile = gEngfuncs.COM_ParseFile(pfile, token);

	// Keep looping until we hit the end of this menu
	while (token[0] != '}' && token[0] != '\0')
	{
		// token should already be the index but we ignore it

		// Get the button name
		pfile = gEngfuncs.COM_ParseFile(pfile, token);
		std::string text = token;
		std::string name = std::to_string(index) + text;
		std::string path = basePath + text;

		// Get the button command
		pfile = gEngfuncs.COM_ParseFile(pfile, token);
		std::string command = token;

		// Convert name to Unicode
		wchar_t itemText[256];
		wchar_t indexChar = L'#';
		if (index >= 0 && index < 9)
			indexChar = L'0' + index + 1;
		else if (index == 9)
			indexChar = L'0';

		vgui2::StringIndex_t tokenIdx = vgui2::INVALID_STRING_INDEX;
		if (text[0] == '#' && (tokenIdx = g_pVGuiLocalize->FindIndex(text.c_str() + 1)) != vgui2::INVALID_STRING_INDEX)
		{
			V_snwprintf(itemText, std::size(itemText), L"%lc  %ls", indexChar, g_pVGuiLocalize->GetValueByIndex(tokenIdx));
		}
		else
		{
			wchar_t wbuf[256];
			g_pVGuiLocalize->ConvertANSIToUnicode(text.c_str(), wbuf, sizeof(wbuf));
			V_snwprintf(itemText, std::size(itemText), L"%lc  %ls", indexChar, wbuf);
		}

		// Find out if it's a submenu or a button we're dealing with
		if (command[0] == '{')
		{
			vgui2::Menu *menu = new vgui2::Menu(nullptr, nullptr);
			menu->SetProportional(IsProportional());
			pParentMenu->AddCascadingMenuItem(name.c_str(), itemText, "CascadingMenuItemPressed", this, menu, nullptr);
			RecursiveLoadItems(pfile, menu, path);
		}
		else
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "engine_cmd %s", command.c_str());
			pParentMenu->AddMenuItem(name.c_str(), itemText, buf, this, nullptr);
		}

		pfile = gEngfuncs.COM_ParseFile(pfile, token);
		index++;
	}
}
