#ifndef HUD_TEXT_MESSAGE_H
#define HUD_TEXT_MESSAGE_H
#include "base.h"

class CHudTextMessage : public CHudElemBase<CHudTextMessage>
{
public:
	void Init();
	static char *LocaliseTextString(const char *msg, char *dst_buffer, int buffer_size);
	static char *BufferedLocaliseTextString(const char *msg);
	static char *LookupString(const char *msg_name, int *msg_dest = NULL);
	int MsgFunc_TextMsg(const char *pszName, int iSize, void *pbuf);
};

#endif
