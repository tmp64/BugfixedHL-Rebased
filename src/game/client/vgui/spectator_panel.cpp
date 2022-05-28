#include <vgui/IPanel.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "hud.h"
#include "hud/spectator.h"
#include "cl_util.h"
#include "client_steam_context.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "avatar_image.h"
#include "spectator_panel.h"

class CSpectatorInfoPanel : public vgui2::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE(CSpectatorInfoPanel, vgui2::EditablePanel);

	CSpectatorInfoPanel(CSpectatorPanel *pParent)
	    : BaseClass(pParent, "SpectatorInfoPanel")
	{
		m_pParent = pParent;

		m_pAvatar = new vgui2::ImagePanel(this, "AvatarImg");
		m_pNameLabel = new vgui2::Label(this, "NameLabel", "PlayerName");
		m_pKillsLabel = new vgui2::Label(this, "KillsLabel", "K: 32");
		m_pDeathsLabel = new vgui2::Label(this, "DeathsLabel", "D: 50");

		LoadControlSettings(VGUI2_ROOT_DIR "resource/SpectatorInfoPanel.res");
	}

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetPaintBackgroundType(0);
		SetPaintBackgroundEnabled(true);
		SetBgColor(pScheme->GetColor("Frame.BgColor", Color(0, 0, 0, 64)));
	}

	virtual void PaintBackground() override
	{
		// Main BG
		vgui2::surface()->DrawSetColor(GetBgColor());
		vgui2::surface()->DrawFilledRect(0, m_iBg1Margin, GetWide(), m_iBg1Margin + m_iBg1Tall);
		vgui2::surface()->DrawFilledRect(0, m_iBg2Margin, GetWide(), m_iBg2Margin + m_iBg2Tall);

		// Avatar BG
		int x, y, wide, tall;
		m_pAvatar->GetBounds(x, y, wide, tall);
		vgui2::surface()->DrawSetColor(m_DefaultAvatarBorderColor);
		vgui2::surface()->DrawFilledRect(x - m_iAvatarBorder, y - m_iAvatarBorder,
		    x + wide + m_iAvatarBorder, y + tall + m_iAvatarBorder);
	}

	void SetPlayer(int iPlayerIdx)
	{
		Assert(iPlayerIdx != 0);
		CSpectatorPanel::PlayerData &data = m_pParent->m_PlayerData[iPlayerIdx];
		if (!data.pi)
			data.pi = GetPlayerInfo(iPlayerIdx);
		Assert(data.pi->IsConnected());

		if (!data.pInfoAvatar)
		{
			data.pInfoAvatar = new CAvatarImage();
			data.pInfoAvatar->SetDrawFriend(false);
			data.pInfoAvatar->SetSize(m_pAvatar->GetWide(), m_pAvatar->GetTall());
		}

		uint64_t steamId = data.pi->GetValidSteamID64();
		if (ClientSteamContext().SteamUtils() && steamId != 0)
		{
			CSteamID steamIDForPlayer(steamId);
			data.pInfoAvatar->SetAvatarSteamID(steamIDForPlayer, k_EAvatarSize64x64);
		}
		else
		{
			data.pInfoAvatar->ClearAvatarSteamID();
		}

		m_pAvatar->SetImage(data.pInfoAvatar);

		char buf[512];
		m_pNameLabel->SetColorCodedText(data.pi->GetDisplayName());
		m_pNameLabel->SetFgColor(g_pViewport->GetTeamColor(data.pi->GetTeamNumber()));

		snprintf(buf, sizeof(buf), "K: %d", data.pi->GetFrags());
		m_pKillsLabel->SetText(buf);

		snprintf(buf, sizeof(buf), "D: %d", data.pi->GetDeaths());
		m_pDeathsLabel->SetText(buf);
	}

