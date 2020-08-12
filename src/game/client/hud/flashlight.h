#ifndef HUD_FLAHSLIGHT_H
#define HUD_FLAHSLIGHT_H
#include "base.h"

class CHudFlashlight : public CHudElemBase<CHudFlashlight>
{
public:
	void Init();
	void VidInit();
	void Draw(float flTime);
	void Reset();
	int MsgFunc_Flashlight(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_FlashBat(const char *pszName, int iSize, void *pbuf);

private:
	HSPRITE m_hSprite1;
	HSPRITE m_hSprite2;
	HSPRITE m_hBeam;
	wrect_t m_rc1;
	wrect_t m_rc2;
	wrect_t m_rcBeam;
	float m_flBat;
	int m_iBat;
	int m_fOn;
	float m_fFade;
	int m_iWidth; // width of the battery innards
};

#endif
