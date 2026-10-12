#include <algorithm>
#include <string>
#include <vgui/IImage.h>
#include <vgui/IInputInternal.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/Menu.h>
#include "hud.h"
#include "cl_util.h"
#include "cl_voice_status.h"
#include "client_vgui.h"
#include "client_steam_context.h"
#include "scoreboard_data.h"
#include "hud/chat.h"
#include "hud/text_message.h"
#include "vgui/client_viewport.h"
#include "vgui/score_panel.h"
#include "vgui/avatar_image.h"

#define STEAM_PROFILE_URL "http://steamcommunity.com/profiles/"

ConVar hud_scoreboard_mousebtn("hud_scoreboard_mousebtn", "1", FCVAR_ARCHIVE);
ConVar hud_scoreboard_showavatars("hud_scoreboard_showavatars", "1", FCVAR_ARCHIVE);
ConVar hud_scoreboard_showloss("hud_scoreboard_showloss", "1", FCVAR_ARCHIVE);
ConVar hud_scoreboard_efftype("hud_scoreboard_efftype", "0", FCVAR_ARCHIVE);
ConVar hud_scoreboard_effpercent("hud_scoreboard_effpercent", "0", FCVAR_ARCHIVE);
ConVar hud_scoreboard_showsteamid("hud_scoreboard_showsteamid", "1", FCVAR_ARCHIVE);
ConVar hud_scoreboard_showeff("hud_scoreboard_showeff", "1", FCVAR_ARCHIVE);
ConVar hud_scoreboard_size("hud_scoreboard_size", "0", FCVAR_ARCHIVE);
ConVar hud_scoreboard_spacing_normal("hud_scoreboard_spacing_normal", "0", FCVAR_ARCHIVE);
ConVar hud_scoreboard_spacing_compact("hud_scoreboard_spacing_compact", "0", FCVAR_ARCHIVE);

namespace
{

int s_iMutedIconTexture = -1;

class CPlayerImage : public vgui2::IImage
{
public:
	void SetAvatar(CAvatarImage *pAvatar)
	{
		m_pAvatar = pAvatar;

		if (m_pAvatar)
		{
			m_pAvatar->SetPos(m_iX, m_iY);
			m_pAvatar->SetOffset(m_iOffX, m_iOffY);
			m_pAvatar->SetSize(m_iWide, m_iTall);
			m_pAvatar->SetColor(m_DrawColor);
		}
	}

	void SetMuted(bool state)
	{
		m_bIsMuted = state;
	}

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint() override
	{
		if (m_pAvatar)
			m_pAvatar->Paint();

		if (m_bIsMuted && s_iMutedIconTexture != -1)
		{
			vgui2::surface()->DrawSetTexture(s_iMutedIconTexture);
			vgui2::surface()->DrawSetColor(m_DrawColor);
			vgui2::surface()->DrawTexturedRect(m_iX + m_iOffX, m_iY + m_iOffY,
			    m_iX + m_iOffX + m_iWide, m_iY + m_iOffY + m_iTall);
		}
	}

	// Set the position of the image
	virtual void SetPos(int x, int y) override
	{
		m_iX = x;
		m_iY = y;
		SetAvatar(m_pAvatar);
	}

	virtual void SetOffset(int x, int y)
	{
		m_iOffX = x;
		m_iOffY = y;
		SetAvatar(m_pAvatar);
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall) override
	{
		wide = m_iWide;
		tall = m_iTall;
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall) override
	{
		GetContentSize(wide, tall);
	}

	// Sets the size of the image
	virtual void SetSize(int wide, int tall) override
	{
		m_iWide = wide;
		m_iTall = tall;
		SetAvatar(m_pAvatar);
	}

	// Set the draw color
	virtual void SetColor(Color col) override
	{
		m_DrawColor = col;
		SetAvatar(m_pAvatar);
	}

private:
	int m_iX = 0, m_iY = 0;
	int m_iOffX = 0, m_iOffY = 0;
	int m_iWide = 0, m_iTall = 0;
	Color m_DrawColor = Color(255, 255, 255, 255);
	CAvatarImage *m_pAvatar = nullptr;
	bool m_bIsMuted = false;
};

}

CScorePanel::CScorePanel()
    : BaseClass(nullptr, VIEWPORT_PANEL_SCORE)
{
	SetTitle("", true);
	SetCloseButtonVisible(false);
	SetMoveable(false);
	SetSizeable(false);
	SetProportional(true);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetScheme("ClientScheme");

	// Header labels
	m_pServerNameLabel = new vgui2::Label(this, "ServerName", "A Half-Life Server");
	m_pMapNameLabel = new vgui2::Label(this, "MapName", "Map: crossfire");
	m_pPlayerCountLabel = new vgui2::Label(this, "PlayerCount", "2/32");
	m_pLeaderTitleLabel = new vgui2::Label(this, "LeaderTitle", "");
	m_pLeaderLineLabel = new vgui2::Label(this, "LeaderLine", "");
	m_pLeaderTitleLabel->SetVisible(false);
	m_pLeaderLineLabel->SetVisible(false);

	// Player list
	m_pPlayerList = new vgui2::SectionedListPanel(this, "PlayerList");
	m_pPlayerList->SetMouseInputEnabled(true);
	m_pPlayerList->SetVerticalScrollbar(false);

	wchar_t *specTag = g_pVGuiLocalize->Find("#BHL_Scores_PlayerSpec");
	if (specTag)
	{
		Q_WStringToUTF8(specTag, m_szSpectatorTag, sizeof(m_szSpectatorTag));
	}

	// Image list
	m_pImageList = new vgui2::ImageList(true);
	m_pPlayerList->SetImageList(m_pImageList, true);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerImage *pImg = new CPlayerImage();
		pImg->SetSize(32, 32);
		m_pImageList->SetImageAtIndex(i, pImg);
	}

	m_iMutedIconTexture = -1;

	CreatePlayerMenu();

	LoadControlSettings(VGUI2_ROOT_DIR "resource/ScorePanel.res");
	SetVisible(false);
}

void CScorePanel::UpdateServerName()
{
	wchar_t wbuf[MAX_SERVERNAME_LENGTH];
	CScoreboardData &data = CScoreboardData::Get();

	// The title of the custom scoreboard replaces the server name
	if (m_bCustom && data.GetTitle()[0])
		CScoreboardData::LocalizeString(data.GetTitle(), wbuf, sizeof(wbuf));
	else
		g_pVGuiLocalize->ConvertANSIToUnicode(g_pViewport->GetServerName(), wbuf, sizeof(wbuf));

	m_pServerNameLabel->SetText(wbuf);

	if (m_bCustom)
		CScoreboardData::LocalizeString(data.GetTag(), m_wszTag, sizeof(m_wszTag));
	else
		m_wszTag[0] = L'\0';
}

void CScorePanel::EnableMousePointer(bool bEnable)
{
	if (bEnable && !IsVisible())
		return;

	SetMouseInputEnabled(bEnable);
	SetKeyBoardInputEnabled(false);

	int x = gEngfuncs.GetWindowCenterX();
	int y = gEngfuncs.GetWindowCenterY();
	vgui2::input()->SetCursorPos(x, y);
}

void CScorePanel::UpdateOnPlayerInfo(int client)
{
	if (!IsVisible())
		return;

	RestoreSize();
	UpdateClientInfo(client);
	UpdateScoresAndCounts();
	Resize();
}

void CScorePanel::DeathMsg(int killer, int victim)
{
	if (!GetThisPlayerInfo())
	{
		// Not yet connected
		return;
	}

	if (victim == GetThisPlayerInfo()->GetIndex())
	{
		// if we were the one killed, set the scoreboard to indicate killer
		m_flKillerHighlightStart = gHUD.m_flTime;
		m_iKillerIndex = killer;
	}
}

void CScorePanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pPlayerList->SetBorder(pScheme->GetBorder("FrameBorder"));
	m_pPlayerList->SetPaintBackgroundType(0);

	m_ThisPlayerBgColor = pScheme->GetColor("ThisPlayerBgColor", Color(0, 0, 0, 0));
	m_KillerBgColor = pScheme->GetColor("KillerBgColor", Color(0, 0, 0, 0));

	s_iMutedIconTexture = m_iMutedIconTexture;
}

void CScorePanel::OnThink()
{
	CScoreboardData &data = CScoreboardData::Get();

	if (data.IsActive() != m_bCustom || (m_bCustom && (data.GetSerial() != m_iStoreSerial || m_bSectionsDirty)))
	{
		m_iStoreSerial = data.GetSerial();

		// Sections and columns can't be changed in place, so the schema or the order of teams rebuilds them
		if (data.IsActive() != m_bCustom || data.GetSchemaSerial() != m_iSchemaSerial)
			FullUpdate();
		else if (m_bSectionsDirty || !std::is_sorted(m_SortedTeamIDs.begin(), m_SortedTeamIDs.begin() + m_iSortedTeamCount, [this](int ilhs, int irhs) { return IsTeamBefore(ilhs, irhs); }))
			RefreshItems();
		else
			UpdateAllClients();
	}

	if (m_iKillerIndex != 0 && m_PlayerData[m_iKillerIndex].nItemID != -1)
	{
		// Update bg color
		UpdateClientInfo(m_iKillerIndex);
	}

	if (m_flLastUpdateTime + UPDATE_PERIOD <= gEngfuncs.GetAbsoluteTime())
	{
		UpdateAllClients();
	}
}

void CScorePanel::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "MenuMute"))
	{
		OnPlayerMenuCommand(MenuAction::Mute);
	}
	else if (!Q_stricmp(command, "MenuSteamProfile"))
	{
		OnPlayerMenuCommand(MenuAction::SteamProfile);
	}
	else if (!Q_stricmp(command, "MenuSteamURL"))
	{
		OnPlayerMenuCommand(MenuAction::SteamURL);
	}
	else if (!Q_stricmp(command, "MenuCopyName"))
	{
		OnPlayerMenuCommand(MenuAction::CopyName);
	}
	else if (!Q_stricmp(command, "MenuCopyNameRaw"))
	{
		OnPlayerMenuCommand(MenuAction::CopyNameRaw);
	}
	else if (!Q_stricmp(command, "MenuCopySteamID"))
	{
		OnPlayerMenuCommand(MenuAction::CopySteamID);
	}
	else if (!Q_stricmp(command, "MenuCopySteamID64"))
	{
		OnPlayerMenuCommand(MenuAction::CopySteamID64);
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CScorePanel::PaintBackground()
{
	BaseClass::PaintBackground();

	if (m_bGunGame && m_pLeaderTitleLabel->IsVisible())
	{
		// Leader banner: a box around both lines with an accent at the left edge
		int x, y, wide, tall, lineX, lineY, lineWide, lineTall;
		m_pLeaderTitleLabel->GetBounds(x, y, wide, tall);
		m_pLeaderLineLabel->GetBounds(lineX, lineY, lineWide, lineTall);

		int x0 = std::min(x, lineX) - m_iGGBannerAccentWide - m_iGGBannerPadding;
		int y0 = y - m_iGGBannerPadding;
		int x1 = std::max(x + wide, lineX + lineWide) + m_iGGBannerPadding;
		int y1 = lineY + lineTall + m_iGGBannerPadding;

		vgui2::surface()->DrawSetColor(m_GGBannerBgColor);
		vgui2::surface()->DrawFilledRect(x0, y0, x1, y1);
		vgui2::surface()->DrawSetColor(m_GGBannerAccentColor);
		vgui2::surface()->DrawFilledRect(x0, y0, x0 + m_iGGBannerAccentWide, y1);
	}
}

void CScorePanel::Paint()
{
	BaseClass::Paint();

	if (m_bCustom && m_wszTag[0] && m_pServerNameLabel->IsVisible())
	{
		int x, y, wide, tall, textWide, textTall;
		m_pServerNameLabel->GetBounds(x, y, wide, tall);
		m_pServerNameLabel->GetContentSize(textWide, textTall);
		DrawScoreBadge(m_CellStyle, x + std::min(textWide, wide) + m_CellStyle.iSpacing, y + tall / 2, m_wszTag);
	}
}

const char *CScorePanel::GetName()
{
	return VIEWPORT_PANEL_SCORE;
}

void CScorePanel::Reset()
{
	if (IsVisible())
	{
		ShowPanel(false);
	}

	m_iKillerIndex = 0;
	m_flKillerHighlightStart = 0;
}

void CScorePanel::ShowPanel(bool state)
{
	if (state == IsVisible())
		return;

	if (state)
	{
		FullUpdate();

		// Make active unless chat is visible
		// (possible if scoreboard is activated by intermission when chat is open)
		if (CHudChat::Get()->GetMessageMode() == MM_NONE)
			Activate();
	}
	else
	{
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		m_pPlayerMenu->SetVisible(false);
	}

	SetVisible(state);
}

vgui2::VPANEL CScorePanel::GetVPanel()
{
	return BaseClass::GetVPanel();
}

bool CScorePanel::IsVisible()
{
	return BaseClass::IsVisible();
}

void CScorePanel::SetParent(vgui2::VPANEL parent)
{
	BaseClass::SetParent(parent);
}

void CScorePanel::OnItemContextMenu(int itemID)
{
	OpenPlayerMenu(itemID);
}

void CScorePanel::FullUpdate()
{
	CScoreboardData &data = CScoreboardData::Get();
	m_bCustom = data.IsActive();
	m_bGunGame = m_bCustom && data.IsGunGame();
	m_iStoreSerial = data.GetSerial();
	m_iSchemaSerial = data.GetSchemaSerial();

	UpdateServerName();
	UpdateMapName();
	RefreshItems();
}

void CScorePanel::UpdateMapName()
{
	char buf[64];
	wchar_t wbuf[64];
	V_FileBase(gEngfuncs.pfnGetLevelName(), buf, sizeof(buf));
	g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
	m_pMapNameLabel->SetText(wbuf);
}

void CScorePanel::RefreshItems()
{
	std::fill(m_TeamData.begin(), m_TeamData.end(), TeamData());
	std::fill(m_PlayerData.begin(), m_PlayerData.end(), PlayerData());
	std::fill(m_IsTeamSectionCreated.begin(), m_IsTeamSectionCreated.end(), false);
	m_bSectionsDirty = false;
	m_pPlayerList->RemoveAll();
	m_pPlayerList->RemoveAllSections();

	BuildCustomColumns();
	RestoreSize();

	// Assign player teams, calculate team scores
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i)->Update();
		PlayerData &pd = m_PlayerData[i];
		pd.bIsConnected = pi->IsConnected();

		if (!pd.bIsConnected)
			continue;

		pd.nItemID = -1;
		pd.nTeamID = GetPlayerTeam(pi);

		TeamData &td = m_TeamData[pd.nTeamID];
		td.iFrags += pi->GetFrags();
		td.iDeaths += pi->GetDeaths();
		td.iPlayerCount++;
	}

	// Override team scores and add them to sorting list
	int iTeamCount = 0;

	for (int i = 0; i <= MAX_TEAMS; i++)
	{
		TeamData &td = m_TeamData[i];

		if (td.iPlayerCount == 0)
			continue;

		CTeamInfo *ti = GetTeamInfo(i);

		if (ti->IsScoreOverriden())
		{
			td.iFrags = ti->GetFrags();
			td.iDeaths = ti->GetDeaths();
		}

		m_SortedTeamIDs[iTeamCount] = i;
		iTeamCount++;
	}

	// Sort teams based on the score
	std::sort(m_SortedTeamIDs.begin(), m_SortedTeamIDs.begin() + iTeamCount, [this](int ilhs, int irhs) {
		return IsTeamBefore(ilhs, irhs);
	});
	m_iSortedTeamCount = iTeamCount;

	// Create header before any other sections
	CreateSection(HEADER_SECTION_ID);

	// Create sections for teams
	if (iTeamCount != 1 || m_SortedTeamIDs[0] != 0)
	{
		for (int i = 0; i < iTeamCount; i++)
		{
			CreateSection(m_SortedTeamIDs[i]);
		}
	}

	// Create last section for spectators
	if (m_TeamData[TEAM_SPECTATOR].iPlayerCount > 0)
	{
		CreateSection(TEAM_SPECTATOR);
	}

	UpdateAllClients();
}

