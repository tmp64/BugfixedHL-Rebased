#include <string>
#include <vector>
#include <vgui_controls/Button.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/TextEntry.h>
#include <KeyValues.h>
#include "client_vgui.h"
#include "cvar_check_button.h"
#include "cvar_text_entry.h"
#include "options_models.h"
#include "hud.h"
#include "cl_util.h"

extern ConVar cl_forceenemymodels;

CModelSubOptions::CModelSubOptions(vgui2::Panel *parent)
    : BaseClass(parent, "CModelSubOptions")
{
	SetSize(100, 100); // Silence "parent not sized yet" warning

	m_pEnemyModels = new vgui2::ListPanel(this, "EnemyModels");
	m_pEnemyModels->AddColumnHeader(0, "ModelName", "#BHL_AdvOptions_Models_ModelName", 1024, 0, 1024, vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW);
	m_pEnemyModels->SetColumnSortable(0, false);
	m_pEnemyModels->SetIgnoreDoubleClick(true);

	m_pAddEnemyModel = new vgui2::Button(this, "AddEnemyModel", "#BHL_AdvOptions_Models_Add", this, "AddEnemyModel");
	m_pRemoveEnemyModel = new vgui2::Button(this, "RemoveEnemyModel", "#BHL_AdvOptions_Models_Remove", this, "RemoveEnemyModel");
	m_pRemoveAllEnemyModels = new vgui2::Button(this, "RemoveAllEnemyModels", "#BHL_AdvOptions_Models_RemoveAll", this, "RemoveAllEnemyModels");
	m_pNewEnemyModelName = new vgui2::TextEntry(this, "NewEnemyModelName");

	m_pTeamModel = new CCvarTextEntry(this, "TeamModel", "cl_forceteammatesmodels");
	m_pEnemyColors = new CCvarTextEntry(this, "EnemyColors", "cl_forceenemycolors");
	m_pTeamColors = new CCvarTextEntry(this, "TeamColors", "cl_forceteammatescolors");
	m_pHideCorpses = new CCvarCheckButton(this, "HideCorpses", "#BHL_AdvOptions_Models_HideCorpses", "cl_hidecorpses");
	m_pLeftHand = new CCvarCheckButton(this, "LeftHand", "#BHL_AdvOptions_Models_LeftHand", "cl_righthand");
	m_pAngledBob = new CCvarCheckButton(this, "AngledBob", "#BHL_AdvOptions_Models_AngledBob", "cl_bob_angled");
	m_pNoShells = new CCvarCheckButton(this, "NoShells", "#BHL_AdvOptions_Models_NoShells", "cl_noshells");
	m_pNoViewModel = new CCvarCheckButton(this, "NoViewModel", "#BHL_AdvOptions_Models_NoViewmodel", "r_drawviewmodel", true);

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/ModelSubOptions.res");
}

void CModelSubOptions::OnCommand(const char *cmd)
{
	if (!Q_stricmp(cmd, "AddEnemyModel"))
	{
		std::vector<wchar_t> buf(m_pNewEnemyModelName->GetTextLength() + 1);
		m_pNewEnemyModelName->GetText(buf.data(), buf.size() * sizeof(wchar_t));
		Q_StripPrecedingAndTrailingWhitespaceW(buf.data());
		int len = Q_wcslen(buf.data());

		if (len != 0)
		{
			KeyValuesAD kv((std::string("Item") + std::to_string(m_iNewItemIdx)).c_str());
			kv->SetWString("ModelName", buf.data());
			m_pEnemyModels->AddItem(kv, 0, false, false);
			m_pNewEnemyModelName->SetText(L"");
			m_iNewItemIdx++;
		}
	}
	else if (!Q_stricmp(cmd, "RemoveEnemyModel"))
	{
		int numSelected = m_pEnemyModels->GetSelectedItemsCount();

		if (numSelected != 0)
		{
			std::vector<int> itemsToRemove(numSelected);

			for (int i = 0; i < numSelected; i++)
			{
				itemsToRemove[i] = m_pEnemyModels->GetSelectedItem(i);
			}

			m_pEnemyModels->ClearSelectedItems();

			for (int i = 0; i < numSelected; i++)
			{
				m_pEnemyModels->RemoveItem(itemsToRemove[i]);
			}
		}
	}
	else if (!Q_stricmp(cmd, "RemoveAllEnemyModels"))
	{
		m_pEnemyModels->RemoveAll();
	}
	else
	{
		BaseClass::OnCommand(cmd);
	}
}

void CModelSubOptions::OnResetData()
{
	ParseEnemyModels();
	m_pTeamModel->ResetData();
	m_pEnemyColors->ResetData();
	m_pTeamColors->ResetData();
	m_pHideCorpses->ResetData();
	m_pLeftHand->ResetData();
	m_pAngledBob->ResetData();
	m_pNoShells->ResetData();
	m_pNoViewModel->ResetData();
}

void CModelSubOptions::OnApplyChanges()
{
	ApplyEnemyModels();
	m_pTeamModel->ApplyChanges();
	m_pEnemyColors->ApplyChanges();
	m_pTeamColors->ApplyChanges();
	m_pHideCorpses->ApplyChanges();
	m_pLeftHand->ApplyChanges();
	m_pAngledBob->ApplyChanges();
	m_pNoShells->ApplyChanges();
	m_pNoViewModel->ApplyChanges();
}

void CModelSubOptions::ParseEnemyModels()
{
	m_pEnemyModels->RemoveAll();

	char buffer[MAX_TEAM_NAME];
	char *buffer_end = buffer + sizeof(buffer) - 1;

	const char *src = cl_forceenemymodels.GetString();
	char *dst = buffer;
	int itemIdx = 0;
	while (*src != 0)
	{
		while (*src != ';' && *src != 0)
		{
			if (dst < buffer_end)
			{
				*dst = *src;
				dst++;
			}
			src++;
		}
		*dst = 0;
		dst = buffer;
		if (*src == ';')
			src++;
		if (buffer[0] == 0)
			continue;

		// Add the model to the list
		KeyValuesAD kv((std::string("Item") + std::to_string(itemIdx)).c_str());
		kv->SetString("ModelName", buffer);
		m_pEnemyModels->AddItem(kv, 0, false, false);
		itemIdx++;
	}
}

void CModelSubOptions::ApplyEnemyModels()
{
	std::string value;
	int items = m_pEnemyModels->GetItemCount();

	for (int i = 0; i < items; i++)
	{
		int item = m_pEnemyModels->GetItemIDFromRow(i);
		KeyValues *kv = m_pEnemyModels->GetItem(item);
		value.append(kv->GetString("ModelName"));
		value.push_back(';');
	}

	// Erase last semicolon
	if (!value.empty())
		value.erase(value.size() - 1);

	cl_forceenemymodels.SetValue(value.c_str());
}
