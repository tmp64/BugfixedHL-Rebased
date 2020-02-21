#ifndef HUD_TRAIN_H
#define HUD_TRAIN_H
#include "base.h"

class CHudTrain : public CHudElemBase<CHudTrain>
{
public:
	void Init();
	void VidInit();
	void Draw(float flTime);
	int MsgFunc_Train(const char *pszName, int iSize, void *pbuf);

private:
	HSPRITE m_hSprite;
	int m_iPos;
};

#endif
