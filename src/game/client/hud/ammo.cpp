/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"

#include <string.h>
#include <stdio.h>

#include "ammo.h"
#include "ammohistory.h"
#include "crosshair.h"
#include "menu.h"
#include "vgui/client_viewport.h"

ConVar hud_fastswitch("hud_fastswitch", "0", FCVAR_ARCHIVE, "Controls whether or not weapons can be selected in one keypress");
ConVar hud_weapon("hud_weapon", "0", FCVAR_BHL_ARCHIVE, "Controls displaying sprite of currently selected weapon");
extern ConVar cl_cross_zoom;

WEAPON *gpActiveSel; // NULL means off, 1 means just the menu bar, otherwise
    // this points to the active weapon menu item
WEAPON *gpLastSel; // Last weapon menu selection

client_sprite_t *GetSpriteFromList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

WeaponsResource gWR;

int g_weaponselect = 0;

void WeaponsResource::LoadAllWeaponSprites(void)
{
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if (rgWeapons[i].iId)
			LoadWeaponSprites(&rgWeapons[i]);
	}
}

int WeaponsResource::CountAmmo(int iId)
{
	if (iId < 0)
		return 0;

	return riAmmo[iId];
}

int WeaponsResource::HasAmmo(WEAPON *p)
{
	if (!p)
		return FALSE;

	// weapons with no max ammo can always be selected
	if (p->iMax1 == -1)
		return TRUE;

	return (p->iAmmoType == -1) || p->iClip > 0 || CountAmmo(p->iAmmoType)
	    || CountAmmo(p->iAmmo2Type) || (p->iFlags & WEAPON_FLAGS_SELECTONEMPTY);
}

void WeaponsResource::LoadWeaponSprites(WEAPON *pWeapon)
{
	int i, iRes;

	if (ScreenWidth < 640)
		iRes = 320;
	else
		iRes = 640;

	char sz[256];

	if (!pWeapon)
		return;

	memset(&pWeapon->rcActive, 0, sizeof(wrect_t));
	memset(&pWeapon->rcInactive, 0, sizeof(wrect_t));
	memset(&pWeapon->rcAmmo, 0, sizeof(wrect_t));
	memset(&pWeapon->rcAmmo2, 0, sizeof(wrect_t));
	memset(&pWeapon->rcCrosshair, 0, sizeof(wrect_t));
	memset(&pWeapon->rcAutoaim, 0, sizeof(wrect_t));
	memset(&pWeapon->rcZoomedCrosshair, 0, sizeof(wrect_t));
	memset(&pWeapon->rcZoomedAutoaim, 0, sizeof(wrect_t));
	pWeapon->hInactive = 0;
	pWeapon->hActive = 0;
	pWeapon->hAmmo = 0;
	pWeapon->hAmmo2 = 0;
	pWeapon->hCrosshair = 0;
	pWeapon->hAutoaim = 0;
	pWeapon->hZoomedCrosshair = 0;
	pWeapon->hZoomedAutoaim = 0;

	snprintf(sz, sizeof(sz), "sprites/%s.txt", pWeapon->szName);
	client_sprite_t *pList = SPR_GetList(sz, &i);

	if (!pList)
		return;

	client_sprite_t *p;

	p = GetSpriteFromList(pList, "crosshair", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hCrosshair = SPR_Load(sz);
		pWeapon->rcCrosshair = p->rc;
	}

	p = GetSpriteFromList(pList, "autoaim", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hAutoaim = SPR_Load(sz);
		pWeapon->rcAutoaim = p->rc;
	}

	p = GetSpriteFromList(pList, "zoom", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hZoomedCrosshair = SPR_Load(sz);
		pWeapon->rcZoomedCrosshair = p->rc;
	}
	else
	{
		pWeapon->hZoomedCrosshair = pWeapon->hCrosshair; //default to non-zoomed crosshair
		pWeapon->rcZoomedCrosshair = pWeapon->rcCrosshair;
	}

	p = GetSpriteFromList(pList, "zoom_autoaim", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hZoomedAutoaim = SPR_Load(sz);
		pWeapon->rcZoomedAutoaim = p->rc;
	}
	else
	{
		pWeapon->hZoomedAutoaim = pWeapon->hZoomedCrosshair; //default to zoomed crosshair
		pWeapon->rcZoomedAutoaim = pWeapon->rcZoomedCrosshair;
	}

	p = GetSpriteFromList(pList, "weapon_s", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hActive = SPR_Load(sz);
		pWeapon->rcActive = p->rc;
	}

	p = GetSpriteFromList(pList, "weapon", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hInactive = SPR_Load(sz);
		pWeapon->rcInactive = p->rc;

		gHR.iHistoryGap = max(gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top);
	}

	p = GetSpriteFromList(pList, "ammo", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hAmmo = SPR_Load(sz);
		pWeapon->rcAmmo = p->rc;

		gHR.iHistoryGap = max(gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top);
	}

	p = GetSpriteFromList(pList, "ammo2", iRes, i);
	if (p)
	{
		snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);
		pWeapon->hAmmo2 = SPR_Load(sz);
		pWeapon->rcAmmo2 = p->rc;

		gHR.iHistoryGap = max(gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top);
	}

	// Load all "damage by" sprites into global sprites list
	p = GetSpriteFromList(pList, "d_", iRes, i);
	while (p != NULL)
	{
		gHUD.AddSprite(*p);
		p++;
		p = GetSpriteFromList(p, "d_", iRes, i - (p - pList));
	}
}

