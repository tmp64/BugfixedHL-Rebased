#ifndef HUD_SAY_TEXT_H
#define HUD_SAY_TEXT_H
#include "base.h"

class CHudSayText : public CHudElemBase<CHudSayText>
{
public:
	void Init();
	void InitHudData();
	void VidInit();
	void Draw(float flTime);
	int MsgFunc_SayText(const char *pszName, int iSize, void *pbuf);
	void SayTextPrint(const char *pszBuf, int iBufSize, int clientIndex = -1);
	void EnsureTextFitsInOneLineAndWrapIfHaveTo(int line);
	friend class CHudSpectator;

private:
	struct cvar_s *m_HUD_saytext;
	struct cvar_s *m_HUD_saytext_time;
};

#endif