void CScorePanel::CreateSection(int nTeamID)
{
	if (m_IsTeamSectionCreated[nTeamID])
		return;

	m_IsTeamSectionCreated[nTeamID] = true;

	char buf[128];
	TeamData &td = m_TeamData[nTeamID];

	m_pPlayerList->AddSection(nTeamID, "", m_bCustom ? StaticPlayerSortFuncCustom : StaticPlayerSortFuncByFrags);
	m_pPlayerList->SetSectionFgColor(nTeamID, g_pViewport->GetTeamColor(nTeamID));

	if (nTeamID == HEADER_SECTION_ID)
	{
		m_pPlayerList->SetSectionAlwaysVisible(nTeamID);
		m_pPlayerList->SetSectionDividerColor(nTeamID, Color(0, 0, 0, 0));
	}

	if (m_bCustom)
	{
		// Team sections get their texts from UpdateScoresAndCounts
		for (const CustomColumn &col : m_CustomColumns)
		{
			m_pPlayerList->AddColumnToSection(nTeamID, col.szName, nTeamID == HEADER_SECTION_ID ? col.wszTitle : L"",
			    col.iFlags, col.iWidth);
		}

		return;
	}

	// Avatar
	m_pPlayerList->AddColumnToSection(nTeamID, "avatar", "",
	    vgui2::SectionedListPanel::COLUMN_IMAGE | vgui2::SectionedListPanel::COLUMN_CENTER,
	    m_iColumnWidthAvatar);

	// Name
	const char *nameCol;

	if (nTeamID == HEADER_SECTION_ID)
		nameCol = "#PlayerName";
	else
		nameCol = "??? (?/?)";

	m_pPlayerList->AddColumnToSection(nTeamID, "name", nameCol,
	    vgui2::SectionedListPanel::COLUMN_BRIGHT | (gHUD.GetColorCodeAction() == ColorCodeAction::Handle ? vgui2::SectionedListPanel::COLUMN_COLORED : 0),
	    GetNameColumnWidth());

	// SteamID
	if (hud_scoreboard_showsteamid.GetBool())
	{
		m_pPlayerList->AddColumnToSection(nTeamID, "steamid", nTeamID == HEADER_SECTION_ID ? "#BHL_Scores_ColSteamID" : "",
		    vgui2::SectionedListPanel::COLUMN_BRIGHT,
		    m_iColumnWidthSteamID);
	}

	// Efficiency
	if (hud_scoreboard_showeff.GetBool())
	{
		m_pPlayerList->AddColumnToSection(nTeamID, "eff", nTeamID == HEADER_SECTION_ID ? "#BHL_Scores_ColEff" : "???",
		    vgui2::SectionedListPanel::COLUMN_BRIGHT,
		    m_iColumnWidthEff);
	}

	// Frags
	m_pPlayerList->AddColumnToSection(nTeamID, "frags", nTeamID == HEADER_SECTION_ID ? "#PlayerScore" : "???",
	    vgui2::SectionedListPanel::COLUMN_BRIGHT,
	    m_iColumnWidthFrags);

	// Deaths
	m_pPlayerList->AddColumnToSection(nTeamID, "deaths", nTeamID == HEADER_SECTION_ID ? "#PlayerDeath" : "???",
	    vgui2::SectionedListPanel::COLUMN_BRIGHT,
	    m_iColumnWidthDeaths);

	// Ping
	const char *pingLabel;

	if (nTeamID == HEADER_SECTION_ID)
		pingLabel = hud_scoreboard_showloss.GetBool() ? "#BHL_Scores_ColPingLoss" : "#BHL_Scores_ColPing";
	else
		pingLabel = "";

	m_pPlayerList->AddColumnToSection(nTeamID, "ping", pingLabel,
	    vgui2::SectionedListPanel::COLUMN_BRIGHT,
	    m_iColumnWidthPing);
}

void CScorePanel::UpdateAllClients()
{
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		UpdateClientInfo(i);
	}

	UpdateScoresAndCounts();
	Resize();

	m_flLastUpdateTime = gEngfuncs.GetAbsoluteTime();
}

void CScorePanel::UpdateClientInfo(int client)
{
	CPlayerInfo *pi = GetPlayerInfo(client);
	PlayerData &pd = m_PlayerData[client];

	if (pi->IsConnected() && !pd.bIsConnected)
	{
		// Player just connected
		pd.bIsConnected = true;

		if (pd.nItemID != -1 && m_pPlayerList->IsItemIDValid(pd.nItemID))
			m_pPlayerList->RemoveItem(pd.nItemID);

		pd.nItemID = -1;
		pd.nTeamID = pi->GetTeamNumber();
	}
	else if (!pi->IsConnected() && pd.bIsConnected)
	{
		// Player disconnected
		pd.bIsConnected = false;

		m_pPlayerList->RemoveItem(pd.nItemID);
		pd.nItemID = -1;
		pd.nTeamID = 0;
	}

	Assert(pd.bIsConnected == pi->IsConnected());

	// Skip unconnected players
	if (!pi->IsConnected())
		return;

	if (GetPlayerTeam(pi) != pd.nTeamID)
	{
		// Player changed team
		m_pPlayerList->RemoveItem(pd.nItemID);
		pd.nItemID = -1;
		pd.nTeamID = pi->GetTeamNumber();
	}

	if (m_bCustom && !m_IsTeamSectionCreated[pd.nTeamID])
		m_bSectionsDirty = true;

	// Create section for player's team if need to
	CreateSection(pd.nTeamID);

	KeyValues *playerKv = new KeyValues("data");

	{
		char buf[128];
		playerKv->SetInt("client", client);

		if (pi->IsThisPlayer())
			playerKv->SetBool("thisplayer", true);

		// Avatar
		UpdateClientIcon(pi);
		playerKv->SetInt("avatar", client); // Client index == index into m_pImageList

		// Name
		if (pi->IsSpectator())
		{
			snprintf(buf, sizeof(buf), "%s %s%s",
			    pi->GetDisplayName(gHUD.GetColorCodeAction() == ColorCodeAction::Strip),
			    gHUD.GetColorCodeAction() == ColorCodeAction::Handle ? "^0" : "",
			    m_szSpectatorTag);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s", pi->GetDisplayName(gHUD.GetColorCodeAction() == ColorCodeAction::Strip));
		}
		playerKv->SetString("name", buf);

		// SteamID
		playerKv->SetString("steamid", pi->GetSteamID());

		// Efficiency
		float eff = CalculateEfficiency(pi->GetFrags(), pi->GetDeaths());
		if (hud_scoreboard_effpercent.GetBool())
			snprintf(buf, sizeof(buf), "%.0f%%", eff * 100.0);
		else
			snprintf(buf, sizeof(buf), "%.2f", eff);
		playerKv->SetString("eff", buf);

		// Frags & deaths
		playerKv->SetInt("frags", pi->GetFrags());
		playerKv->SetInt("deaths", pi->GetDeaths());

		// Ping
		if (hud_scoreboard_showloss.GetBool())
		{
			snprintf(buf, sizeof(buf), "%d/%d", pi->GetPing(), pi->GetPacketLoss());
			playerKv->SetString("ping", buf);
		}
		else
		{
			playerKv->SetInt("ping", pi->GetPing());
		}

		if (m_bCustom)
			UpdateCustomCells(pi, playerKv);
	}

	if (pd.nItemID == -1)
	{
		// Create player's row
		pd.nItemID = m_pPlayerList->AddItem(pd.nTeamID, playerKv);
		m_pPlayerList->SetItemFgColor(pd.nItemID, gHUD.GetClientColor(client, NoTeamColor::White));
		m_pPlayerList->InvalidateLayout();
	}
	else
	{
		// Update player's row
		m_pPlayerList->ModifyItem(pd.nItemID, pd.nTeamID, playerKv);
	}

	m_pPlayerList->SetItemBgColor(pd.nItemID, GetPlayerBgColor(pi));

	playerKv->deleteThis();
}

