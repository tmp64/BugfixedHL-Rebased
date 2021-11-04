//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/MessageMap.h>
#include <vgui/ISurface.h>
#include "avatar_image.h"
#include "tga_image.h"
#include "client_steam_context.h"
#include "client_vgui.h"
#include "KeyValues.h"
#include "hud.h"
#include "cl_util.h"

using namespace vgui2;

DECLARE_BUILD_FACTORY(CAvatarImagePanel);

CUtlMap<AvatarImagePair_t, int> CAvatarImage::s_AvatarImageCache; // cache of steam id's to textureids to use for images
bool CAvatarImage::m_sbInitializedAvatarCache = false;

static CTGAImage *s_DefaultAvatarImage = nullptr;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CAvatarImage::CAvatarImage(void)
#ifndef NO_STEAM
    : m_sPersonaStateChangedCallback(this, &CAvatarImage::OnPersonaStateChanged)
#endif
{
	ClearAvatarSteamID();
	m_nX = 0;
	m_nY = 0;
	m_wide = m_tall = 0;
	m_avatarWide = m_avatarTall = 0;
	m_Color = Color(255, 255, 255, 255);
	m_bLoadPending = false;
	m_fNextLoadTime = 0.0f;
	m_AvatarSize = k_EAvatarSize32x32;

	//=============================================================================
	// HPE_BEGIN:
	//=============================================================================
	// [tj] Default to drawing the friend icon for avatars
	m_bDrawFriend = true;

	// [menglish] Default icon for avatar icons if there is no avatar icon for the player
	m_iTextureID = -1;

	if (!s_DefaultAvatarImage)
	{
		s_DefaultAvatarImage = new CTGAImage(VGUI2_ROOT_DIR "gfx/default_avatar");
	}

	m_pDefaultImage = s_DefaultAvatarImage;

	SetAvatarSize(DEFAULT_AVATAR_SIZE, DEFAULT_AVATAR_SIZE);

	//=============================================================================
	// HPE_END
	//=============================================================================

	if (!m_sbInitializedAvatarCache)
	{
		m_sbInitializedAvatarCache = true;
		SetDefLessFunc(s_AvatarImageCache);
	}
}

