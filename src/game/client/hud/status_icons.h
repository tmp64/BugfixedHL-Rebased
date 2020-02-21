#ifndef HUD_STATUS_ICONS_H
#define HUD_STATUS_ICONS_H
#include "base.h"

class CHudStatusIcons : public CHudElemBase<CHudStatusIcons>
{
public:
	void Init();
	void VidInit();
	void Reset();
	void Draw(float flTime);
	int MsgFunc_StatusIcon(const char *pszName, int iSize, void *pbuf);

	enum
	{
		MAX_ICONSPRITENAME_LENGTH = MAX_SPRITE_NAME_LENGTH,
		MAX_ICONSPRITES = 4,
	};

	//had to make these public so CHud could access them (to enable concussion icon)
	//could use a friend declaration instead...
	void EnableIcon(char *pszIconName, unsigned char red, unsigned char green, unsigned char blue);
	void DisableIcon(char *pszIconName);

private:
	typedef struct
	{
		char szSpriteName[MAX_ICONSPRITENAME_LENGTH];
		HSPRITE spr;
		wrect_t rc;
		unsigned char r, g, b;
	} icon_sprite_t;

	icon_sprite_t m_IconList[MAX_ICONSPRITES];
};

#endif