void CScorePanel::UpdateClientIcon(CPlayerInfo *pi)
{
	CPlayerImage *pImg = static_cast<CPlayerImage *>(m_pImageList->GetImage(pi->GetIndex()));

	// Update size
	int size = GetClientIconSize();
	pImg->SetSize(size, size);

	// Update muted state
	pImg->SetMuted(GetClientVoiceMgr()->IsPlayerBlocked(pi->GetIndex()));

	// Update avatar
	uint64 steamID64 = pi->GetValidSteamID64();
	if (hud_scoreboard_showavatars.GetBool() && ClientSteamContext().SteamFriends() && ClientSteamContext().SteamUtils() && steamID64 != 0)
	{
		CSteamID steamIDForPlayer(steamID64);
		auto it = m_PlayerAvatars.find(steamIDForPlayer);

		if (it == m_PlayerAvatars.end())
		{
			CAvatarImage *pAvatar = new CAvatarImage();
			pAvatar->SetDrawFriend(false);
			pAvatar->SetAvatarSteamID(steamIDForPlayer);

			pImg->SetAvatar(pAvatar);
			m_PlayerAvatars.insert({ steamIDForPlayer, pAvatar });
		}
		else
		{
			pImg->SetAvatar(it->second);
		}
	}
	else
	{
		pImg->SetAvatar(nullptr);
	}
}

void CScorePanel::UpdateScoresAndCounts()
{
	// Reset team data
	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		TeamData &td = m_TeamData[i];
		td.iPlayerCount = 0;
		td.iFrags = 0;
		td.iDeaths = 0;
	}

	// Refresh scores
	int iPlayerCount = 0;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i);

		if (!pi->IsConnected())
			continue;

		TeamData &td = m_TeamData[pi->GetTeamNumber()];
		td.iFrags += pi->GetFrags();
		td.iDeaths += pi->GetDeaths();
		
		if (GetPlayerTeam(pi) != TEAM_SPECTATOR)
			td.iPlayerCount++;

		iPlayerCount++;
	}

	char buf[128];
	wchar_t wbuf[128];

	auto fnUpdateTeamHeader = [&](const char *pszTeamName, int nTeamID) {
		TeamData &td = m_TeamData[nTeamID];

		// Team name and player count
		wchar_t wbuf2[128];
		wchar_t *localizedName = nullptr;
		if (pszTeamName[0] == L'#' && (localizedName = g_pVGuiLocalize->Find(pszTeamName)))
		{
			// localizedName set to localized name
		}
		else
		{
			// set localizedName to pszTeamName converted to WString
			g_pVGuiLocalize->ConvertANSIToUnicode(pszTeamName, wbuf2, sizeof(wbuf2));
			localizedName = wbuf2;
		}

		V_snwprintf(wbuf, 128, L"%ls (%d/%d)", localizedName, td.iPlayerCount, iPlayerCount);
		m_pPlayerList->ModifyColumn(nTeamID, "name", wbuf);

		// Team efficiency
		float eff = CalculateEfficiency(td.iFrags, td.iDeaths);
		if (hud_scoreboard_effpercent.GetBool())
			snprintf(buf, sizeof(buf), "%.0f%%", eff * 100.0);
		else
			snprintf(buf, sizeof(buf), "%.2f", eff);
		g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
		m_pPlayerList->ModifyColumn(nTeamID, "eff", wbuf);

		// Team frags
		snprintf(buf, sizeof(buf), "%d", td.iFrags);
		g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
		m_pPlayerList->ModifyColumn(nTeamID, "frags", wbuf);

		// Team deaths
		snprintf(buf, sizeof(buf), "%d", td.iDeaths);
		g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
		m_pPlayerList->ModifyColumn(nTeamID, "deaths", wbuf);

		if (m_bCustom)
			UpdateCustomTeamHeader(nTeamID);
	};

	// Update team score and player count
	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		TeamData &td = m_TeamData[i];

		if (td.iPlayerCount == 0)
			continue;

		CTeamInfo *ti = GetTeamInfo(i);

		if (ti->IsScoreOverriden())
		{
			td.iFrags = ti->GetFrags();
			td.iDeaths = ti->GetDeaths();
		}

		fnUpdateTeamHeader(ti->GetDisplayName(), i);
	}

	// Update spectator team
	fnUpdateTeamHeader("#Spectators", TEAM_SPECTATOR);

	// Update total player count
	snprintf(buf, sizeof(buf), "%d/%d", iPlayerCount, gEngfuncs.GetMaxClients());
	m_pPlayerCountLabel->SetText(buf);

}

int CScorePanel::GetNameColumnWidth()
{
	int w = m_iColumnWidthName;

	if (!hud_scoreboard_showsteamid.GetBool())
		w += m_iColumnWidthSteamID;

	if (!hud_scoreboard_showeff.GetBool())
		w += m_iColumnWidthEff;

	return w;
}

int CScorePanel::GetPlayerTeam(CPlayerInfo *pi)
{
	if (pi->GetTeamName()[0] == '\0' && g_pViewport->GetNumberOfNonEmptyTeamPlayers() > 0)
		return TEAM_SPECTATOR;
	else
		return pi->GetTeamNumber();
}

Color CScorePanel::GetPlayerBgColor(CPlayerInfo *pi)
{
	if (pi->IsThisPlayer())
	{
		return m_ThisPlayerBgColor;
	}
	else if (m_iKillerIndex == pi->GetIndex())
	{
		Color color = m_KillerBgColor;
		float t = m_flKillerHighlightStart;
		float dt = HIGHLIGHT_KILLER_TIME;
		float k = -color.a() / dt;
		float b = -k * (t + dt);
		float a = k * gHUD.m_flTime + b;
		if (a > color.a() || a <= 0)
		{
			m_iKillerIndex = 0;
			m_flKillerHighlightStart = 0;
			return Color(0, 0, 0, 0);
		}
		else
		{
			color[3] = (int)a;
			return color;
		}
	}

	return Color(0, 0, 0, 0);
}

float CScorePanel::CalculateEfficiency(int kills, int deaths)
{
	int type = clamp(hud_scoreboard_efftype.GetInt(), 0, 2);

	switch (type)
	{
	case 0:
	{
		// K / D
		if (deaths == 0)
			deaths = 1;
		return (float)kills / deaths;
	}
	case 1:
	{
		// K / (D + 1)
		if (deaths == -1)
			deaths = 0;
		return (float)kills / (deaths + 1);
	}
	case 2:
	{
		// K / (K + D)
		if (kills + deaths == 0)
			return 1.0f;
		return (float)kills / (kills + deaths);
	}
	}

	return 0;
}

int CScorePanel::GetClientIconSize()
{
	return clamp(m_pPlayerList->GetLineSpacing() - 2, 0, 32);
}

