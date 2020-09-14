// Martin Webrant (BulliT)

#ifndef HUD_AG_TIMEOUT_H
#define HUD_AG_TIMEOUT_H
#include "hud/base.h"

class AgHudTimeout : public CHudElemBase<AgHudTimeout>
{
public:
	AgHudTimeout()
	    : m_State(0)
	    , m_iTime(0)
	{
	}

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

	int MsgFunc_Timeout(const char *pszName, int iSize, void *pbuf);

private:
	enum enumState
	{
		Inactive = 0,
		Called = 1,
		Pause = 2,
		Countdown = 3
	};

	int m_State;
	int m_iTime;
};

#endif