// Returns the first weapon for a given slot.
WEAPON *WeaponsResource::GetFirstPos(int iSlot)
{
	if (iSlot >= MAX_WEAPON_SLOTS)
		return NULL;

	WEAPON *pret = NULL;

	for (int i = 0; i < MAX_WEAPON_POSITIONS; i++)
	{
		if (rgSlots[iSlot][i] && HasAmmo(rgSlots[iSlot][i]))
		{
			pret = rgSlots[iSlot][i];
			break;
		}
	}

	return pret;
}

WEAPON *WeaponsResource::GetNextActivePos(int iSlot, int iSlotPos)
{
	if (iSlot >= MAX_WEAPON_SLOTS || iSlotPos + 1 >= MAX_WEAPON_POSITIONS)
		return NULL;

	WEAPON *p = gWR.rgSlots[iSlot][iSlotPos + 1];

	if (!p || !gWR.HasAmmo(p))
		return GetNextActivePos(iSlot, iSlotPos + 1);

	return p;
}

int giBucketHeight, giBucketWidth, giABHeight, giABWidth; // Ammo Bar width and height

HSPRITE ghsprBuckets; // Sprite for top row of weapons menu

// width of ammo fonts
#define AMMO_SMALL_WIDTH 10
#define AMMO_LARGE_WIDTH 20

#define HISTORY_DRAW_TIME "5"

DEFINE_HUD_ELEM(CHudAmmo);

void CHudAmmo::Init()
{
	BaseHudClass::Init();

	HookMessage<&CHudAmmo::MsgFunc_CurWeapon>("CurWeapon"); // Current weapon and clip
	HookMessage<&CHudAmmo::MsgFunc_WeaponList>("WeaponList"); // new weapon type
	HookMessage<&CHudAmmo::MsgFunc_AmmoPickup>("AmmoPickup"); // flashes an ammo pickup record
	HookMessage<&CHudAmmo::MsgFunc_WeapPickup>("WeapPickup"); // flashes a weapon pickup record
	HookMessage<&CHudAmmo::MsgFunc_ItemPickup>("ItemPickup");
	HookMessage<&CHudAmmo::MsgFunc_HideWeapon>("HideWeapon"); // hides the weapon, ammo, and crosshair displays temporarily
	HookMessage<&CHudAmmo::MsgFunc_AmmoX>("AmmoX"); // update known ammo type's count

	HookCommand<&CHudAmmo::UserCmd_Slot1>("slot1");
	HookCommand<&CHudAmmo::UserCmd_Slot2>("slot2");
	HookCommand<&CHudAmmo::UserCmd_Slot3>("slot3");
	HookCommand<&CHudAmmo::UserCmd_Slot4>("slot4");
	HookCommand<&CHudAmmo::UserCmd_Slot5>("slot5");
	HookCommand<&CHudAmmo::UserCmd_Slot6>("slot6");
	HookCommand<&CHudAmmo::UserCmd_Slot7>("slot7");
	HookCommand<&CHudAmmo::UserCmd_Slot8>("slot8");
	HookCommand<&CHudAmmo::UserCmd_Slot9>("slot9");
	HookCommand<&CHudAmmo::UserCmd_Slot10>("slot10");
	HookCommand<&CHudAmmo::UserCmd_Close>("cancelselect");
	HookCommand<&CHudAmmo::UserCmd_NextWeapon>("invnext");
	HookCommand<&CHudAmmo::UserCmd_PrevWeapon>("invprev");

	Reset();

	m_iFlags |= HUD_ACTIVE; //!!!

	gWR.Init();
	gHR.Init();
};