void CScorePanel::CreatePlayerMenu()
{
	m_pPlayerMenu = new vgui2::Menu(this, "PlayerMenu");
	m_pPlayerMenu->SetVisible(false);
	m_pPlayerMenu->AddActionSignalTarget(this);

	m_MenuData.nMuteItemID = m_pPlayerMenu->AddMenuItem("Mute", "#BHL_Scores_MenuMute", "MenuMute", this);
	m_pPlayerMenu->AddSeparator();

	m_MenuData.nProfilePageItemID = m_pPlayerMenu->AddMenuItem("SteamProfile", "#BHL_Scores_MenuSteamProfile", "MenuSteamProfile", this);
	m_MenuData.nProfileUrlItemID = m_pPlayerMenu->AddMenuItem("SteamURL", "#BHL_Scores_MenuSteamURL", "MenuSteamURL", this);
	m_pPlayerMenu->AddSeparator();

	m_pPlayerMenu->AddMenuItem("CopyName", "#BHL_Scores_MenuCopyName", "MenuCopyName", this);
	m_pPlayerMenu->AddMenuItem("CopyNameRaw", "#BHL_Scores_MenuCopyNameRaw", "MenuCopyNameRaw", this);
	m_pPlayerMenu->AddMenuItem("CopySteamID", "#BHL_Scores_MenuCopySteamID", "MenuCopySteamID", this);
	m_pPlayerMenu->AddMenuItem("CopySteamID64", "#BHL_Scores_MenuCopySteamID64", "MenuCopySteamID64", this);
}

void CScorePanel::OpenPlayerMenu(int itemID)
{
	// Set menu info
	m_MenuData.nItemID = itemID;
	m_MenuData.nClient = 0;
	KeyValues *kv = m_pPlayerList->GetItemData(itemID);
	if (!kv)
		return;
	m_MenuData.nClient = kv->GetInt("client", 0);
	if (m_MenuData.nClient == 0)
		return;

	// SteamID64
	m_MenuData.nSteamID64 = GetPlayerInfo(m_MenuData.nClient)->GetValidSteamID64();
	if (m_MenuData.nSteamID64 != 0)
	{
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfilePageItemID, true);
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfileUrlItemID, true);
	}
	else
	{
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfilePageItemID, false);
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfileUrlItemID, false);
	}

	// Player muting
	bool isMuted = GetClientVoiceMgr()->IsPlayerBlocked(m_MenuData.nClient);
	bool thisPlayer = kv->GetBool("thisplayer", 0);
	if (thisPlayer && !isMuted)
	{
		// Can't mute yourself
		m_pPlayerMenu->UpdateMenuItem(m_MenuData.nMuteItemID, "#BHL_Scores_MenuMute", new KeyValues("Command", "command", "MenuMute"));
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nMuteItemID, false);
	}
	else
	{
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nMuteItemID, true);
		if (isMuted)
		{
			m_pPlayerMenu->UpdateMenuItem(m_MenuData.nMuteItemID, "#BHL_Scores_MenuUnmute", new KeyValues("Command", "command", "MenuMute"));
		}
		else
		{
			m_pPlayerMenu->UpdateMenuItem(m_MenuData.nMuteItemID, "#BHL_Scores_MenuMute", new KeyValues("Command", "command", "MenuMute"));
		}
	}

	m_pPlayerMenu->PositionRelativeToPanel(this, vgui2::Menu::CURSOR, 0, true);
}

void CScorePanel::OnPlayerMenuCommand(MenuAction command)
{
	CPlayerInfo *pi = GetPlayerInfo(m_MenuData.nClient);

	if (!pi->IsConnected())
		return;

	switch (command)
	{
	case MenuAction::Mute:
	{
		if (GetClientVoiceMgr()->IsPlayerBlocked(pi->GetIndex()))
		{
			// Unmute
			GetClientVoiceMgr()->SetPlayerBlockedState(pi->GetIndex(), false);

			char string1[1024];
			snprintf(string1, sizeof(string1), CHudTextMessage::BufferedLocaliseTextString("#Unmuted"), pi->GetDisplayName(true));
			CHudChat::Get()->ChatPrintf(0, "** %s", string1);
		}
		else if (!pi->IsThisPlayer())
		{
			// Mute
			GetClientVoiceMgr()->SetPlayerBlockedState(pi->GetIndex(), true);

			char string1[1024];
			char string2[1024];
			snprintf(string1, sizeof(string1), CHudTextMessage::BufferedLocaliseTextString("#Muted"), pi->GetDisplayName(true));
			snprintf(string2, sizeof(string2), "%s", CHudTextMessage::BufferedLocaliseTextString("#No_longer_hear_that_player"));
			CHudChat::Get()->ChatPrintf(0, "** %s", string1);
			CHudChat::Get()->ChatPrintf(0, "** %s", string2);
		}

		// Muting one player may mute others (if they have identical Unique IDs)
		UpdateAllClients();

		break;
	}
	case MenuAction::SteamProfile:
	{
		if (ClientSteamContext().SteamFriends())
		{
			// Open in overlay
			CSteamID steamId = CSteamID((uint64)m_MenuData.nSteamID64);
			ClientSteamContext().SteamFriends()->ActivateGameOverlayToUser("steamid", steamId);
		}
		else
		{
			// Open in browser
			std::string url = STEAM_PROFILE_URL + std::to_string(m_MenuData.nSteamID64);
			vgui2::system()->ShellExecute("open", url.c_str());
		}

		break;
	}
	case MenuAction::SteamURL:
	{
		std::string url = STEAM_PROFILE_URL + std::to_string(m_MenuData.nSteamID64);
		vgui2::system()->SetClipboardText(url.c_str(), url.size());
		break;
	}
	case MenuAction::CopyName:
	{
		wchar_t name[MAX_PLAYER_NAME + 1];
		g_pVGuiLocalize->ConvertANSIToUnicode(pi->GetDisplayName(true), name, sizeof(name));
		vgui2::system()->SetClipboardText(name, wcslen(name));
		break;
	}
	case MenuAction::CopyNameRaw:
	{
		wchar_t name[MAX_PLAYER_NAME + 1];
		g_pVGuiLocalize->ConvertANSIToUnicode(pi->GetName(), name, sizeof(name));
		vgui2::system()->SetClipboardText(name, wcslen(name));
		break;
	}
	case MenuAction::CopySteamID:
	{
		std::string steamid = "STEAM_" + std::string(pi->GetSteamID());
		vgui2::system()->SetClipboardText(steamid.c_str(), steamid.size());
		break;
	}
	case MenuAction::CopySteamID64:
	{
		std::string steamid = std::to_string(m_MenuData.nSteamID64);
		vgui2::system()->SetClipboardText(steamid.c_str(), steamid.size());
		break;
	}
	}
}

CScorePanel::SizeMode CScorePanel::GetSizeMode()
{
	return (SizeMode)clamp(hud_scoreboard_size.GetInt(), 0, 2);
}

int CScorePanel::GetLineSpacingForHeight(int h)
{
	if (h < 600)
		return 18; // (0; 600)
	if (h < 720)
		return 22; // [600; 720]
	if (h < 800)
		return 24; // [720; 800)
	if (h < 1024)
		return 26; // [800; 1024)
	if (h < 1080)
		return 28; // [1024; 1080)

	return 28; // >= 1080
}

int CScorePanel::GetLineSpacingForNormal()
{
	if (hud_scoreboard_spacing_normal.GetInt() > 0)
		return hud_scoreboard_spacing_normal.GetInt() + m_iLineSpacingExtra;
	if (m_iLineSpacingExtra > 0)
		return m_iDefaultLineSpacing + m_iLineSpacingExtra;
	return 0;
}

int CScorePanel::GetLineSpacingForCompact()
{
	if (hud_scoreboard_spacing_compact.GetInt() > 0)
		return hud_scoreboard_spacing_compact.GetInt() + m_iLineSpacingExtra;
	return GetLineSpacingForHeight(ScreenHeight) + m_iLineSpacingExtra;
}

void CScorePanel::RestoreSize()
{
	if (GetSizeMode() == SizeMode::Compact)
		m_pPlayerList->SetLineSpacingOverride(GetLineSpacingForCompact());
	else
		m_pPlayerList->SetLineSpacingOverride(GetLineSpacingForNormal());
}

