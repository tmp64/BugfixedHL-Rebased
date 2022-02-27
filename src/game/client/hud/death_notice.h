#ifndef HUD_DEATH_NOTICE_H
#define HUD_DEATH_NOTICE_H
#include "base.h"

class CHudDeathNotice : public CHudElemBase<CHudDeathNotice>
{
public:
	void Init();
	void InitHudData();
	void VidInit();
	void Draw(float flTime);
	void Think();
	int MsgFunc_DeathMsg(const char *pszName, int iSize, void *pbuf);

private:
	int m_HUD_d_skull; // sprite index of skull icon
};

#endif