void CHudAmmo::Reset(void)
{
	m_fFade = 0;
	m_pWeapon = NULL;
	m_fOnTarget = FALSE;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay = 0;

	gWR.Reset();
	gHR.Reset();

	//	VidInit();

	m_iMaxSlot = 4;
}

void CHudAmmo::VidInit()
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex("bucket1");
	m_HUD_selection = gHUD.GetSpriteIndex("selection");

	ghsprBuckets = gHUD.GetSprite(m_HUD_bucket0);
	giBucketWidth = gHUD.GetSpriteRect(m_HUD_bucket0).right - gHUD.GetSpriteRect(m_HUD_bucket0).left;
	giBucketHeight = gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top;

	gHR.iHistoryGap = max(gHR.iHistoryGap, gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top);

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	if (ScreenWidth >= 640)
	{
		giABWidth = 20;
		giABHeight = 4;
	}
	else
	{
		giABWidth = 10;
		giABHeight = 2;
	}
}

//
// Think:
//  Used for selection of weapon menu item.
//
void CHudAmmo::Think(void)
{
	if (gHUD.m_iFOV != m_iLastFOV)
	{
		// Update crosshair after zoom change
		UpdateCrosshair();
		m_iLastFOV = gHUD.m_iFOV;
	}

	if (gHUD.m_fPlayerDead)
		return;

	if (gHUD.m_iWeaponBits != gWR.iOldWeaponBits)
	{
		gWR.iOldWeaponBits = gHUD.m_iWeaponBits;

		for (int i = MAX_WEAPONS - 1; i > 0; i--)
		{
			WEAPON *p = gWR.GetWeapon(i);

			if (p)
			{
				if (gHUD.m_iWeaponBits & (1 << p->iId))
					gWR.PickupWeapon(p);
				else
					gWR.DropWeapon(p);
			}
		}
	}

	if (!gpActiveSel)
		return;

	// has the player selected one?
	if (gHUD.m_iKeyBits & IN_ATTACK || hud_fastswitch.GetInt() == 2)
	{
		if (gpActiveSel != (WEAPON *)1)
		{
			ServerCmd(gpActiveSel->szName);
			g_weaponselect = gpActiveSel->iId;
		}

		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		PlaySound("common/wpn_select.wav", 1);
	}
}

//
// Helper function to return a Ammo pointer from id
//

HSPRITE *WeaponsResource::GetAmmoPicFromWeapon(int iAmmoId, wrect_t &rect)
{
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if (rgWeapons[i].iAmmoType == iAmmoId)
		{
			rect = rgWeapons[i].rcAmmo;
			return &rgWeapons[i].hAmmo;
		}
		else if (rgWeapons[i].iAmmo2Type == iAmmoId)
		{
			rect = rgWeapons[i].rcAmmo2;
			return &rgWeapons[i].hAmmo2;
		}
	}

	return NULL;
}

// Menu Selection Code