void CScorePanel::Resize()
{
	SizeMode mode = GetSizeMode();

	// Returns true if scrollbar was enabled
	auto fnUpdateSize = [&](int &height) {
		int wide, tall, x, y;
		int listHeight = 0, addHeight = 0;
		bool bIsOverflowed = false;
		height = 0;

		m_pPlayerList->GetPos(x, y);
		addHeight = y + m_iBottomPadding; // Distance on the top and on the bottom of the player list
		height += addHeight;
		m_pPlayerList->GetContentSize(wide, tall);
		listHeight = max(m_iMinHeight, tall);
		height += listHeight;

		if (ScreenHeight - height < m_iVerticalMargin * 2)
		{
			// It didn't fit
			height = ScreenHeight - m_iVerticalMargin * 2;
			listHeight = height - addHeight;
			m_pPlayerList->SetVerticalScrollbar(true);
			bIsOverflowed = true;
		}
		else
		{
			m_pPlayerList->SetVerticalScrollbar(false);
		}

		m_pPlayerList->GetSize(wide, tall);
		m_pPlayerList->SetSize(wide, listHeight);

		return bIsOverflowed;
	};

	int wide, tall, x, y;
	int height;
	if (fnUpdateSize(height) && mode == SizeMode::Auto)
	{
		// Content overflowed, scrollbar is now visible. Set comapct line spacing
		m_pPlayerList->SetLineSpacingOverride(GetLineSpacingForCompact());

		// Refresh player info to update avatar sizes
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			UpdateClientInfo(i);
		}

		// Resize again
		fnUpdateSize(height);
	}

	GetSize(wide, tall);
	SetSize(wide, height);

	// Move to center
	GetPos(x, y);
	y = (ScreenHeight - height) / 2;
	SetPos(x, y);

	// A new size lays out the children from the layout file, which moves the banner
	UpdateLeaderBanner();
}

bool CScorePanel::StaticPlayerSortFuncByFrags(vgui2::SectionedListPanel *list, int itemID1, int itemID2)
{
	KeyValues *it1 = list->GetItemData(itemID1);
	KeyValues *it2 = list->GetItemData(itemID2);
	Assert(it1 && it2);

	// first compare frags
	int v1 = it1->GetInt("frags");
	int v2 = it2->GetInt("frags");
	if (v1 > v2)
		return true;
	else if (v1 < v2)
		return false;

	// next compare deaths
	v1 = it1->GetInt("deaths");
	v2 = it2->GetInt("deaths");
	if (v1 > v2)
		return false;
	else if (v1 < v2)
		return true;

	// the same, so compare itemID's (as a sentinel value to get deterministic sorts)
	return itemID1 < itemID2;
}

bool CScorePanel::IsTeamBefore(int ilhs, int irhs)
{
	if (m_bCustom)
	{
		int cmp = CScoreboardData::Get().CompareTeams(ilhs, irhs);

		if (cmp != 0)
			return cmp < 0;
	}

	const TeamData &lhs = m_TeamData[ilhs];
	const TeamData &rhs = m_TeamData[irhs];

	// Compare kills
	if (lhs.iFrags > rhs.iFrags)
		return true;
	else if (lhs.iFrags < rhs.iFrags)
		return false;

	// Compare deaths if kills are equal
	if (lhs.iDeaths < rhs.iDeaths)
		return true;
	else if (lhs.iDeaths > rhs.iDeaths)
		return false;

	// Compare idx if everything is equal
	return ilhs > irhs;
}

