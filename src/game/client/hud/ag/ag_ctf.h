// Martin Webrant (BulliT)

#ifndef HUD_AG_CTF_H
#define HUD_AG_CTF_H
#include "hud/base.h"

class AgHudCTF : public CHudElemBase<AgHudCTF>
{
public:
	AgHudCTF()
	    : m_iFlagStatus1(0)
	    , m_iFlagStatus2(0)
	    , m_pCvarClCtfVolume(NULL)
	{
	}

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
	void Reset() override;

	int MsgFunc_CTF(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_CTFSound(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_CTFFlag(const char *pszName, int iSize, void *pbuf);

private:
	enum enumFlagStatus
	{
		Off = -1,
		Home = 0,
		Stolen = 1,
		Lost = 2,
		Carry = 3
	};

	typedef struct
	{
		HSPRITE spr;
		wrect_t rc;
	} icon_flagstatus_t;

	icon_flagstatus_t m_IconFlagStatus[4];
	int m_iFlagStatus1;
	int m_iFlagStatus2;

	cvar_t *m_pCvarClCtfVolume;
};

extern int g_iPlayerFlag1;
extern int g_iPlayerFlag2;

#endif