void WeaponsResource::SelectSlot(int iSlot, int fAdvance, int iDirection)
{
	if (CHudMenu::Get()->m_fMenuDisplayed && (fAdvance == FALSE) && (iDirection == 1))
	{ // menu is overriding slot use commands
		CHudMenu::Get()->SelectMenuItem(iSlot + 1); // slots are one off the key numbers
		return;
	}

	if (iSlot > CHudAmmo::Get()->GetMaxSlot())
		return;

	if (gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL))
		return;

	if (!(gHUD.m_iWeaponBits & (1 << (WEAPON_SUIT))))
		return;

	if (!(gHUD.m_iWeaponBits & ~(1 << (WEAPON_SUIT))))
		return;

	WEAPON *p = NULL;
	bool fastSwitch = hud_fastswitch.GetInt() != 0;

	if ((gpActiveSel == NULL) || (gpActiveSel == (WEAPON *)1) || (iSlot != gpActiveSel->iSlot))
	{
		PlaySound("common/wpn_hudon.wav", 1);
		p = GetFirstPos(iSlot);

		if (p && fastSwitch) // check for fast weapon switch mode
		{
			// if fast weapon switch is on, then weapons can be selected in a single keypress
			// but only if there is only one item in the bucket
			WEAPON *p2 = GetNextActivePos(p->iSlot, p->iSlotPos);
			if (!p2)
			{ // only one active item in bucket, so change directly to weapon
				ServerCmd(p->szName);
				g_weaponselect = p->iId;
				return;
			}
		}
	}
	else
	{
		PlaySound("common/wpn_moveselect.wav", 1);
		if (gpActiveSel)
			p = GetNextActivePos(gpActiveSel->iSlot, gpActiveSel->iSlotPos);
		if (!p)
			p = GetFirstPos(iSlot);
	}

	if (!p) // no selection found
	{
		// just display the weapon list, unless fastswitch is on just ignore it
		if (!fastSwitch)
			gpActiveSel = (WEAPON *)1;
		else
			gpActiveSel = NULL;
	}
	else
		gpActiveSel = p;
}

//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
//
int CHudAmmo::MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	gWR.SetAmmo(iIndex, abs(iCount));

	m_fFade = FADE_TIME;

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	// Add ammo to the history
	gHR.AddToHistory(HISTSLOT_AMMO, iIndex, abs(iCount));

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int iIndex = READ_BYTE();

	// Add the weapon to the history
	gHR.AddToHistory(HISTSLOT_WEAP, iIndex);

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory(HISTSLOT_ITEM, szName);

	return 1;
}

int CHudAmmo::MsgFunc_HideWeapon(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if (gEngfuncs.IsSpectateOnly())
		return 1;

	if (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL))
	{
		static wrect_t nullrc;
		gpActiveSel = NULL;
		SetCrosshair(0, nullrc, 0, 0, 0);
	}
	else
	{
		UpdateCrosshair();
	}

	return 1;
}

//
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf)
{
	static wrect_t nullrc;

	BEGIN_READ(pbuf, iSize);

	int iState = READ_BYTE();
	int iId = READ_CHAR();
	int iClip = READ_CHAR();

	// detect if we're also on target
	if (iState > 1)
	{
		m_fOnTarget = TRUE;
	}

	if (iId < 1)
	{
		m_pWeapon = NULL;
		SetCrosshair(0, nullrc, 0, 0, 0);
		return 0;
	}

	if (g_iUser1 != OBS_IN_EYE)
	{
		// Is player dead???
		if ((iId == -1) && (iClip == -1))
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
			return 1;
		}
		gHUD.m_fPlayerDead = FALSE;
	}

	WEAPON *pWeapon = gWR.GetWeapon(iId);

	if (!pWeapon)
		return 0;

	if (iClip < -1)
		pWeapon->iClip = abs(iClip);
	else
		pWeapon->iClip = iClip;

	if (iState == 0) // we're not the current weapon, so update no more
		return 1;

	m_pWeapon = pWeapon;

	UpdateCrosshair();

	m_fFade = FADE_TIME;
	m_iFlags |= HUD_ACTIVE;

	return 1;
}

