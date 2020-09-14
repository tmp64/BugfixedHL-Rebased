// Martin Webrant (BulliT)

#ifndef HUD_AG_COUNTDOWN_H
#define HUD_AG_COUNTDOWN_H
#include "hud/base.h"

class AgHudCountdown : public CHudElemBase<AgHudCountdown>
{
public:
	AgHudCountdown()
	    : m_btCountdown(0)
	{
	}

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

	int MsgFunc_Countdown(const char *pszName, int iSize, void *pbuf);

private:
	char m_btCountdown;
	char m_szPlayer1[32];
	char m_szPlayer2[32];
};

#endif
