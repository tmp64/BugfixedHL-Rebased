//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_CHAT_H
#define HUD_CHAT_H

#include "base.h"
#include <vgui/KeyCode.h>
#include <vgui_controls/Panel.h>
#include "vgui_controls/Frame.h"
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <KeyValues.h>

class CHudChatInputLine;
class CHudChatEntry;

namespace vgui2
{
class IScheme;
};

#define CHATLINE_NUM_FLASHES 8.0f
#define CHATLINE_FLASH_TIME  5.0f
#define CHATLINE_FADE_TIME   1.0f

#define CHAT_HISTORY_FADE_TIME      0.25f
#define CHAT_HISTORY_IDLE_TIME      15.0f
#define CHAT_HISTORY_IDLE_FADE_TIME 2.5f
#define CHAT_HISTORY_ALPHA          64

/**
 * Maximum length of a chat input string in bytes that can be sent in one "say_team" command
 */
constexpr int MAX_CHAT_STRING_LEN = 101;

/**
 * Maximum number of wide chars that can be entered in the chat input.
 */
constexpr int MAX_CHAT_INPUT_STRING_LEN = 256;

extern Color g_SdkColorBlue;
extern Color g_SdkColorRed;
extern Color g_SdkColorGreen;
extern Color g_SdkColorDarkGreen;
extern Color g_SdkColorYellow;
extern Color g_SdkColorGrey;

// Message mode types
enum
{
	MM_NONE = 0,
	MM_SAY,
	MM_SAY_TEAM,
};

//-----------------------------------------------------------------------------
enum TextColor
{
	COLOR_NORMAL = 1,
	COLOR_PLAYERNAME = 2,

	// These are not used in HL1, were not tested and may not be handled correctly
	COLOR_PLAYERNAME_SOURCE = 3,
	COLOR_LOCATION = 4,
	COLOR_ACHIEVEMENT = 5,
	COLOR_CUSTOM = 6, // Will use the most recently SetCustomColor()
	COLOR_HEXCODE = 7, // Reads the color from the next six characters
	COLOR_HEXCODE_ALPHA = 8, // Reads the color and alpha from the next eight characters
	COLOR_MAX
};

//--------------------------------------------------------------------------------------------------------------
struct TextRange
{
	TextRange() { preserveAlpha = false; }
	int start;
	int end;
	Color color;
	bool preserveAlpha;
};

void StripEndNewlineFromString(char *str);
void StripEndNewlineFromString(wchar_t *str);

char *ConvertCRtoNL(char *str);
wchar_t *ConvertCRtoNL(wchar_t *str);
char *RemoveColorMarkup(char *str);

//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a wide string
 */
inline wchar_t *CloneWString(const wchar_t *str)
{
	const int nLen = V_wcslen(str) + 1;
	wchar_t *cloneStr = new wchar_t[nLen];
	const int nSize = nLen * sizeof(wchar_t);
	V_wcsncpy(cloneStr, str, nSize);
	return cloneStr;
}

//-----------------------------------------------------------------------------
// Purpose: An output/display line of the chat interface
//-----------------------------------------------------------------------------
class CHudChatLine : public vgui2::RichText
{
	typedef vgui2::RichText BaseClass;

public:
	CHudChatLine(vgui2::Panel *parent, const char *panelName);
	~CHudChatLine();

	void SetExpireTime(void);

	bool IsReadyToExpire(void);

	void Expire(void);

	float GetStartTime(void);

	int GetCount(void);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	vgui2::HFont GetFont() { return m_hFont; }

	Color GetTextColor(void) { return m_clrText; }
	void SetNameLength(int iLength) { m_iNameLength = iLength; }
	void SetNameColor(Color cColor) { m_clrNameColor = cColor; }

	virtual void PerformFadeout(void);
	virtual void InsertAndColorizeText(wchar_t *buf, int clientIndex);
	virtual void Colorize(int alpha = 255); ///< Re-inserts the text in the appropriate colors at the given alpha

	void SetNameStart(int iStart) { m_iNameStart = iStart; }

protected:
	int m_iNameLength;
	vgui2::HFont m_hFont;

	Color m_clrText;
	Color m_clrNameColor;

	float m_flExpireTime;

	CUtlVector<TextRange> m_textRanges;
	wchar_t *m_text = nullptr;

	int m_iNameStart;

private:
	float m_flStartTime;
	int m_nCount;

	vgui2::HFont m_hFontMarlett;

private:
	CHudChatLine(const CHudChatLine &); // not defined, not accessible
};

class CHudChatHistory : public vgui2::RichText
{
	DECLARE_CLASS_SIMPLE(CHudChatHistory, RichText);

public:
	CHudChatHistory(vgui2::Panel *pParent, const char *panelName);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void OnKeyCodeTyped(vgui2::KeyCode code) override;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CHudChat : public CHudElemBase<CHudChat>, public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CHudChat, EditablePanel);