void CHudAmmo::UpdateCrosshair()
{
	static wrect_t nullrc;
	if (m_pWeapon == NULL)
	{
		SetCrosshair(0, nullrc, 0, 0, 0);
		return;
	}
	if (gHUD.m_iFOV >= 90)
	{ // normal crosshairs
		if (m_fOnTarget && m_pWeapon->hAutoaim)
			SetCrosshair(m_pWeapon->hAutoaim, m_pWeapon->rcAutoaim, 255, 255, 255);
		else
		{
			if (!CHudCrosshair::Get()->IsEnabled())
				SetCrosshair(m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255);
			else // Disable crosshair because custom one is enabled
				SetCrosshair(0, nullrc, 0, 0, 0);
		}
	}
	else
	{ // zoomed crosshairs
		int crossZoom = cl_cross_zoom.GetInt();
		if (CHudCrosshair::Get()->IsEnabled() && crossZoom == 1)
		{
			// Disable crosshair because custom one is enabled
			SetCrosshair(0, nullrc, 0, 0, 0);
		}
		else
		{
			if (m_fOnTarget && m_pWeapon->hZoomedAutoaim)
				SetCrosshair(m_pWeapon->hZoomedAutoaim, m_pWeapon->rcZoomedAutoaim, 255, 255, 255);
			else
				SetCrosshair(m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255);
		}
	}
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	WEAPON Weapon;

	strcpy(Weapon.szName, READ_STRING());
	Weapon.iAmmoType = (int)READ_CHAR();

	Weapon.iMax1 = READ_BYTE();
	if (Weapon.iMax1 == 255)
		Weapon.iMax1 = -1;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2 = READ_BYTE();
	if (Weapon.iMax2 == 255)
		Weapon.iMax2 = -1;

	Weapon.iSlot = READ_CHAR();
	Weapon.iSlotPos = READ_CHAR();
	Weapon.iId = READ_CHAR();
	Weapon.iFlags = READ_BYTE();
	Weapon.iClip = 0;

	gWR.AddWeapon(&Weapon);

	if (Weapon.iSlot > m_iMaxSlot && Weapon.iSlot < MAX_WEAPON_SLOTS)
		m_iMaxSlot = Weapon.iSlot;

	return 1;
}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// Slot button pressed
void CHudAmmo::SlotInput(int iSlot)
{
	// Let the Viewport use it first, for menus
	if (g_pViewport && g_pViewport->SlotInput(iSlot))
		return;

	gWR.SelectSlot(iSlot, FALSE, 1);
}

void CHudAmmo::UserCmd_Slot1(void)
{
	SlotInput(0);
}

void CHudAmmo::UserCmd_Slot2(void)
{
	SlotInput(1);
}

void CHudAmmo::UserCmd_Slot3(void)
{
	SlotInput(2);
}

void CHudAmmo::UserCmd_Slot4(void)
{
	SlotInput(3);
}

void CHudAmmo::UserCmd_Slot5(void)
{
	SlotInput(4);
}

void CHudAmmo::UserCmd_Slot6(void)
{
	SlotInput(5);
}

void CHudAmmo::UserCmd_Slot7(void)
{
	SlotInput(6);
}

void CHudAmmo::UserCmd_Slot8(void)
{
	SlotInput(7);
}

void CHudAmmo::UserCmd_Slot9(void)
{
	SlotInput(8);
}

void CHudAmmo::UserCmd_Slot10(void)
{
	SlotInput(9);
}

void CHudAmmo::UserCmd_Close(void)
{
	if (gpActiveSel)
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		PlaySound("common/wpn_hudoff.wav", 1);
	}
	else
		gEngfuncs.pfnClientCmd("escape");
}

