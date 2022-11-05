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
// Hud_scores.cpp
//
// implementation of CHudScores class
//

#include <algorithm>
#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "scores.h"
#include "vgui/client_viewport.h"
#include "hud/ag/ag_global.h"

ConVar hud_scores("hud_scores", "0", FCVAR_BHL_ARCHIVE, "Maximum number of lines for HUD scoreboard, 0 to disable");
ConVar hud_scores_pos("hud_scores_pos", "30 50", FCVAR_BHL_ARCHIVE, "Position of HUD scoreboard, 'x y' in pixels");
ConVar hud_scores_alpha("hud_scores_alpha", "20", FCVAR_BHL_ARCHIVE, "Alpha of HUD scoreboard");

DEFINE_HUD_ELEM(CHudScores);

void CHudScores::Init()
{
	m_iFlags |= HUD_ACTIVE;
}

void CHudScores::VidInit()
{
	m_iOverLay = 0;
	m_flScoreBoardLastUpdated = 0;
}

void CHudScores::Draw(float flTime)
{
	if (hud_scores.GetInt() < 1)
		return;

	// No Scoreboard in single-player
	if (gEngfuncs.GetMaxClients() <= 1)
		return;

	// Update the Scoreboard
	if (m_flScoreBoardLastUpdated < gHUD.m_flTime)
	{
		Update();
		m_flScoreBoardLastUpdated = gHUD.m_flTime + 0.5;
	}

	int xpos, ypos;
	xpos = 30;
	ypos = 50;
	sscanf(hud_scores_pos.GetString(), "%i %i", &xpos, &ypos);

	for (int iLine = 0; iLine < m_iLines && iLine < hud_scores.GetInt(); iLine++)
	{
		HudScoresData &data = m_ScoresData[iLine];

		int r, g, b;
		r = data.color.r();
		g = data.color.g();
		b = data.color.b();
		const uint8_t alpha = static_cast<uint8_t>(clamp(hud_scores_alpha.GetInt(), 0, 255));
		FillRGBA(xpos - 10, ypos + 2, m_iOverLay, gHUD.m_scrinfo.iCharHeight * 0.9, r, g, b, alpha);

		ScaleColors(r, g, b, 135);

		const int ixposnew = AgDrawHudString(xpos, ypos, ScreenWidth, data.wszScore, r, g, b);
		m_iOverLay = max(ixposnew - xpos + 20, m_iOverLay);
		ypos += gHUD.m_scrinfo.iCharHeight * 0.9;
	}
}

void CHudScores::Update()
{
	char buf[128];
	int iMaxLines = hud_scores.GetInt();
	m_iLines = 0;

	if (gHUD.m_Teamplay)
	{
		std::fill(m_TeamData.begin(), m_TeamData.end(), TeamData());

		// Assign player teams, calculate team scores
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			CPlayerInfo *pi = GetPlayerInfo(i)->Update();

			if (!pi->IsConnected())
				continue;

			TeamData &td = m_TeamData[pi->GetTeamNumber()];
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
		std::sort(m_SortedTeamIDs.begin(), m_SortedTeamIDs.begin() + iTeamCount, [&](int ilhs, int irhs) {
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
		});

		// Add teams to m_ScoresData
		for (int i = 0; i < iTeamCount && m_iLines < iMaxLines; i++)
		{
			int teamIdx = m_SortedTeamIDs[i];
			TeamData &team = m_TeamData[teamIdx];
			HudScoresData &data = m_ScoresData[m_iLines];

			snprintf(buf, sizeof(buf), "%-5i %s", team.iFrags, GetTeamInfo(teamIdx)->GetDisplayName());
			Q_UTF8ToWString(buf, data.wszScore, sizeof(data.wszScore), STRINGCONVERT_REPLACE);
			data.color = g_pViewport->GetTeamColor(teamIdx);
			m_iLines++;
		}
	}
	else
	{
		// Fill m_SortedPlayerIDs with players
		int iPlayerCount = 0;
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			CPlayerInfo *pi = GetPlayerInfo(i)->Update();

			if (pi->IsConnected())
			{
				m_SortedPlayerIDs[iPlayerCount] = i;
				iPlayerCount++;
			}
		}

		// Sort players
		std::sort(m_SortedPlayerIDs.begin(), m_SortedPlayerIDs.begin() + iPlayerCount, [&](int ilhs, int irhs) {
			CPlayerInfo *lhs = GetPlayerInfo(ilhs);
			CPlayerInfo *rhs = GetPlayerInfo(irhs);

			// Compare kills
			if (lhs->GetFrags() > rhs->GetFrags())
				return true;
			else if (lhs->GetFrags() < rhs->GetFrags())
				return false;

			// Compare deaths if kills are equal
			if (lhs->GetDeaths() < rhs->GetDeaths())
				return true;
			else if (lhs->GetDeaths() > rhs->GetDeaths())
				return false;

			// Compare idx if everything is equal
			return ilhs > irhs;
		});

		// Add players to m_ScoresData
		for (int i = 0; i < iPlayerCount && m_iLines < iMaxLines; i++)
		{
			int idx = m_SortedPlayerIDs[i];
			CPlayerInfo *pi = GetPlayerInfo(idx);
			HudScoresData &data = m_ScoresData[m_iLines];

			snprintf(buf, sizeof(buf), "%-5i %s", pi->GetFrags(), pi->GetDisplayName());
			Q_UTF8ToWString(buf, data.wszScore, sizeof(data.wszScore), STRINGCONVERT_REPLACE);
			data.color = gHUD.GetClientColor(idx, NoTeamColor::Orange);
			m_iLines++;
		}
	}
}
