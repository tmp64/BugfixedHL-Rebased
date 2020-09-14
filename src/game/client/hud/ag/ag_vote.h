// Martin Webrant (BulliT)

#ifndef HUD_AG_VOTE_H
#define HUD_AG_VOTE_H
#include "hud/base.h"

class AgHudVote : public CHudElemBase<AgHudVote>
{
public:
	AgHudVote();

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

	int MsgFunc_Vote(const char *pszName, int iSize, void *pbuf);

private:
	enum class VoteStatus
	{
		NotRunning = 0,
		Called = 1,
		Accepted = 2,
		Denied = 3,
	};

	float m_flTurnoff = 0;
	VoteStatus m_iVoteStatus = VoteStatus::NotRunning;
	int m_iFor = 0;
	int m_iAgainst = 0;
	int m_iUndecided = 0;
	char m_byVoteStatus = 0;
	char m_szVote[32];
	char m_szValue[32];
	char m_szCalled[32];
};

#endif