// Selects the next item in the weapon menu
void CHudAmmo::UserCmd_NextWeapon(void)
{
	if (gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)))
		return;

	if (!gpActiveSel || gpActiveSel == (WEAPON *)1)
		gpActiveSel = m_pWeapon;

	int pos = 0;
	int slot = 0;
	if (gpActiveSel)
	{
		pos = gpActiveSel->iSlotPos + 1;
		slot = gpActiveSel->iSlot;
	}

	for (int loop = 0; loop <= 1; loop++)
	{
		for (; slot < MAX_WEAPON_SLOTS; slot++)
		{
			for (; pos < MAX_WEAPON_POSITIONS; pos++)
			{
				WEAPON *wsp = gWR.GetWeaponSlot(slot, pos);

				if (wsp && gWR.HasAmmo(wsp))
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = 0;
		}

		slot = 0; // start looking from the first slot again
	}

	gpActiveSel = NULL;
}

// Selects the previous item in the menu
void CHudAmmo::UserCmd_PrevWeapon(void)
{
	if (gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)))
		return;

	if (!gpActiveSel || gpActiveSel == (WEAPON *)1)
		gpActiveSel = m_pWeapon;

	int pos = MAX_WEAPON_POSITIONS - 1;
	int slot = MAX_WEAPON_SLOTS - 1;
	if (gpActiveSel)
	{
		pos = gpActiveSel->iSlotPos - 1;
		slot = gpActiveSel->iSlot;
	}

	for (int loop = 0; loop <= 1; loop++)
	{
		for (; slot >= 0; slot--)
		{
			for (; pos >= 0; pos--)
			{
				WEAPON *wsp = gWR.GetWeaponSlot(slot, pos);

				if (wsp && gWR.HasAmmo(wsp))
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = MAX_WEAPON_POSITIONS - 1;
		}

		slot = MAX_WEAPON_SLOTS - 1;
	}

	gpActiveSel = NULL;
}

//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------

void CHudAmmo::Draw(float flTime)
{
	int x, y, r, g, b;
	float a;
	int AmmoWidth;

	if (!(gHUD.m_iWeaponBits & (1 << (WEAPON_SUIT))))
		return;

	if ((gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)))
		return;

	// Draw Weapon Menu
	DrawWList(flTime);

	// Draw ammo pickup history
	gHR.DrawAmmoHistory(flTime);

	if (!(m_iFlags & HUD_ACTIVE))
		return;

	if (!m_pWeapon)
		return;

	WEAPON *pw = m_pWeapon; // shorthand

	AmmoWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;

	if (!hud_dim.GetBool())
		a = MIN_ALPHA + ALPHA_AMMO_MAX;
	else if (m_fFade > 0)
	{
		// Fade the ammo number back to dim
		m_fFade -= (gHUD.m_flTimeDelta * 20);
		if (m_fFade <= 0)
			m_fFade = 0;
		a = MIN_ALPHA + (m_fFade / FADE_TIME) * ALPHA_AMMO_FLASH;
	}
	else
		a = MIN_ALPHA;

	float alphaDim = a;

	a *= gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);

	// Draw weapon sprite
	if (hud_weapon.GetBool())
	{
		y = ScreenHeight - (m_pWeapon->rcInactive.bottom - m_pWeapon->rcInactive.top);
		x = ScreenWidth - (8.5 * AmmoWidth) - (m_pWeapon->rcInactive.right - m_pWeapon->rcInactive.left);
		SPR_Set(m_pWeapon->hInactive, r, g, b);
		SPR_DrawAdditive(0, x, y, &m_pWeapon->rcInactive);
	}

	// SPR_Draw Ammo
	if ((pw->iAmmoType < 0) && (pw->iAmmo2Type < 0))
		return;

	int iFlags = DHN_DRAWZERO; // draw 0 values

	// Does this weapon have a clip?
	y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;

	// Does weapon have any ammo at all?
	if (m_pWeapon->iAmmoType > 0)
	{
		int iIconWidth = m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left;

		if (pw->iClip >= 0)
		{
			a = alphaDim * gHUD.GetHudTransparency();
			gHUD.GetHudAmmoColor(pw->iClip, GetMaxClip(pw->szName), r, g, b);
			ScaleColors(r, g, b, a);

			// room for the number and the '|' and the current ammo
			x = ScreenWidth - (8 * AmmoWidth) - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b);

			wrect_t rc;
			rc.top = 0;
			rc.left = 0;
			rc.right = AmmoWidth;
			rc.bottom = 100;

			int iBarWidth = AmmoWidth / 10;

			x += AmmoWidth / 2;

			// draw the | bar
			FillRGBA(x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a);

			x += iBarWidth + AmmoWidth / 2;
			;

			// GL Seems to need this
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}
		else
		{
			a = alphaDim * gHUD.GetHudTransparency();
			gHUD.GetHudAmmoColor(gWR.CountAmmo(pw->iAmmoType), pw->iMax1, r, g, b);
			ScaleColors(r, g, b, a);

			// SPR_Draw a bullets only line
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}

		// Draw the ammo Icon
		int iOffset = (m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top) / 8;
		SPR_Set(m_pWeapon->hAmmo, r, g, b);
		SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo);
	}

	// Does weapon have seconday ammo?
	if (pw->iAmmo2Type > 0)
	{
		int iIconWidth = m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left;

		// Do we have secondary ammo?
		if ((pw->iAmmo2Type != 0) && (gWR.CountAmmo(pw->iAmmo2Type) > 0))
		{
			a = alphaDim * gHUD.GetHudTransparency();
			gHUD.GetHudAmmoColor(pw->iClip, GetMaxClip(pw->szName), r, g, b);
			ScaleColors(r, g, b, a);

			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight / 4;
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmo2Type), r, g, b);

			// Draw the ammo Icon
			SPR_Set(m_pWeapon->hAmmo2, r, g, b);
			int iOffset = (m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top) / 8;
			SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo2);
		}
	}
}