public:
	enum
	{
		CHAT_INTERFACE_LINES = 6,
		MAX_CHARS_PER_LINE = 128
	};

	CHudChat();

	virtual void CreateChatInputLine(void);
	virtual void CreateChatLines(void);

	virtual void Init(void);

	virtual void InitHUDData();
	void LevelShutdown(void);

	virtual void Printf(const char *fmt, ...);
	virtual void ChatPrintf(int iPlayerIndex, const char *fmt, ...);

	virtual void StartMessageMode(int iMessageModeType);
	virtual void StopMessageMode(void);
	void Send(void);

	MESSAGE_FUNC(OnChatEntrySend, "ChatEntrySend");
	MESSAGE_FUNC(OnChatEntryStopMessageMode, "ChatEntryStopMessageMode");

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void Paint(void);
	virtual void OnTick(void);
	virtual void Reset();
#ifdef _XBOX
	virtual bool ShouldDraw();
#endif
	vgui2::Panel *GetInputPanel(void);

	static int m_nLineCounter;

	virtual int GetChatInputOffset(void);

	CHudChatHistory *GetChatHistory();

	void FadeChatHistory();
	float m_flHistoryFadeTime;
	float m_flHistoryIdleTime;

	CHudChatInputLine *GetChatInput(void) { return m_pChatInput; }

	//-----------------------------------------------------------------------------
	virtual Color GetDefaultTextColor(void);
	virtual Color GetTextColorForClient(TextColor colorNum, int clientIndex);
	virtual Color GetClientColor(int clientIndex);

	int GetMessageMode(void) { return m_nMessageMode; }

	void SetCustomColor(Color colNew) { m_ColorCustom = colNew; }
	void SetCustomColor(const char *pszColorName);

protected:
	CHudChatLine *FindUnusedChatLine(void);

	CHudChatInputLine *m_pChatInput = nullptr;
	CHudChatLine *m_ChatLine = nullptr;
	int m_iFontHeight;

	CHudChatHistory *m_pChatHistory = nullptr;

	Color m_ColorCustom;

	int MsgFunc_SayText(const char *pszName, int iSize, void *pbuf);

private:
	void Clear(void);

	int ComputeBreakChar(int width, const char *text, int textlen);

	int m_nMessageMode;

	int m_nVisibleHeight;

	vgui2::HFont m_hChatFont;
	Color m_DefTextColor;
	Color m_ConsoleMsgColor;
	const char *m_pszChatSoundPath = nullptr;

public:
	// memory handling, uses calloc so members are zero'd out on instantiation
	void *operator new(size_t stAllocateBlock)
	{
		Assert(stAllocateBlock != 0);
		void *pMem = ::operator new(stAllocateBlock);
		memset(pMem, 0, stAllocateBlock);
		return pMem;
	}

	void operator delete(void *pMem)
	{
#if defined(_DEBUG)
		int size = _msize(pMem);
		memset(pMem, 0xcd, size);
#endif
		::operator delete(pMem);
	}
};

class CHudChatEntry : public vgui2::TextEntry
{
	typedef vgui2::TextEntry BaseClass;

public:
	CHudChatEntry(vgui2::Panel *parent, char const *panelName, vgui2::Panel *pChat)
	    : BaseClass(parent, panelName)
	{
		SetCatchEnterKey(true);
		SetAllowNonAsciiCharacters(true);
		SetDrawLanguageIDAtLeft(true);
		m_pHudChat = pChat;
	}

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);

		SetPaintBorderEnabled(false);
	}

	virtual void OnKeyCodeTyped(vgui2::KeyCode code);

private:
	vgui2::Panel *m_pHudChat = nullptr;
};

//-----------------------------------------------------------------------------
// Purpose: The prompt and text entry area for chat messages
//-----------------------------------------------------------------------------
class CHudChatInputLine : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CHudChatInputLine(vgui2::Panel *parent, char const *panelName);

	void SetPrompt(const wchar_t *prompt);
	void ClearEntry(void);
	void SetEntry(const wchar_t *entry);
	void GetMessageText(wchar_t *buffer, int buffersizebytes);

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	vgui2::Panel *GetInputPanel(void);
	virtual vgui2::VPANEL GetCurrentKeyFocus() { return m_pInput->GetVPanel(); }

	virtual void Paint()
	{
		BaseClass::Paint();
	}

	vgui2::Label *GetPrompt(void) { return m_pPrompt; }

protected:
	vgui2::Label *m_pPrompt = nullptr;
	CHudChatEntry *m_pInput = nullptr;
};

#endif // HUD_BASECHAT_H
