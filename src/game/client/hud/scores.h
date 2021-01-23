#ifndef HUD_SCORES_H
#define HUD_SCORES_H
#include <array>
#include "base.h"

struct HudScoresData
{
	wchar_t wszScore[64];
	Color color;
};

class CHudScores : public CHudElemBase<CHudScores>
{
public:
	void Init();
	void VidInit();
	void Draw(float flTime);

private:
	struct TeamData
	{
		int iFrags = 0;
		int iDeaths = 0;
		int iPlayerCount = 0;
	};

	std::array<TeamData, MAX_TEAMS + 1> m_TeamData;
	std::array<int, MAX_TEAMS + 1> m_SortedTeamIDs;
	std::array<int, MAX_PLAYERS + 1> m_SortedPlayerIDs;
	HudScoresData m_ScoresData[MAX_PLAYERS] = {};
	int m_iLines = 0;
	int m_iOverLay = 0;
	float m_flScoreBoardLastUpdated = 0;

	void Update();
};

#endif