//-----------------------------------------------------------------------------
// Purpose: reset the image to a default state (will render with the default image)
//-----------------------------------------------------------------------------
void CAvatarImage::ClearAvatarSteamID(void)
{
	m_bValid = false;
	m_bFriend = false;
	m_bLoadPending = false;
	m_SteamID.Set(0, k_EUniverseInvalid, k_EAccountTypeInvalid);
#ifndef NO_STEAM
	m_sPersonaStateChangedCallback.Unregister();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Set the CSteamID for this image; this will cause a deferred load
//-----------------------------------------------------------------------------
bool CAvatarImage::SetAvatarSteamID(CSteamID steamIDUser, EAvatarSize avatarSize /*= k_EAvatarSize32x32 */)
{
	ClearAvatarSteamID();

	m_SteamID = steamIDUser;
	m_AvatarSize = avatarSize;
	m_bLoadPending = true;
#ifndef NO_STEAM
	m_sPersonaStateChangedCallback.Register(this, &CAvatarImage::OnPersonaStateChanged);
#endif

	LoadAvatarImage();
	UpdateFriendStatus();

	return m_bValid;
}

//-----------------------------------------------------------------------------
// Purpose: Called when somebody changes their avatar image
//-----------------------------------------------------------------------------
void CAvatarImage::OnPersonaStateChanged(PersonaStateChange_t *info)
{
	if ((info->m_ulSteamID == m_SteamID.ConvertToUint64()) && (info->m_nChangeFlags & k_EPersonaChangeAvatar))
	{
		// Mark us as invalid.
		m_bValid = false;
		m_bLoadPending = true;

		// Poll
		LoadAvatarImage();
	}
}

//-----------------------------------------------------------------------------
// Purpose: load the avatar image if we have a load pending
//-----------------------------------------------------------------------------
void CAvatarImage::LoadAvatarImage()
{
#ifdef CSS_PERF_TEST
	return;
#endif
	// attempt to retrieve the avatar image from Steam
	if (m_bLoadPending && ClientSteamContext().SteamFriends() && ClientSteamContext().SteamUtils() && gEngfuncs.GetClientTime() >= m_fNextLoadTime)
	{
		if (!ClientSteamContext().SteamFriends()->RequestUserInformation(m_SteamID, false))
		{
			int iAvatar = 0;
			switch (m_AvatarSize)
			{
			case k_EAvatarSize32x32:
				iAvatar = ClientSteamContext().SteamFriends()->GetSmallFriendAvatar(m_SteamID);
				break;
			case k_EAvatarSize64x64:
				iAvatar = ClientSteamContext().SteamFriends()->GetMediumFriendAvatar(m_SteamID);
				break;
			case k_EAvatarSize184x184:
				iAvatar = ClientSteamContext().SteamFriends()->GetLargeFriendAvatar(m_SteamID);
				break;
			}

			//Msg( "Got avatar %d for SteamID %llud (%s)\n", iAvatar, m_SteamID.ConvertToUint64(), ClientSteamContext().SteamFriends()->GetFriendPersonaName( m_SteamID ) );

			if (iAvatar > 0) // if its zero, user doesn't have an avatar.  If -1, Steam is telling us that it's fetching it
			{
				uint32 wide = 0, tall = 0;
				if (ClientSteamContext().SteamUtils()->GetImageSize(iAvatar, &wide, &tall) && wide > 0 && tall > 0)
				{
					int destBufferSize = wide * tall * 4;
					byte *rgbDest = (byte *)stackalloc(destBufferSize);
					if (ClientSteamContext().SteamUtils()->GetImageRGBA(iAvatar, rgbDest, destBufferSize))
						InitFromRGBA(iAvatar, rgbDest, wide, tall);

					stackfree(rgbDest);
				}
			}
		}

		if (m_bValid)
		{
			// if we have a valid image, don't attempt to load it again
			m_bLoadPending = false;
		}
		else
		{
			// otherwise schedule another attempt to retrieve the image
			m_fNextLoadTime = gEngfuncs.GetClientTime() + 1.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Query Steam to set the m_bFriend status flag
//-----------------------------------------------------------------------------
void CAvatarImage::UpdateFriendStatus(void)
{
	if (!m_SteamID.IsValid())
		return;

	if (ClientSteamContext().SteamFriends() && ClientSteamContext().SteamUtils())
		m_bFriend = ClientSteamContext().SteamFriends()->HasFriend(m_SteamID, k_EFriendFlagImmediate);
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the surface with the supplied raw RGBA image data
//-----------------------------------------------------------------------------
void CAvatarImage::InitFromRGBA(int iAvatar, const byte *rgba, int width, int height)
{
	int iTexIndex = s_AvatarImageCache.Find(AvatarImagePair_t(m_SteamID, iAvatar));
	if (iTexIndex == s_AvatarImageCache.InvalidIndex())
	{
		m_iTextureID = vgui2::surface()->CreateNewTextureID(true);
		vgui2::surface()->DrawSetTextureRGBA(m_iTextureID, rgba, width, height, true, false);
		iTexIndex = s_AvatarImageCache.Insert(AvatarImagePair_t(m_SteamID, iAvatar));
		s_AvatarImageCache[iTexIndex] = m_iTextureID;
	}
	else
		m_iTextureID = s_AvatarImageCache[iTexIndex];

	m_bValid = true;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the image and optional friend icon
//-----------------------------------------------------------------------------
void CAvatarImage::Paint(void)
{
	int posX = m_nX + m_offX;
	int posY = m_nY + m_offY;

	if (m_bDrawFriend)
	{
		posX += FRIEND_ICON_AVATAR_INDENT_X * m_avatarWide / DEFAULT_AVATAR_SIZE;
		posY += FRIEND_ICON_AVATAR_INDENT_Y * m_avatarTall / DEFAULT_AVATAR_SIZE;
	}

	if (m_bLoadPending)
	{
		LoadAvatarImage();
	}

	if (m_bValid)
	{
		vgui2::surface()->DrawSetTexture(m_iTextureID);
		vgui2::surface()->DrawSetColor(m_Color);
		vgui2::surface()->DrawTexturedRect(posX, posY, posX + m_avatarWide, posY + m_avatarTall);
	}
	else if (m_pDefaultImage)
	{
		// draw default
		m_pDefaultImage->SetSize(m_avatarWide, m_avatarTall);
		m_pDefaultImage->SetPos(posX, posY);
		m_pDefaultImage->SetColor(m_Color);
		m_pDefaultImage->Paint();
	}

	if (m_SecondImage)
	{
		m_SecondImage->Paint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the avatar size; scale the total image and friend icon to fit
//-----------------------------------------------------------------------------
void CAvatarImage::SetAvatarSize(int wide, int tall)
{
	m_avatarWide = wide;
	m_avatarTall = tall;

	if (m_bDrawFriend)
	{
		// scale the size of the friend background frame icon
		m_wide = FRIEND_ICON_SIZE_X * m_avatarWide / DEFAULT_AVATAR_SIZE;
		m_tall = FRIEND_ICON_SIZE_Y * m_avatarTall / DEFAULT_AVATAR_SIZE;
	}
	else
	{
		m_wide = m_avatarWide;
		m_tall = m_avatarTall;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the total image size; scale the avatar portion to fit
//-----------------------------------------------------------------------------
void CAvatarImage::SetSize(int wide, int tall)
{
	m_wide = wide;
	m_tall = tall;

	if (m_bDrawFriend)
	{
		// scale the size of the avatar portion based on the total image size
		m_avatarWide = DEFAULT_AVATAR_SIZE * m_wide / FRIEND_ICON_SIZE_X;
		m_avatarTall = DEFAULT_AVATAR_SIZE * m_tall / FRIEND_ICON_SIZE_Y;
	}
	else
	{
		m_avatarWide = m_wide;
		m_avatarTall = m_tall;
	}
}

bool CAvatarImage::Evict()
{
	return false;
}

int CAvatarImage::GetNumFrames()
{
	return 0;
}

void CAvatarImage::SetFrame(int nFrame)
{
}

vgui2::HTexture CAvatarImage::GetID()
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CAvatarImagePanel::CAvatarImagePanel(vgui2::Panel *parent, const char *name)
    : BaseClass(parent, name)
{
	m_bScaleImage = false;
	m_pImage = new CAvatarImage();
	m_bSizeDirty = true;
	m_bClickable = false;
}

//-----------------------------------------------------------------------------
// Purpose: Set the avatar by entity number
//-----------------------------------------------------------------------------
void CAvatarImagePanel::SetPlayer(int entindex, EAvatarSize avatarSize)
{
	if (!entindex)
		m_pImage->ClearAvatarSteamID();

	uint64 steamID64 = GetPlayerInfo(entindex)->Update()->GetValidSteamID64();

	if (steamID64 && ClientSteamContext().SteamUtils())
	{
		CSteamID steamIDForPlayer = CSteamID(steamID64);
		SetPlayer(steamIDForPlayer, avatarSize);
	}
	else
	{
		m_pImage->ClearAvatarSteamID();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the avatar by SteamID
//-----------------------------------------------------------------------------
void CAvatarImagePanel::SetPlayer(CSteamID steamIDForPlayer, EAvatarSize avatarSize)
{
	m_pImage->ClearAvatarSteamID();

	if (steamIDForPlayer.GetAccountID() != 0)
		m_pImage->SetAvatarSteamID(steamIDForPlayer, avatarSize);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAvatarImagePanel::PaintBackground(void)
{
	if (m_bSizeDirty)
		UpdateSize();

	m_pImage->Paint();
}

void CAvatarImagePanel::ClearAvatar()
{
	m_pImage->ClearAvatarSteamID();
}

void CAvatarImagePanel::SetDefaultAvatar(vgui2::IImage *pDefaultAvatar)
{
	m_pImage->SetDefaultImage(pDefaultAvatar);
}

void CAvatarImagePanel::SetAvatarSize(int width, int height)
{
	if (m_bScaleImage)
	{
		// panel is charge of image size - setting avatar size this way not allowed
		Assert(false);
		return;
	}
	else
	{
		m_pImage->SetAvatarSize(width, height);
		m_bSizeDirty = true;
	}
}

void CAvatarImagePanel::OnSizeChanged(int newWide, int newTall)
{
	BaseClass::OnSizeChanged(newWide, newTall);
	m_bSizeDirty = true;
}

void CAvatarImagePanel::OnMousePressed(vgui2::MouseCode code)
{
	if (!m_bClickable || code != vgui2::MOUSE_LEFT)
		return;

	PostActionSignal(new KeyValues("AvatarMousePressed"));

	// audible feedback
	const char *soundFilename = "ui/buttonclick.wav";

	vgui2::surface()->PlaySound(soundFilename);
}

void CAvatarImagePanel::SetShouldScaleImage(bool bScaleImage)
{
	m_bScaleImage = bScaleImage;
	m_bSizeDirty = true;
}

void CAvatarImagePanel::SetShouldDrawFriendIcon(bool bDrawFriend)
{
	m_pImage->SetDrawFriend(bDrawFriend);
	m_bSizeDirty = true;
}

void CAvatarImagePanel::UpdateSize()
{
	if (m_bScaleImage)
	{
		// the panel is in charge of the image size
		m_pImage->SetAvatarSize(GetWide(), GetTall());
	}
	else
	{
		// the image is in charge of the panel size
		SetSize(m_pImage->GetAvatarWide(), m_pImage->GetAvatarTall());
	}

	m_bSizeDirty = false;
}

void CAvatarImagePanel::ApplySettings(KeyValues *inResourceData)
{
	m_bScaleImage = inResourceData->GetInt("scaleImage", 0);

	BaseClass::ApplySettings(inResourceData);
}
