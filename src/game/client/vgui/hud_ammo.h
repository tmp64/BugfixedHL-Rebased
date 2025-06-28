#ifndef VGUI_HUD_AMMO_H
#define VGUI_HUD_AMMO_H

#include <vgui_controls/EditablePanel.h>
#include "global_consts.h"
#include "IViewportPanel.h"
#include "viewport_panel_names.h"

namespace vgui2
{
class Label;
}

class CTGAImage;

class CHudAmmoPanel : public vgui2::EditablePanel, public IViewportPanel
{
public:
	DECLARE_CLASS_SIMPLE(CHudAmmoPanel, vgui2::EditablePanel);

	CHudAmmoPanel();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PaintBackground() override;

	void UpdateAmmoPanel(WEAPON *pWeapon, int maxClip, int ammo1, int ammo2);

	void OnThink() override;

	//IViewportPanel overrides
	virtual const char *GetName() override;
	virtual void Reset() override;
	virtual void ShowPanel(bool state) override;
	virtual vgui2::VPANEL GetVPanel() override;
	virtual bool IsVisible() override;
	virtual void SetParent(vgui2::VPANEL parent) override;

private:
	vgui2::Label *m_pDigitLeftLabel = nullptr;
	vgui2::Label *m_pDigitLeftBgLabel = nullptr;
	vgui2::Label *m_pDigitLeftLabelGlow = nullptr;
	vgui2::Label *m_pDigitRightLabel = nullptr;
	vgui2::Label *m_pDigitRightBgLabel = nullptr;
	vgui2::Label *m_pDigitRightLabelGlow = nullptr;

	int m_iBarX = 0;
	int m_iBarFullX = 0;
	CPanelAnimationStringVar(32, m_szBarFullX, "bar_full_xpos", "100");
	CPanelAnimationStringVar(32, m_szBarX, "bar_xpos", "100");

	CPanelAnimationVar(Color, m_DividerOffColor, "divider_offcolor", "128 128 128 96");
	CPanelAnimationVarAliasType(int, m_iDividerX, "divider_xpos", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iDividerY, "divider_ypos", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iDividerWide, "divider_wide", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iDividerTall, "divider_tall", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBarWide, "bar_wide", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBarFullWide, "bar_full_wide", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBarShrink, "bar_shrink", "1", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconX, "ammoicon_xpos", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconFullX, "ammoicon_full_xpos", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconY, "ammoicon_ypos", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconWide, "ammoicon_wide", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iAmmoIconTall, "ammoicon_tall", "0", "proportional_int");
	
	// TGA images for ammo types
	CTGAImage *m_pAmmoIcon = nullptr;
	CTGAImage *m_pAmmoIconDefault = nullptr;
	CTGAImage *m_pAmmo357 = nullptr;
	CTGAImage *m_pAmmo9mm = nullptr;
	CTGAImage *m_pAmmoBolt = nullptr;
	CTGAImage *m_pAmmoBuckshot = nullptr;
	CTGAImage *m_pAmmoEnergy = nullptr;
	CTGAImage *m_pAmmoGrenadeFrag = nullptr;
	CTGAImage *m_pAmmoGrenadeHornet = nullptr;
	CTGAImage *m_pAmmoGrenadeMP5 = nullptr;
	CTGAImage *m_pAmmoGrenadeRPG = nullptr;
	CTGAImage *m_pAmmoGrenadeSatchel = nullptr;
	CTGAImage *m_pAmmoGrenadeTripmine = nullptr;
	CTGAImage *m_pAmmoSnark = nullptr;

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
