// Martin Webrant (BulliT)

#ifndef HUD_AG_LONGJUMP_H
#define HUD_AG_LONGJUMP_H
#include "hud/base.h"

class AgHudLongjump : public CHudElemBase<AgHudLongjump>
{
public:
	AgHudLongjump()
	    : m_flTurnoff(0)
	{
	}

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

	int MsgFunc_Longjump(const char *pszName, int iSize, void *pbuf);

private:
	float m_flTurnoff;
};

#endif
