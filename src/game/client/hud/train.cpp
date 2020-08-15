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
// Train.cpp
//
// implementation of CHudAmmo class
//

#include <string.h>
#include <stdio.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "train.h"

DEFINE_HUD_ELEM(CHudTrain);

void CHudTrain::Init(void)
{
	BaseHudClass::Init();

	HookMessage<&CHudTrain::MsgFunc_Train>("Train");

	m_iPos = 0;
	m_iFlags = 0;
};

void CHudTrain::VidInit(void)
{
	m_hSprite = 0;
};

void CHudTrain::Draw(float fTime)
{
	if (!m_hSprite)
		m_hSprite = LoadSprite("sprites/%d_train.spr");

	if (m_iPos)
	{
		int r, g, b, x, y;
		float a;

		a = 255 * gHUD.GetHudTransparency();

		gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
		ScaleColors(r, g, b, a);
		SPR_Set(m_hSprite, r, g, b);

		// This should show up to the right and part way up the armor number
		y = ScreenHeight - SPR_Height(m_hSprite, 0) - gHUD.m_iFontHeight;
		x = ScreenWidth / 3 + SPR_Width(m_hSprite, 0) / 4;

		SPR_DrawAdditive(m_iPos - 1, x, y, NULL);
	}
}

int CHudTrain::MsgFunc_Train(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// update Train data
	m_iPos = READ_BYTE();

	if (m_iPos)
		m_iFlags |= HUD_ACTIVE;
	else
		m_iFlags &= ~HUD_ACTIVE;

	return 1;
}
