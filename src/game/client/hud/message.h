#ifndef HUD_MESSAGE_H
#define HUD_MESSAGE_H
#include <string>
#include "base.h"

constexpr int MAX_HUD_STRING = 80;
constexpr int MAX_HUD_MESSAGES = 16;

// Space correction between text lines in hud messages in pixels
constexpr int ADJUST_MESSAGE = 0;

struct message_parms_t
{
	client_textmessage_t *pMessage;
	float time;
	int x, y;
	int totalWidth, totalHeight;
	int width;
	int lines;
	int lineLength;
	int length;
	int r, g, b;
	int currentChar;
	int fadeBlend;
	float charTime;
	float fadeTime;
};

class CHudMessage : public CHudElemBase<CHudMessage>
{
public:
	void Init();
	void VidInit();
	void Draw(float flTime);
	void Think();
	int MsgFunc_HudText(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_GameTitle(const char *pszName, int iSize, void *pbuf);

	float FadeBlend(float fadein, float fadeout, float hold, float localTime);
	int XPosition(float x, int width, int lineWidth);
	int YPosition(float y, int height);

	void MessageAdd(const char *pName, float time);
	void MessageAdd(client_textmessage_t *newMessage);
	void MessageDrawScan(client_textmessage_t *pMessage, float time, const std::wstring &wstr);
	void MessageScanStart();
	void MessageScanNextChar(Color srcColor);
	void Reset();

private:
	client_textmessage_t *m_pMessages[MAX_HUD_MESSAGES];
	float m_startTime[MAX_HUD_MESSAGES];
	message_parms_t m_parms;
	float m_gameTitleTime;
	client_textmessage_t *m_pGameTitle;
	std::wstring m_sMessageStrings[MAX_HUD_MESSAGES];

	int m_HUD_title_life;
	int m_HUD_title_half;
	bool m_bEndAfterMessage = false;

	void CStrToWide(const char *pString, std::wstring &wstr);
};

#endif
