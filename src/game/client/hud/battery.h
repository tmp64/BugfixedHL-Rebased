#ifndef HUD_BATTERY_H
#define HUD_BATTERY_H
#include "base.h"

class CHudBattery : public CHudElemBase<CHudBattery>
{
public:
	void Init();
	void VidInit();
	void Draw(float flTime);
	int MsgFunc_Battery(const char *pszName, int iSize, void *pbuf);

private:
	HSPRITE m_hSprite1;
	HSPRITE m_hSprite2;
	wrect_t m_rc1;
	wrect_t m_rc2;
	int m_iBat;
	float m_fFade;
	int m_iHeight; // width of the battery innards
};

#endif