//
// Draws the ammo bar on the hud
//
int DrawBar(int x, int y, int width, int height, float f)
{
	int r, g, b;
	float a;

	if (f < 0)
		f = 0;
	if (f > 1)
		f = 1;

	if (f)
	{
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if (w <= 0)
			w = 1;
		a = 255 * gHUD.GetHudTransparency();
		UnpackRGB(r, g, b, RGB_GREENISH);
		FillRGBA(x, y, w, height, r, g, b, a);
		x += w;
		width -= w;
	}

	a = 128 * gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	FillRGBA(x, y, width, height, r, g, b, a);

	return (x + width);
}

void DrawAmmoBar(WEAPON *p, int x, int y, int width, int height)
{
	if (!p)
		return;

	if (p->iAmmoType != -1)
	{
		if (!gWR.CountAmmo(p->iAmmoType))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType) / (float)p->iMax1;

		x = DrawBar(x, y, width, height, f);

		// Do we have secondary ammo too?

		if (p->iAmmo2Type != -1)
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type) / (float)p->iMax2;

			x += 5; //!!!

			DrawBar(x, y, width, height, f);
		}
	}
}

//
// Draw Weapon Menu
//
int CHudAmmo::DrawWList(float flTime)
{
	int r, g, b, x, y, i;
	float a;

	if (!gpActiveSel)
		return 0;

	int iActiveSlot;

	if (gpActiveSel == (WEAPON *)1)
		iActiveSlot = -1; // current slot has no weapons
	else
		iActiveSlot = gpActiveSel->iSlot;

	x = 10; //!!!
	y = 10; //!!!

	// Ensure that there are available choices in the active slot
	if (iActiveSlot > 0)
	{
		if (!gWR.GetFirstPos(iActiveSlot))
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}

	// Draw top line
	for (i = 0; i <= m_iMaxSlot; i++)
	{
		int iWidth;

		if (iActiveSlot == i)
			a = 255;
		else
			a = 192;

		a *= gHUD.GetHudTransparency();
		gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
		ScaleColors(r, g, b, a);

		SPR_Set(gHUD.GetSprite(m_HUD_bucket0 + i), r, g, b);

		// make active slot wide enough to accomodate gun pictures
		if (i == iActiveSlot)
		{
			WEAPON *p = gWR.GetFirstPos(iActiveSlot);
			if (p)
				iWidth = p->rcActive.right - p->rcActive.left;
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_bucket0 + i));

		x += iWidth + 5;
	}

	x = 10;

	// Draw all of the buckets
	for (i = 0; i <= m_iMaxSlot; i++)
	{
		y = giBucketHeight + 10;

		// If this is the active slot, draw the bigger pictures,
		// otherwise just draw boxes
		if (i == iActiveSlot)
		{
			WEAPON *p = gWR.GetFirstPos(i);
			int iWidth = giBucketWidth;
			if (p)
				iWidth = p->rcActive.right - p->rcActive.left;

			for (int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++)
			{
				p = gWR.GetWeaponSlot(i, iPos);

				if (!p || !p->iId)
					continue;

				gHUD.GetHudColor(HudPart::Common, 0, r, g, b);

				// if active, then we must have ammo.

				if (gpActiveSel == p)
				{
					a = 255 * gHUD.GetHudTransparency();
					ScaleColors(r, g, b, a);

					SPR_Set(p->hActive, r, g, b);
					SPR_DrawAdditive(0, x, y, &p->rcActive);

					SPR_Set(gHUD.GetSprite(m_HUD_selection), r, g, b);
					SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_selection));
				}
				else
				{
					// Draw Weapon if Red if no ammo

					if (gWR.HasAmmo(p))
					{
						a = 192 * gHUD.GetHudTransparency();
						ScaleColors(r, g, b, a);
					}
					else
					{
						a = 128 * gHUD.GetHudTransparency();
						UnpackRGB(r, g, b, RGB_REDISH);
						ScaleColors(r, g, b, a);
					}

					SPR_Set(p->hInactive, r, g, b);
					SPR_DrawAdditive(0, x, y, &p->rcInactive);
				}

				// Draw Ammo Bar

				DrawAmmoBar(p, x + giABWidth / 2, y, giABWidth, giABHeight);

				y += p->rcActive.bottom - p->rcActive.top + 5;
			}

			x += iWidth + 5;
		}
		else
		{
			// Draw Row of weapons.

			for (int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++)
			{
				WEAPON *p = gWR.GetWeaponSlot(i, iPos);

				if (!p || !p->iId)
					continue;

				if (gWR.HasAmmo(p))
				{
					a = 128 * gHUD.GetHudTransparency();
					gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
				}
				else
				{
					a = 96 * gHUD.GetHudTransparency();
					UnpackRGB(r, g, b, RGB_REDISH);
				}

				FillRGBA(x, y, giBucketWidth, giBucketHeight, r, g, b, a);

				y += giBucketHeight + 5;
			}

			x += giBucketWidth + 5;
		}
	}

	return 1;
}

