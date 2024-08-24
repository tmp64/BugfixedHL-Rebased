// Martin Webrant (BulliT)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "ag_vote.h"
#include "ag_global.h"

DEFINE_HUD_ELEM(AgHudVote);

AgHudVote::AgHudVote()
{
}

void AgHudVote::Init()
{
	HookMessage<&AgHudVote::MsgFunc_Vote>("Vote");

	m_iFlags = 0;
	m_flTurnoff = 0.0;
	m_iVoteStatus = VoteStatus::NotRunning;
	m_iFor = 0;
	m_iAgainst = 0;
	m_iUndecided = 0;
	m_szVote[0] = '\0';
	m_szValue[0] = '\0';
	m_szCalled[0] = '\0';
}

void AgHudVote::VidInit()
{
}

void AgHudVote::Reset(void)
{
	m_iFlags &= ~HUD_ACTIVE;
}

void AgHudVote::Draw(float fTime)
{
	if (gHUD.m_flTime > m_flTurnoff || gHUD.m_iIntermission)
	{
		Reset();
		return;
	}

	char szText[128];

	int r, g, b, a;
	a = 255 * gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);

	sprintf(szText, "Vote: %s %s", m_szVote, m_szValue);
	gHUD.DrawHudString(ScreenWidth / 20, ScreenHeight / 8, 0, szText, r, g, b);
	sprintf(szText, "Called by: %s", m_szCalled);
	AgDrawHudString(ScreenWidth / 20, ScreenHeight / 8 + gHUD.m_scrinfo.iCharHeight, ScreenWidth, szText, r, g, b);
	if (VoteStatus::Called == m_iVoteStatus)
	{
		sprintf(szText, "For: %d", m_iFor);
		gHUD.DrawHudString(ScreenWidth / 20, ScreenHeight / 8 + gHUD.m_scrinfo.iCharHeight * 2, 0, szText, r, g, b);
		sprintf(szText, "Against: %d ", m_iAgainst);
		gHUD.DrawHudString(ScreenWidth / 20, ScreenHeight / 8 + gHUD.m_scrinfo.iCharHeight * 3, 0, szText, r, g, b);
		sprintf(szText, "Undecided: %d", m_iUndecided);
		gHUD.DrawHudString(ScreenWidth / 20, ScreenHeight / 8 + gHUD.m_scrinfo.iCharHeight * 4, 0, szText, r, g, b);
	}
	else if (VoteStatus::Accepted == m_iVoteStatus)
	{
		V_strcpy_safe(szText, "Accepted!");
		gHUD.DrawHudString(ScreenWidth / 20, ScreenHeight / 8 + gHUD.m_scrinfo.iCharHeight * 2, 0, szText, r, g, b);
	}
	else if (VoteStatus::Denied == m_iVoteStatus)
	{
		V_strcpy_safe(szText, "Denied!");
		gHUD.DrawHudString(ScreenWidth / 20, ScreenHeight / 8 + gHUD.m_scrinfo.iCharHeight * 2, 0, szText, r, g, b);
	}
}

int AgHudVote::MsgFunc_Vote(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iVoteStatus = (VoteStatus)READ_BYTE();
	m_iFor = READ_BYTE();
	m_iAgainst = READ_BYTE();
	m_iUndecided = READ_BYTE();
	V_strcpy_safe(m_szVote, READ_STRING());
	V_strcpy_safe(m_szValue, READ_STRING());
	V_strcpy_safe(m_szCalled, READ_STRING());

	m_flTurnoff = gHUD.m_flTime + 4; // Hold for 4 seconds.

	if (m_iVoteStatus != VoteStatus::NotRunning)
		m_iFlags |= HUD_ACTIVE;
	else
		m_iFlags &= ~HUD_ACTIVE;

	return 1;
}
