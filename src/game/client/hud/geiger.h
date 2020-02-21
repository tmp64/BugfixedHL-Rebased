#ifndef HUD_GEIGER_H
#define HUD_GEIGER_H
#include "base.h"

class CHudGeiger : public CHudElemBase<CHudGeiger>
{
public:
	void Init();
	void VidInit();
	void Draw(float flTime);
	int MsgFunc_Geiger(const char *pszName, int iSize, void *pbuf);

private:
	int m_iGeigerRange;
};

#endif