void CScorePanel::BuildCustomColumns()
{
	m_CustomColumns.clear();
	m_iLineSpacingExtra = 0;
	m_iLevelColumn = -1;
	m_iWeaponColumn = -1;
	m_iProgressColumn = -1;

	if (!m_bCustom)
		return;

	CScoreboardData &data = CScoreboardData::Get();

	m_CellStyle.hFont = m_CellFont;
	m_CellStyle.hBadgeFont = m_BadgeFont;
	m_CellStyle.filledColor = m_CellFilledColor;
	m_CellStyle.emptyColor = m_CellEmptyColor;
	m_CellStyle.currentColor = m_CellCurrentColor;
	m_CellStyle.lastColor = m_CellLastColor;
	m_CellStyle.badgeBgColor = m_BadgeBgColor;
	m_CellStyle.badgeFgColor = m_BadgeFgColor;
	m_CellStyle.iDotSize = m_iCellDotSize;
	m_CellStyle.iDotGap = m_iCellDotGap;
	m_CellStyle.iStripTall = m_iCellStripTall;
	m_CellStyle.iStripGap = m_iCellStripGap;
	m_CellStyle.iSpacing = m_iCellSpacing;

	if (m_bGunGame)
	{
		m_iLevelColumn = data.FindColumn("level");
		m_iWeaponColumn = data.FindColumn("weapon");
		m_iProgressColumn = data.FindColumn("progress");
	}

	// Column names indexed by ColumnType, the same as the keys of Builtin columns
	static const char *const s_StdColumnNames[] = { "rank", "avatar", "name", "steamid", "eff", "frags", "deaths", "ping" };
	int nSlotCount = 0;

	auto fnGetBuiltinType = [](const char *key) {
		for (ColumnType type : { ColumnType::Rank, ColumnType::SteamID, ColumnType::Eff, ColumnType::Frags, ColumnType::Deaths, ColumnType::Ping })
		{
			if (!Q_strcmp(key, s_StdColumnNames[(int)type]))
				return type;
		}

		return ColumnType::Custom;
	};

	// bCell: the column is drawn by CScoreCellImage
	auto fnAddColumn = [&](ColumnType type, int nColumnID, const char *title, int width, int align, bool bCell) {
		CustomColumn col;
		col.type = type;
		col.nColumnID = nColumnID;
		col.nSlot = bCell ? nSlotCount++ : -1;
		col.iAlign = align;
		col.iWidth = width;

		if (type == ColumnType::Avatar)
			col.iFlags = vgui2::SectionedListPanel::COLUMN_IMAGE | vgui2::SectionedListPanel::COLUMN_CENTER;
		else if (bCell)
			col.iFlags = vgui2::SectionedListPanel::COLUMN_IMAGE;
		else if (type == ColumnType::Name && gHUD.GetColorCodeAction() == ColorCodeAction::Handle)
			col.iFlags = vgui2::SectionedListPanel::COLUMN_BRIGHT | vgui2::SectionedListPanel::COLUMN_COLORED;
		else
			col.iFlags = vgui2::SectionedListPanel::COLUMN_BRIGHT;

		if (type != ColumnType::Avatar && align == 1)
			col.iFlags |= vgui2::SectionedListPanel::COLUMN_CENTER;
		else if (type != ColumnType::Avatar && align == 2)
			col.iFlags |= vgui2::SectionedListPanel::COLUMN_RIGHT;

		if (type == ColumnType::Custom)
			snprintf(col.szName, sizeof(col.szName), "c%d_%s", nColumnID, data.GetColumn(nColumnID).szKey);
		else
			Q_strncpy(col.szName, s_StdColumnNames[(int)type], sizeof(col.szName));

		CScoreboardData::LocalizeString(title, col.wszTitle, sizeof(col.wszTitle));
		m_CustomColumns.push_back(col);
	};

	// pColumn: the Builtin column that moves the standard one
	auto fnAddStdColumn = [&](ColumnType type, const CScoreboardData::Column *pColumn) {
		const char *title = "";
		int width = 0;
		bool bVisible = true;

		switch (type)
		{
		case ColumnType::Rank:
			title = "#";
			width = m_iColumnWidthRank;
			break;
		case ColumnType::Avatar:
			width = m_iColumnWidthAvatar;
			break;
		case ColumnType::Name:
			title = "#PlayerName";
			break;
		case ColumnType::SteamID:
			title = "#BHL_Scores_ColSteamID";
			width = m_iColumnWidthSteamID;
			bVisible = hud_scoreboard_showsteamid.GetBool() && !data.IsStdColumnHidden(bhl::sb::E_ConfigFlags::HideSteamID);
			break;
		case ColumnType::Eff:
			title = "#BHL_Scores_ColEff";
			width = m_iColumnWidthEff;
			bVisible = hud_scoreboard_showeff.GetBool() && !data.IsStdColumnHidden(bhl::sb::E_ConfigFlags::HideEff);
			break;
		case ColumnType::Frags:
			title = "#PlayerScore";
			width = m_iColumnWidthFrags;
			bVisible = !data.IsStdColumnHidden(bhl::sb::E_ConfigFlags::HideFrags);
			break;
		case ColumnType::Deaths:
			title = "#PlayerDeath";
			width = m_iColumnWidthDeaths;
			bVisible = !data.IsStdColumnHidden(bhl::sb::E_ConfigFlags::HideDeaths);
			break;
		case ColumnType::Ping:
			title = hud_scoreboard_showloss.GetBool() ? "#BHL_Scores_ColPingLoss" : "#BHL_Scores_ColPing";
			width = m_iColumnWidthPing;
			break;
		case ColumnType::Custom:
			break;
		}

		if (!bVisible)
			return;

		int align = 0;

		if (pColumn)
		{
			align = pColumn->GetAlign();

			if (pColumn->szTitle[0])
				title = pColumn->szTitle;
		}

		fnAddColumn(type, -1, title, width, align, false);
	};

	// Standard columns moved by Builtin columns
	std::array<const CScoreboardData::Column *, (int)ColumnType::Custom> builtins = {};

	for (int i = 0; i < bhl::sb::MAX_COLUMNS; i++)
	{
		const CScoreboardData::Column &col = data.GetColumn(i);

		if (col.bValid && !col.IsHidden() && col.display == bhl::sb::E_Display::Builtin)
		{
			ColumnType type = fnGetBuiltinType(col.szKey);

			if (type != ColumnType::Custom && !builtins[(int)type])
				builtins[(int)type] = &col;
		}
	}

	if (builtins[(int)ColumnType::Rank])
		fnAddStdColumn(ColumnType::Rank, builtins[(int)ColumnType::Rank]);

	fnAddStdColumn(ColumnType::Avatar, nullptr);
	fnAddStdColumn(ColumnType::Name, nullptr);

	for (ColumnType type : { ColumnType::SteamID, ColumnType::Eff, ColumnType::Frags, ColumnType::Deaths })
	{
		if (!builtins[(int)type])
			fnAddStdColumn(type, nullptr);
	}

	for (int i = 0; i < bhl::sb::MAX_COLUMNS; i++)
	{
		const CScoreboardData::Column &col = data.GetColumn(i);

		if (!col.bValid || col.IsHidden())
			continue;

		if (col.display == bhl::sb::E_Display::Builtin)
		{
			ColumnType type = fnGetBuiltinType(col.szKey);

			if (type != ColumnType::Custom && type != ColumnType::Rank && builtins[(int)type] == &col)
				fnAddStdColumn(type, &col);

			continue;
		}

		int width = col.iWidth;
		bool bCell = false;

		switch (col.display)
		{
		case bhl::sb::E_Display::Icon:
			width = width ? width : 100;
			bCell = true;
			break;
		case bhl::sb::E_Display::Meter:
			width = width ? width : 90;
			bCell = true;
			break;
		case bhl::sb::E_Display::Steps:
			width = width ? width : 60;
			bCell = true;
			break;
		default:
			width = width ? width : 60;
			break;
		}

		fnAddColumn(ColumnType::Custom, i, col.szTitle, vgui2::scheme()->GetProportionalScaledValue(width), col.GetAlign(), bCell);
	}

	if (!builtins[(int)ColumnType::Ping])
		fnAddStdColumn(ColumnType::Ping, nullptr);

	// Name takes the free width and shrinks to its minimum, then columns are dropped from the end
	int reservedWide = LIST_ROW_PADDING + m_pPlayerList->GetScrollBar()->GetWide();
	int otherWide = 0;

	for (const CustomColumn &col : m_CustomColumns)
		otherWide += col.iWidth;

	int freeWide = m_pPlayerList->GetWide() - reservedWide;

	// On narrow screens Name gives up to a fifth of its minimum before a column is dropped
	int nameMin = std::max(std::min(freeWide - otherWide, m_iColumnWidthNameMin), m_iColumnWidthNameMin * 4 / 5);

	while (otherWide + nameMin > freeWide && m_CustomColumns.back().type != ColumnType::Name)
	{
		otherWide -= m_CustomColumns.back().iWidth;
		m_CustomColumns.pop_back();
	}

	for (CustomColumn &col : m_CustomColumns)
	{
		if (col.type == ColumnType::Name)
			col.iWidth = std::max(freeWide - otherWide, nameMin);
		else if (col.type == ColumnType::Custom && data.GetColumn(col.nColumnID).display == bhl::sb::E_Display::Steps)
			m_iLineSpacingExtra = m_iCellStripTall + m_iCellStripGap;
	}

	m_pPlayerList->SetLineSpacingOverride(0);
	m_iDefaultLineSpacing = m_pPlayerList->GetLineSpacing();
}

CScoreCellImage *CScorePanel::GetCellImage(int index)
{
	if (!m_pImageList->IsValidIndex(index) || !m_pImageList->GetImage(index))
		m_pImageList->SetImageAtIndex(index, new CScoreCellImage(m_CellStyle));

	return static_cast<CScoreCellImage *>(m_pImageList->GetImage(index));
}

void CScorePanel::UpdateCustomCells(CPlayerInfo *pi, KeyValues *kv)
{
	CScoreboardData &data = CScoreboardData::Get();
	int client = pi->GetIndex();
	int lineSpacing = m_pPlayerList->GetLineSpacing();
	bool bLastLevel = IsOnLastLevel(client);
	Color color = gHUD.GetClientColor(client, NoTeamColor::White);
	wchar_t wbuf[128];
	char buf[128];

	for (const CustomColumn &col : m_CustomColumns)
	{
		if (col.type == ColumnType::Rank)
		{
			// With team sections the place is shown in team headers
			int place = m_PlayerData[client].nTeamID == HEADER_SECTION_ID ? GetPlayerRank(client) : 0;
			buf[0] = '\0';

			if (place > 0)
				snprintf(buf, sizeof(buf), "%d", place);

			kv->SetString(col.szName, buf);
			continue;
		}

		if (col.type != ColumnType::Custom)
			continue;

		if (col.nSlot == -1)
		{
			data.GetPlayerValue(client, col.nColumnID).ToString(wbuf, sizeof(wbuf));
			Q_WStringToUTF8(wbuf, buf, sizeof(buf));
			kv->SetString(col.szName, buf);
			continue;
		}

		// Image columns read the index of the image from the row
		int index = CELL_IMAGE_BASE + col.nSlot * MAX_PLAYERS + client - 1;
		CScoreCellImage *pCell = GetCellImage(index);
		pCell->SetSize(col.iWidth - COLUMN_DATA_GAP, lineSpacing);
		pCell->SetAlign(col.iAlign);
		pCell->SetColor(color);
		pCell->SetValue(data.GetColumn(col.nColumnID).display, data.GetPlayerValue(client, col.nColumnID));

		if (bLastLevel && col.nColumnID == m_iWeaponColumn)
			pCell->SetBadge(g_pVGuiLocalize->Find("#BHL_GG_Final"));

		kv->SetInt(col.szName, index);
	}
}

void CScorePanel::UpdateCustomTeamHeader(int nTeamID)
{
	CScoreboardData &data = CScoreboardData::Get();
	bool bIsTeam = nTeamID >= 1 && nTeamID <= MAX_TEAMS;
	wchar_t wbuf[128];

	for (const CustomColumn &col : m_CustomColumns)
	{
		if (col.type != ColumnType::Rank && col.type != ColumnType::Custom)
			continue;

		wbuf[0] = L'\0';

		if (bIsTeam && col.type == ColumnType::Rank)
		{
			int place = GetTeamRank(nTeamID);

			if (place > 0)
				V_snwprintf(wbuf, ARRAYSIZE(wbuf), L"%d", place);
		}
		else if (bIsTeam)
		{
			const CScoreboardData::Value &value = data.GetTeamValue(nTeamID, col.nColumnID);

			// Like dots in rows, a short progress has no counter
			if (data.GetColumn(col.nColumnID).display != bhl::sb::E_Display::Meter || value.type != bhl::sb::E_ValueType::Progress || value.iMax > bhl::sb::MAX_DOTS)
				value.ToString(wbuf, sizeof(wbuf));
		}

		m_pPlayerList->ModifyColumn(nTeamID, col.szName, wbuf);
	}
}

