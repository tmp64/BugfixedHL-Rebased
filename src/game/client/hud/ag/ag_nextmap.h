// Martin Webrant (BulliT)

#ifndef HUD_AG_NEXTMAP_H
#define HUD_AG_NEXTMAP_H
#include "hud/base.h"

class AgHudNextmap : public CHudElemBase<AgHudNextmap>
{
public:
	AgHudNextmap()
	    : m_flTurnoff(0)
	{
	}

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

	int MsgFunc_Nextmap(const char *pszName, int iSize, void *pbuf);

private:
	float m_flTurnoff;
	char m_szNextmap[32];
};

#endif