// m_iMaxClip is never sended to the client, until i figure out another way, this is the best thing we can use
int CHudAmmo::GetMaxClip(char *weaponname)
{
	if (!strcmp(weaponname, "weapon_9mmhandgun"))
	{
		return 17;
	}
	else if (!strcmp(weaponname, "weapon_9mmAR"))
	{
		return 50;
	}
	else if (!strcmp(weaponname, "weapon_shotgun"))
	{
		return 8;
	}
	else if (!strcmp(weaponname, "weapon_crossbow"))
	{
		return 5;
	}
	else if (!strcmp(weaponname, "weapon_rpg"))
	{
		return 1;
	}
	else if (!strcmp(weaponname, "weapon_357"))
	{
		return 6;
	}
	else // if you are using custom weapons, then custom colors for ammo hud aren't going to be used..
	{
		return -1;
	}
}

/* =================================
	GetSpriteFromList

Finds and returns the sprite which name starts
with 'pszNameStart' and resolution 'iRes'
in the given sprite list 'pList'
iCount is the number of items in the pList
================================= */
client_sprite_t *GetSpriteFromList(client_sprite_t *pList, const char *pszNameStart, int iRes, int iCount)
{
	if (!pList || iCount <= 0)
		return NULL;

	int len = strlen(pszNameStart);
	client_sprite_t *p = pList;
	while (iCount--)
	{
		if (p->iRes == iRes && !strncmp(pszNameStart, p->szName, len))
			return p;
		p++;
	}

	return NULL;
}
