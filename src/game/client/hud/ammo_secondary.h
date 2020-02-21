#ifndef HUD_AMMO_SECONDARY_H
#define HUD_AMMO_SECONDARY_H
#include "base.h"

class CHudAmmoSecondary : public CHudElemBase<CHudAmmoSecondary>
{
public:
	void Init();
	void VidInit();
	void Reset();
	void Draw(float flTime);

	int MsgFunc_SecAmmoVal(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_SecAmmoIcon(const char *pszName, int iSize, void *pbuf);

private:
	enum
	{
		MAX_SEC_AMMO_VALUES = 4
	};

	int m_HUD_ammoicon; // sprite indices
	int m_iAmmoAmounts[MAX_SEC_AMMO_VALUES];
	float m_fFade;
};

#endif