void CScorePanel::UpdateLeaderBanner()
{
	CScoreboardData &data = CScoreboardData::Get();
	wchar_t wbuf[256];

	// Leader: the first team in teamplay, the first participant otherwise
	int leader = 0;
	bool bIsTeam = false;

	if (m_bGunGame)
	{
		for (int i = 1; i <= MAX_TEAMS; i++)
		{
			if (m_TeamData[i].iPlayerCount > 0 && (leader == 0 || IsTeamBefore(i, leader)))
				leader = i;
		}

		bIsTeam = leader != 0;

		for (int i = 1; i <= MAX_PLAYERS && !bIsTeam; i++)
		{
			CPlayerInfo *pi = GetPlayerInfo(i);

			if (pi->IsConnected() && !pi->IsSpectator() && (leader == 0 || IsPlayerBefore(i, leader)))
				leader = i;
		}
	}

	auto fnGetValue = [&](int col) -> const CScoreboardData::Value & {
		return bIsTeam ? data.GetTeamValue(leader, col) : data.GetPlayerValue(leader, col);
	};

	bool bShowBanner = leader != 0 && m_iLevelColumn != -1 && IsLastLevel(fnGetValue(m_iLevelColumn));
	m_pLeaderTitleLabel->SetVisible(bShowBanner);
	m_pLeaderLineLabel->SetVisible(bShowBanner);

	if (!bShowBanner)
	{
		if (m_iServerNameWide != -1)
			m_pServerNameLabel->SetWide(m_iServerNameWide);

		m_iServerNameWide = -1;
		return;
	}

	wchar_t wname[128];
	wchar_t wweapon[64] = L"";
	wchar_t wkills[64];
	int killsLeft = 0;

	if (bIsTeam)
		CScoreboardData::LocalizeString(GetTeamInfo(leader)->GetDisplayName(), wname, sizeof(wname));
	else
		Q_UTF8ToWString(GetPlayerInfo(leader)->GetDisplayName(true), wname, sizeof(wname), STRINGCONVERT_REPLACE);

	if (m_iWeaponColumn != -1)
		CScoreboardData::GetIconName(fnGetValue(m_iWeaponColumn), wweapon, sizeof(wweapon));

	if (m_iProgressColumn != -1)
	{
		const CScoreboardData::Value &progress = fnGetValue(m_iProgressColumn);

		if (progress.type == bhl::sb::E_ValueType::Progress)
			killsLeft = std::max(progress.iMax - progress.iValue, 0);
	}

	FormatScoreKills(wkills, sizeof(wkills), killsLeft);
	ConstructScoreText(wbuf, sizeof(wbuf), "BHL_GG_LeaderLine", 3, wname, wweapon, wkills);

	m_pLeaderTitleLabel->SetText("#BHL_GG_LeaderFinal");
	m_pLeaderTitleLabel->SetFgColor(m_GGBannerAccentColor);
	m_pLeaderLineLabel->SetText(wbuf);

	// Both lines end left of the player count, on the lines of the server and the map name
	int nameX, nameY, nameWide, nameTall, mapX, mapY, mapWide, mapTall, countX, countY, countWide, countTall;
	int mapTextWide, countTextWide, titleWide, lineWide, tall;
	m_pServerNameLabel->GetBounds(nameX, nameY, nameWide, nameTall);
	m_pMapNameLabel->GetBounds(mapX, mapY, mapWide, mapTall);
	m_pPlayerCountLabel->GetBounds(countX, countY, countWide, countTall);
	m_pMapNameLabel->GetContentSize(mapTextWide, tall);
	m_pPlayerCountLabel->GetContentSize(countTextWide, tall);
	m_pLeaderTitleLabel->GetContentSize(titleWide, tall);
	m_pLeaderLineLabel->GetContentSize(lineWide, tall);

	if (m_iServerNameWide == -1)
		m_iServerNameWide = nameWide;

	int tagWide = m_wszTag[0] ? GetScoreBadgeWide(m_CellStyle, m_wszTag) + m_CellStyle.iSpacing : 0;
	int boxLeft = m_iGGBannerAccentWide + m_iGGBannerPadding + m_CellStyle.iSpacing;
	int bannerRight = countX + countWide - countTextWide - m_CellStyle.iSpacing - m_iGGBannerPadding;

	// A long line is cut, so the map name and a third of the server name stay visible
	int bannerMinX = std::max(mapX + mapTextWide, nameX + tagWide + (bannerRight - nameX) / 3) + boxLeft;
	int wide = std::min(std::max(titleWide, lineWide), std::max(bannerRight - bannerMinX, 0));
	int bannerX = bannerRight - wide;
	m_pLeaderTitleLabel->SetBounds(bannerX, nameY, wide, nameTall);
	m_pLeaderLineLabel->SetBounds(bannerX, mapY, wide, mapTall);

	// The server name and its tag end before the banner
	m_pServerNameLabel->SetWide(std::clamp(bannerX - boxLeft - tagWide - nameX, 0, m_iServerNameWide));
}

int CScorePanel::GetPlayerRank(int client)
{
	CScoreboardData &data = CScoreboardData::Get();

	if (data.GetFirstSortColumn() != -1)
		return data.GetPlayerPlace(client);

	// Without sort keys the place follows row order
	if (GetPlayerInfo(client)->IsSpectator())
		return 0;

	int place = 1;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i);

		if (i != client && pi->IsConnected() && !pi->IsSpectator() && IsPlayerBefore(i, client))
			place++;
	}

	return place;
}

int CScorePanel::GetTeamRank(int nTeamID)
{
	CScoreboardData &data = CScoreboardData::Get();

	if (data.GetFirstSortColumn() != -1)
		return data.GetTeamPlace(nTeamID);

	for (int i = 0; i < m_iSortedTeamCount; i++)
	{
		if (m_SortedTeamIDs[i] == nTeamID)
			return i + 1;
	}

	return 0;
}

bool CScorePanel::IsOnLastLevel(int client)
{
	return m_iLevelColumn != -1 && IsLastLevel(CScoreboardData::Get().GetPlayerValue(client, m_iLevelColumn));
}

bool CScorePanel::IsLastLevel(const CScoreboardData::Value &level)
{
	return level.type == bhl::sb::E_ValueType::Progress && level.iMax > 0 && level.iValue >= level.iMax;
}

bool CScorePanel::IsPlayerBefore(int client1, int client2)
{
	int cmp = CScoreboardData::Get().ComparePlayers(client1, client2);

	if (cmp != 0)
		return cmp < 0;

	// Standard order
	CPlayerInfo *pi1 = GetPlayerInfo(client1);
	CPlayerInfo *pi2 = GetPlayerInfo(client2);

	if (pi1->IsConnected() && pi2->IsConnected())
	{
		if (pi1->GetFrags() != pi2->GetFrags())
			return pi1->GetFrags() > pi2->GetFrags();

		if (pi1->GetDeaths() != pi2->GetDeaths())
			return pi1->GetDeaths() < pi2->GetDeaths();
	}

	return client1 < client2;
}

bool CScorePanel::StaticPlayerSortFuncCustom(vgui2::SectionedListPanel *list, int itemID1, int itemID2)
{
	KeyValues *it1 = list->GetItemData(itemID1);
	KeyValues *it2 = list->GetItemData(itemID2);
	Assert(it1 && it2);

	return IsPlayerBefore(it1->GetInt("client"), it2->GetInt("client"));
}
