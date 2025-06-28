#ifndef VGUI_HUD_AMMO_SECONDARY_H
#define VGUI_HUD_AMMO_SECONDARY_H

#include <vgui_controls/EditablePanel.h>
#include "global_consts.h"
#include "IViewportPanel.h"
#include "viewport_panel_names.h"

namespace vgui2
{
class Label;
}

class CTGAImage;

class CHudAmmoSecondaryPanel : public vgui2::EditablePanel, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CHudAmmoSecondaryPanel, vgui2::EditablePanel);

	CHudAmmoSecondaryPanel();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PaintBackground() override;

	void UpdateAmmoSecondaryPanel(WEAPON *pWeapon, int maxClip, int ammo1, int ammo2);

	void OnThink() override;

	//IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

private:
	vgui2::Label *m_pAmmoLabel = nullptr;
	vgui2::Label *m_pAmmoBgLabel = nullptr;
	vgui2::Label *m_pAmmoLabelGlow = nullptr;

	CPanelAnimationVarAliasType(int, m_iAmmoIconX, "ammoicon_xpos", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconY, "ammoicon_ypos", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconWide, "ammoicon_wide", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconTall, "ammoicon_tall", "0", "proportional_int");
	
	// TGA images for ammo types
	CTGAImage *m_pAmmoIcon = nullptr;
	CTGAImage *m_pAmmoIconDefault = nullptr;
	CTGAImage *m_pAmmoGrenadeMP5 = nullptr;

	float m_fFade = 0.0f;
	Color m_hudCurrentColor{};

	// Weapon data
	char m_szName[MAX_WEAPON_NAME];
	int m_iAmmoType;
	int m_iAmmo2Type;
	int m_iMax1;
	int m_iMax2;
	int m_iId;
	int m_iClip;
	int m_iMaxClip;
	int m_iAmmoCount;
	int m_iAmmoCount2;

	ConVarRef m_pHudDim{"hud_dim"};
};

#endif
