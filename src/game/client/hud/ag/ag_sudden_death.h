// Martin Webrant (BulliT)

#ifndef HUD_AG_SUDDEN_DEATH_H
#define HUD_AG_SUDDEN_DEATH_H
#include "hud/base.h"

class AgHudSuddenDeath : public CHudElemBase<AgHudSuddenDeath>
{
public:
	AgHudSuddenDeath()
	    : m_iSuddenDeath(0)
	{
	}

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

	int MsgFunc_SuddenDeath(const char *pszName, int iSize, void *pbuf);

private:
	int m_iSuddenDeath;
};

#endif
