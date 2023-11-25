#include <tier1/KeyValues.h>
#include "gameui/options/cvar_combo_box.h"
#include "hud.h"

CCVarComboBox::CCVarComboBox(vgui2::Panel *parent, const char *panelName, const char *cvarName)
    : BaseClass(parent, panelName, 0, false)
{
	m_pCvar = gEngfuncs.pfnGetCvarPointer(cvarName);
	if (!m_pCvar)
	{
		Msg("%s [CCVarComboBox]: cvar '%s' not found.\n", panelName, cvarName);
	}
}

void CCVarComboBox::AddItem(const char *itemText, const char *cvarValue)
{
	BaseClass::AddItem(itemText, new KeyValues("Item", "value", cvarValue));
	SetNumberOfEditLines(GetMenu()->GetItemCount());
}

void CCVarComboBox::ResetData()
{
	if (m_pCvar)
	{
		// Find the item with the current value
		vgui2::Menu *pMenu = GetMenu();
		int itemCount = pMenu->GetItemCount();

		for (int i = 0; i < itemCount; i++)
		{
			KeyValues *pUserData = pMenu->GetItemUserData(i);

			if (!strcmp(pUserData->GetString("value"), m_pCvar->string))
			{
				// Found it
				ActivateItem(i);
				break;
			}
		}
	}
}

void CCVarComboBox::ApplyChanges()
{
	if (m_pCvar)
	{
		const char *value = GetActiveItemUserData()->GetString("value");
		char cmd[256];
		snprintf(cmd, sizeof(cmd), "%s \"%s\"", m_pCvar->name, value);
		gEngfuncs.pfnClientCmd(cmd);
	}
}