private:
	CSpectatorPanel *m_pParent = nullptr;
	vgui2::ImagePanel *m_pAvatar = nullptr;
	vgui2::Label *m_pNameLabel = nullptr;
	vgui2::Label *m_pKillsLabel = nullptr;
	vgui2::Label *m_pDeathsLabel = nullptr;

	CPanelAnimationVar(Color, m_DefaultAvatarBorderColor, "avatar_border_color", "44 47 66 255");
	CPanelAnimationVarAliasType(int, m_iAvatarBorder, "avatar_border", "1", "proportional_int");

	CPanelAnimationVarAliasType(int, m_iBg1Margin, "bg1_margin", "18", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBg1Tall, "bg1_tall", "30", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBg2Margin, "bg2_margin", "18", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iBg2Tall, "bg2_tall", "16", "proportional_int");
};

CSpectatorPanel::CSpectatorPanel()
    : BaseClass(nullptr, VIEWPORT_PANEL_SPECTATOR)
{
	SetSize(100, 100); // Silence "parent not sized yet" warning
	SetProportional(true);

	m_pInfoPanel = new CSpectatorInfoPanel(this);
	m_pInfoPanel->SetVisible(false);

	SetVisible(false);
}

void CSpectatorPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(true);
}

void CSpectatorPanel::PaintBackground()
{
	// Paint inset border
	if (m_Inset.bDraw)
	{
		vgui2::surface()->DrawSetColor(m_InsetColor);
		vgui2::surface()->DrawFilledRect(m_Inset.iX, m_Inset.iY,
		    m_Inset.iX + m_Inset.iWide, m_Inset.iY + 1);
		vgui2::surface()->DrawFilledRect(m_Inset.iX + m_Inset.iWide, m_Inset.iY,
		    m_Inset.iX + m_Inset.iWide + 1, m_Inset.iY + m_Inset.iTall);
		vgui2::surface()->DrawFilledRect(m_Inset.iX, m_Inset.iY + m_Inset.iTall,
		    m_Inset.iX + m_Inset.iWide + 1, m_Inset.iY + m_Inset.iTall + 1);
		vgui2::surface()->DrawFilledRect(m_Inset.iX, m_Inset.iY + 1,
		    m_Inset.iX + 1, m_Inset.iY + m_Inset.iTall + 1);
	}
}

void CSpectatorPanel::UpdateSpectatingPlayer(int iPlayerIdx)
{
	if (iPlayerIdx != 0)
	{
		if (!m_pInfoPanel->IsVisible())
			m_pInfoPanel->SetVisible(true);
		m_pInfoPanel->SetPlayer(iPlayerIdx);
	}
	else
	{
		if (m_pInfoPanel->IsVisible())
			m_pInfoPanel->SetVisible(false);
	}
}

void CSpectatorPanel::EnableInsetView(bool bIsEnabled)
{
	int x = CHudSpectator::Get()->m_OverviewData.insetWindowX;
	int y = CHudSpectator::Get()->m_OverviewData.insetWindowY;
	int wide = CHudSpectator::Get()->m_OverviewData.insetWindowWidth;
	int tall = CHudSpectator::Get()->m_OverviewData.insetWindowHeight;

	if (bIsEnabled)
	{
		m_Inset.bDraw = true;
		m_Inset.iX = XRES(x) - 1;
		m_Inset.iY = YRES(y) - 1;
		m_Inset.iWide = XRES(wide) + 1;
		m_Inset.iTall = YRES(tall) + 1;
	}
	else
	{
		m_Inset.bDraw = false;
	}
}

const char *CSpectatorPanel::GetName()
{
	return VIEWPORT_PANEL_SPECTATOR;
}

void CSpectatorPanel::Reset()
{
}

void CSpectatorPanel::ShowPanel(bool state)
{
	if (state != IsVisible())
	{
		SetVisible(state);

		if (state)
		{
			FillViewport();
		}
	}
}

vgui2::VPANEL CSpectatorPanel::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CSpectatorPanel::IsVisible()
{
	return BaseClass::IsVisible();
}

void CSpectatorPanel::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}

void CSpectatorPanel::FillViewport()
{
	int wide, tall, vpwide, vptall;
	GetSize(wide, tall);
	vgui2::ipanel()->GetSize(GetVParent(), vpwide, vptall);

	if (wide != vpwide || tall != vptall)
	{
		SetSize(vpwide, vptall);
		SetPos(0, 0);
		InvalidateLayout();
	}
}
