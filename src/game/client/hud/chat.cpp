//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include <ctime>
#include <IBaseUI.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>
#include "client_vgui.h"
#include "vgui/client_viewport.h"
#include "hud.h"
#include "cl_util.h"
#include "chat.h"
#include "parsemsg.h"
#include "cl_voice_status.h"

ConVar hud_saytext_time("hud_saytext_time", "12", 0);
ConVar cl_mute_all_comms("cl_mute_all_comms", "1", FCVAR_ARCHIVE, "If 1, then all communications from a player will be blocked when that player is muted, including chat messages.");

Color g_SdkColorBlue(153, 204, 255, 255);
Color g_SdkColorRed(255, 63, 63, 255);
Color g_SdkColorGreen(153, 255, 153, 255);
Color g_SdkColorDarkGreen(64, 255, 64, 255);
Color g_SdkColorYellow(255, 178, 0, 255);
Color g_SdkColorGrey(204, 204, 204, 255);

// removes all color markup characters, so Msg can deal with the string properly
// returns a pointer to str
char *RemoveColorMarkup(char *str)
{
	char *out = str;
	for (char *in = str; *in != 0; ++in)
	{
		if (*in > 0 && *in < COLOR_MAX)
		{
			if (*in == COLOR_HEXCODE || *in == COLOR_HEXCODE_ALPHA)
			{
				// skip the next six or eight characters
				const int nSkip = (*in == COLOR_HEXCODE ? 6 : 8);
				for (int i = 0; i < nSkip && *in != 0; i++)
				{
					++in;
				}

				// if we reached the end of the string first, then back up
				if (*in == 0)
				{
					--in;
				}
			}

			continue;
		}
		*out = *in;
		++out;
	}
	*out = 0;

	return str;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
wchar_t *ConvertCRtoNL(wchar_t *str)
{
	for (wchar_t *ch = str; *ch != 0; ch++)
		if (*ch == L'\r')
			*ch = L'\n';
	return str;
}

void StripEndNewlineFromString(wchar_t *str)
{
	int s = wcslen(str) - 1;
	if (s >= 0)
	{
		if (str[s] == L'\n' || str[s] == L'\r')
			str[s] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *parent -
//			*panelName -
//-----------------------------------------------------------------------------
CHudChatLine::CHudChatLine(vgui2::Panel *parent, const char *panelName)
    : vgui2::RichText(parent, panelName)
{
	m_hFont = m_hFontMarlett = 0;
	m_flExpireTime = 0.0f;
	m_flStartTime = 0.0f;
	m_iNameLength = 0;
	m_text = NULL;

	SetPaintBackgroundEnabled(true);

	SetVerticalScrollbar(false);
}

CHudChatLine::~CHudChatLine()
{
	if (m_text)
	{
		delete[] m_text;
		m_text = NULL;
	}
}

void CHudChatLine::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont("Default");

#ifdef HL1_CLIENT_DLL
	SetBgColor(Color(0, 0, 0, 0));
	SetFgColor(Color(0, 0, 0, 0));

	SetBorder(NULL);
#else
	SetBgColor(Color(0, 0, 0, 100));
#endif

	m_hFontMarlett = pScheme->GetFont("Marlett");

	m_clrText = pScheme->GetColor("FgColor", GetFgColor());
	SetFont(m_hFont);
}

void CHudChatLine::PerformFadeout(void)
{
	// Flash + Extra bright when new
	float curtime = gEngfuncs.GetAbsoluteTime();

	int lr = m_clrText[0];
	int lg = m_clrText[1];
	int lb = m_clrText[2];

	if (curtime >= m_flStartTime && curtime < m_flStartTime + CHATLINE_FLASH_TIME)
	{
		float frac1 = (curtime - m_flStartTime) / CHATLINE_FLASH_TIME;
		float frac = frac1;

		frac *= CHATLINE_NUM_FLASHES;
		frac *= 2 * M_PI;

		frac = cos(frac);

		frac = clamp(frac, 0.0f, 1.0f);

		frac *= (1.0f - frac1);

		int r = lr, g = lg, b = lb;

		r = r + (255 - r) * frac;
		g = g + (255 - g) * frac;
		b = b + (255 - b) * frac;

		// Draw a right facing triangle in red, faded out over time
		int alpha = 63 + 192 * (1.0f - frac1);
		alpha = clamp(alpha, 0, 255);

		wchar_t wbuf[4096];
		GetText(0, wbuf, sizeof(wbuf));

		SetText("");

		InsertColorChange(Color(r, g, b, 255));
		InsertString(wbuf);
	}
	else if (curtime <= m_flExpireTime && curtime > m_flExpireTime - CHATLINE_FADE_TIME)
	{
		float frac = (m_flExpireTime - curtime) / CHATLINE_FADE_TIME;

		int alpha = frac * 255;
		alpha = clamp(alpha, 0, 255);

		wchar_t wbuf[4096];
		GetText(0, wbuf, sizeof(wbuf));

		SetText("");

		InsertColorChange(Color(lr * frac, lg * frac, lb * frac, alpha));
		InsertString(wbuf);
	}
	else
	{
		wchar_t wbuf[4096];
		GetText(0, wbuf, sizeof(wbuf));

		SetText("");

		InsertColorChange(Color(lr, lg, lb, 255));
		InsertString(wbuf);
	}

	OnThink();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : time -
//-----------------------------------------------------------------------------
void CHudChatLine::SetExpireTime(void)
{
	m_flStartTime = gEngfuncs.GetAbsoluteTime();
	m_flExpireTime = m_flStartTime + hud_saytext_time.GetFloat();
	m_nCount = CHudChat::m_nLineCounter++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CHudChatLine::GetCount(void)
{
	return m_nCount;
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudChatLine::IsReadyToExpire(void)
{
	if (gEngfuncs.GetAbsoluteTime() >= m_flExpireTime)
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : float
//-----------------------------------------------------------------------------
float CHudChatLine::GetStartTime(void)
{
	return m_flStartTime;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChatLine::Expire(void)
{
	SetVisible(false);

	// Spit out label text now
	//	char text[ 256 ];
	//	GetText( text, 256 );

	//	Msg( "%s\n", text );
}

//-----------------------------------------------------------------------------
// Purpose: The prompt and text entry area for chat messages
//-----------------------------------------------------------------------------
CHudChatInputLine::CHudChatInputLine(vgui2::Panel *parent, char const *panelName)
    : vgui2::Panel(parent, panelName)
{
	SetMouseInputEnabled(false);

	m_pPrompt = new vgui2::Label(this, "ChatInputPrompt", L"Enter text:");

	m_pInput = new CHudChatEntry(this, "ChatInput", parent);
	m_pInput->SetMaximumCharCount(127);
}

void CHudChatInputLine::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// FIXME:  Outline
	vgui2::HFont hFont = pScheme->GetFont("ChatFont");

	m_pPrompt->SetFont(hFont);
	m_pInput->SetFont(hFont);

	m_pInput->SetFgColor(pScheme->GetColor("Chat.TypingText", pScheme->GetColor("Panel.FgColor", Color(255, 255, 255, 255))));

	SetPaintBackgroundEnabled(true);
	m_pPrompt->SetPaintBackgroundEnabled(true);
	m_pPrompt->SetContentAlignment(vgui2::Label::a_west);
	m_pPrompt->SetTextInset(2, 0);

	m_pInput->SetMouseInputEnabled(true);

#ifdef HL1_CLIENT_DLL
	m_pInput->SetBgColor(Color(255, 255, 255, 0));
#endif

	SetBgColor(Color(0, 0, 0, 0));
}

void CHudChatInputLine::SetPrompt(const wchar_t *prompt)
{
	Assert(m_pPrompt);
	m_pPrompt->SetText(prompt);
	InvalidateLayout();
}

void CHudChatInputLine::ClearEntry(void)
{
	Assert(m_pInput);
	SetEntry(L"");
}

void CHudChatInputLine::SetEntry(const wchar_t *entry)
{
	Assert(m_pInput);
	Assert(entry);

	m_pInput->SetText(entry);
}

void CHudChatInputLine::GetMessageText(wchar_t *buffer, int buffersizebytes)
{
	m_pInput->GetText(buffer, buffersizebytes);
}

void CHudChatInputLine::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize(wide, tall);

	int w, h;
	m_pPrompt->GetContentSize(w, h);
	m_pPrompt->SetBounds(0, 0, w, tall);

	m_pInput->SetBounds(w + 2, 0, wide - w - 2, tall);
}

vgui2::Panel *CHudChatInputLine::GetInputPanel(void)
{
	return m_pInput;
}

CHudChatHistory::CHudChatHistory(vgui2::Panel *pParent, const char *panelName)
    : BaseClass(pParent, "HudChatHistory")
{
	vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/ChatScheme.res", "ChatScheme");
	SetScheme(scheme);

	InsertFade(-1, -1);
}

void CHudChatHistory::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetFont(pScheme->GetFont("ChatFont"));
	SetAlpha(255);
}

int CHudChat::m_nLineCounter = 1;

//-----------------------------------------------------------------------------
// Purpose: Text chat input/output hud element
//-----------------------------------------------------------------------------
CHudChat::CHudChat()
    : CHudElemBase()
    , BaseClass(NULL, "HudChat")
{
	SetParent(g_pViewport);
	SetProportional(true);

	vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/ChatScheme.res", "ChatScheme");
	SetScheme(scheme);
	SetPaintBackgroundEnabled(true);

	g_pVGuiLocalize->AddFile(g_pFullFileSystem, VGUI2_ROOT_DIR "resource/language/chat_%language%.txt");

	m_nMessageMode = 0;

	vgui2::ivgui()->AddTickSignal(GetVPanel());

	// (We don't actually want input until they bring up the chat line).
	MakePopup();
	SetZPos(-30);

	m_pChatHistory = new CHudChatHistory(this, "HudChatHistory");

	CreateChatLines();
	CreateChatInputLine();
}

DEFINE_HUD_ELEM(CHudChat);

void CHudChat::CreateChatInputLine(void)
{
	m_pChatInput = new CHudChatInputLine(this, "ChatInputLine");
	m_pChatInput->SetVisible(false);

	if (GetChatHistory())
	{
		GetChatHistory()->SetMaximumCharCount(127 * 100);
		GetChatHistory()->SetVisible(true);
	}
}

void CHudChat::CreateChatLines(void)
{
	m_ChatLine = new CHudChatLine(this, "ChatLine1");
	m_ChatLine->SetVisible(false);
}

void CHudChat::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	LoadControlSettings(VGUI2_ROOT_DIR "resource/Chat.res");

	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(2);
	SetPaintBorderEnabled(true);
	SetPaintBackgroundEnabled(true);

	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	m_nVisibleHeight = 0;

#ifdef HL1_CLIENT_DLL
	SetBgColor(Color(0, 0, 0, 0));
	SetFgColor(Color(0, 0, 0, 0));
#endif

	Color cColor = pScheme->GetColor("ChatBgColor", GetBgColor());
	SetBgColor(Color(cColor.r(), cColor.g(), cColor.b(), CHAT_HISTORY_ALPHA));

	GetChatHistory()->SetVerticalScrollbar(false);

	m_pChatInput->SetVisible(false);
	m_ConsoleMsgColor = pScheme->GetColor("ChatConsoleMsg", Color(30, 230, 50, 255));
}

void CHudChat::Reset(void)
{
}

void CHudChat::Paint(void)
{
	if (m_nVisibleHeight == 0)
		return;
}

CHudChatHistory *CHudChat::GetChatHistory(void)
{
	return m_pChatHistory;
}

static void MessageModeVGUI2()
{
	if (gEngfuncs.GetMaxClients() == 1)
		return;
	CHudChat::Get()->StartMessageMode(MM_SAY);
}

static void MessageMode2VGUI2()
{
	if (gEngfuncs.GetMaxClients() == 1)
		return;
	CHudChat::Get()->StartMessageMode(MM_SAY_TEAM);
}

void CHudChat::Init(void)
{
	BaseHudClass::Init();

	HookMessage<&CHudChat::MsgFunc_SayText>("SayText");

	// Hook messagemode and messagemode2
	cmd_function_t *item = gEngfuncs.GetFirstCmdFunctionHandle();
	cmd_function_t *msgMode = nullptr, *msgMode2 = nullptr;

	while (item)
	{
		if (!strcmp(item->name, "messagemode"))
			msgMode = item;
		else if (!strcmp(item->name, "messagemode2"))
			msgMode2 = item;
		item = item->next;
	}

	if (!msgMode || !msgMode2)
	{
		ConPrintf(ConColor::Red, "Failed to hook messagemode and messagemode2\n");
	}
	else
	{
		msgMode->function = MessageModeVGUI2;
		msgMode2->function = MessageMode2VGUI2;
	}
}

static int __cdecl SortLines(void const *line1, void const *line2)
{
	CHudChatLine *l1 = *(CHudChatLine **)line1;
	CHudChatLine *l2 = *(CHudChatLine **)line2;

	// Invisible at bottom
	if (l1->IsVisible() && !l2->IsVisible())
		return -1;
	else if (!l1->IsVisible() && l2->IsVisible())
		return 1;

	// Oldest start time at top
	if (l1->GetStartTime() < l2->GetStartTime())
		return -1;
	else if (l1->GetStartTime() > l2->GetStartTime())
		return 1;

	// Otherwise, compare counter
	if (l1->GetCount() < l2->GetCount())
		return -1;
	else if (l1->GetCount() > l2->GetCount())
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Allow inheriting classes to change this spacing behavior
//-----------------------------------------------------------------------------
int CHudChat::GetChatInputOffset(void)
{
	if (m_pChatInput->IsVisible())
	{
		return m_iFontHeight;
	}
	else
		return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Do respositioning here to avoid latency due to repositioning of vgui
//  voice manager icon panel
//-----------------------------------------------------------------------------
void CHudChat::OnTick(void)
{
	m_nVisibleHeight = 0;

	CHudChatLine *line = m_ChatLine;

	if (line)
	{
		vgui2::HFont font = line->GetFont();
		m_iFontHeight = vgui2::surface()->GetFontTall(font) + 2;

		// Put input area at bottom

		int iChatX, iChatY, iChatW, iChatH;
		int iInputX, iInputY, iInputW, iInputH;

		m_pChatInput->GetBounds(iInputX, iInputY, iInputW, iInputH);
		GetBounds(iChatX, iChatY, iChatW, iChatH);

		m_pChatInput->SetBounds(iInputX, iChatH - (m_iFontHeight * 1.75), iInputW, m_iFontHeight);

		//Resize the History Panel so it fits more lines depending on the screen resolution.
		int iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH;

		GetChatHistory()->GetBounds(iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH);

		iChatHistoryH = (iChatH - (m_iFontHeight * 2.25)) - iChatHistoryY;

		GetChatHistory()->SetBounds(iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH);
	}

	FadeChatHistory();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : width -
//			*text -
//			textlen -
// Output : int
//-----------------------------------------------------------------------------
int CHudChat::ComputeBreakChar(int width, const char *text, int textlen)
{
	CHudChatLine *line = m_ChatLine;
	vgui2::HFont font = line->GetFont();

	int currentlen = 0;
	int lastbreak = textlen;
	for (int i = 0; i < textlen; i++)
	{
		char ch = text[i];

		if (ch <= 32)
		{
			lastbreak = i;
		}

		wchar_t wch[2];

		g_pVGuiLocalize->ConvertANSIToUnicode(&ch, wch, sizeof(wch));

		int a, b, c;

		vgui2::surface()->GetCharABCwide(font, wch[0], a, b, c);
		currentlen += a + b + c;

		if (currentlen >= width)
		{
			// If we haven't found a whitespace char to break on before getting
			//  to the end, but it's still too long, break on the character just before
			//  this one
			if (lastbreak == textlen)
			{
				lastbreak = max(0, i - 1);
			}
			break;
		}
	}

	if (currentlen >= width)
	{
		return lastbreak;
	}
	return textlen;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CHudChat::Printf(const char *fmt, ...)
{
	va_list marker;
	char msg[4096];

	va_start(marker, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, marker);
	va_end(marker);

	ChatPrintf(0, "%s", msg);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::StartMessageMode(int iMessageModeType)
{
	m_nMessageMode = iMessageModeType;

	m_pChatInput->ClearEntry();

	const wchar_t *pszPrompt = (m_nMessageMode == MM_SAY) ? g_pVGuiLocalize->Find("#chat_say") : g_pVGuiLocalize->Find("#chat_say_team");
	if (pszPrompt)
	{
		m_pChatInput->SetPrompt(pszPrompt);
	}
	else
	{
		if (m_nMessageMode == MM_SAY)
		{
			m_pChatInput->SetPrompt(L"Say :");
		}
		else
		{
			m_pChatInput->SetPrompt(L"Say (TEAM) :");
		}
	}

	if (GetChatHistory())
	{
		GetChatHistory()->SetMouseInputEnabled(true);
		GetChatHistory()->SetKeyBoardInputEnabled(false);
		GetChatHistory()->SetVerticalScrollbar(true);
		GetChatHistory()->ResetAllFades(true); // FIXME
		GetChatHistory()->SetPaintBorderEnabled(true);
		GetChatHistory()->SetVisible(true);
	}

	vgui2::SETUP_PANEL(this);
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	m_pChatInput->SetVisible(true);
	vgui2::surface()->CalculateMouseVisible();
	m_pChatInput->RequestFocus();
	m_pChatInput->SetPaintBorderEnabled(true);
	m_pChatInput->SetMouseInputEnabled(true);

	//Place the mouse cursor near the text so people notice it.
	int x, y, w, h, chatx, chaty;
	GetPos(chatx, chaty);
	GetChatHistory()->GetBounds(x, y, w, h);
	vgui2::input()->SetCursorPos(chatx + x + (w / 2), chaty + y + (h / 2));

	m_flHistoryFadeTime = gEngfuncs.GetAbsoluteTime() + CHAT_HISTORY_FADE_TIME;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::StopMessageMode(void)
{
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	if (GetChatHistory())
	{
		GetChatHistory()->SetPaintBorderEnabled(false);
		GetChatHistory()->GotoTextEnd();
		GetChatHistory()->SetMouseInputEnabled(false);
		GetChatHistory()->SetVerticalScrollbar(false);
		GetChatHistory()->ResetAllFades(false, true, CHAT_HISTORY_FADE_TIME); // FIXME
		GetChatHistory()->SelectNoText();
	}

	//Clear the entry since we wont need it anymore.
	m_pChatInput->ClearEntry();
	m_pChatInput->SetVisible(false);
	m_pChatInput->SetKeyBoardInputEnabled(false);
	m_pChatInput->SetMouseInputEnabled(false);

	m_flHistoryFadeTime = gEngfuncs.GetAbsoluteTime() + CHAT_HISTORY_FADE_TIME;

	m_nMessageMode = MM_NONE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::OnChatEntrySend(void)
{
	Send();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::OnChatEntryStopMessageMode(void)
{
	StopMessageMode();
}

void CHudChat::FadeChatHistory(void)
{
	float frac = (m_flHistoryFadeTime - gEngfuncs.GetAbsoluteTime()) / CHAT_HISTORY_FADE_TIME;

	int alpha = frac * CHAT_HISTORY_ALPHA;
	alpha = clamp(alpha, 0, CHAT_HISTORY_ALPHA);

	if (alpha >= 0)
	{
		if (GetChatHistory())
		{
			if (IsMouseInputEnabled())
			{
				SetAlpha(255);
				GetChatHistory()->SetBgColor(Color(0, 0, 0, CHAT_HISTORY_ALPHA - alpha));
				m_pChatInput->GetPrompt()->SetAlpha((CHAT_HISTORY_ALPHA * 2) - alpha);
				m_pChatInput->GetInputPanel()->SetAlpha((CHAT_HISTORY_ALPHA * 2) - alpha);
				SetBgColor(Color(GetBgColor().r(), GetBgColor().g(), GetBgColor().b(), CHAT_HISTORY_ALPHA - alpha));
				SetPaintBackgroundEnabled(true);
				SetPaintBackgroundType(2);
			}
			else
			{
				GetChatHistory()->SetBgColor(Color(0, 0, 0, alpha));
				SetBgColor(Color(GetBgColor().r(), GetBgColor().g(), GetBgColor().b(), alpha));

				m_pChatInput->GetPrompt()->SetAlpha(alpha);
				m_pChatInput->GetInputPanel()->SetAlpha(alpha);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Color CHudChat::GetTextColorForClient(TextColor colorNum, int clientIndex)
{
	Color c;
	switch (colorNum)
	{
	case COLOR_CUSTOM:
		c = m_ColorCustom;
		break;

	case COLOR_PLAYERNAME:
		c = GetClientColor(clientIndex);
		break;

	case COLOR_LOCATION:
		c = g_SdkColorDarkGreen;
		break;

	case COLOR_ACHIEVEMENT:
	{
		vgui2::IScheme *pSourceScheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetScheme("SourceScheme"));
		if (pSourceScheme)
		{
			c = pSourceScheme->GetColor("SteamLightGreen", GetBgColor());
		}
		else
		{
			c = GetDefaultTextColor();
		}
	}
	break;

	default:
		c = GetDefaultTextColor();
	}

	return Color(c[0], c[1], c[2], 255);
}

//-----------------------------------------------------------------------------
void CHudChat::SetCustomColor(const char *pszColorName)
{
	vgui2::IScheme *pScheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetScheme("ClientScheme"));
	SetCustomColor(pScheme->GetColor(pszColorName, Color(255, 255, 255, 255)));
}

//-----------------------------------------------------------------------------
Color CHudChat::GetDefaultTextColor(void)
{
	return g_SdkColorYellow;
}

//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor(int clientIndex)
{
	if (clientIndex >= 1 && clientIndex <= MAX_PLAYERS)
		return g_pViewport->GetTeamColor(GetPlayerInfo(clientIndex)->Update()->GetTeamNumber());

	return g_pViewport->GetTeamColor(0);
}

//-----------------------------------------------------------------------------
// Purpose: Parses a line of text for color markup and inserts it via Colorize()
//-----------------------------------------------------------------------------
void CHudChatLine::InsertAndColorizeText(wchar_t *buf, int clientIndex)
{
	if (m_text)
	{
		delete[] m_text;
		m_text = NULL;
	}
	m_textRanges.RemoveAll();

	//m_text = CloneWString( buf );

	CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());

	if (pChat == NULL)
		return;

	// Parse colorcodes
	{
		wchar_t *buf2 = buf;
		int len = wcslen(buf2);
		m_text = new wchar_t[len + 1];

		wchar_t *str = m_text;

		{
			TextRange range;
			range.start = 0;
			range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
			range.end = len;
			m_textRanges.AddToTail(range);
		}
		int last_range_idx = 0;
		while (*buf2)
		{
			int pos = str - m_text;

			if (pos == m_iNameStart + m_iNameLength - 1 && m_textRanges[last_range_idx].end == pos - 1)
			{
				TextRange range;
				range.start = pos;
				range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
				range.end = len;

				last_range_idx = m_textRanges.Count();
				m_textRanges.AddToTail(range);
			}

			if (*buf2 == '^' && *(buf2 + 1) >= '0' && *(buf2 + 1) <= '9')
			{
				TextRange range;
				int idx = *(buf2 + 1) - '0';
				if (idx == 0 || idx == 9)
				{
					if (pos <= m_iNameStart + m_iNameLength)
						range.color = pChat->GetTextColorForClient(COLOR_PLAYERNAME, clientIndex);
					else
						range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
				}
				else
				{
					// FIXME: Colorcodes
					//int *clr = g_iColorsCodes[idx];
					int clr[] = { 255, 255, 255 };
					range.color = Color(clr[0], clr[1], clr[2]);
				}
				range.start = pos;
				range.end = len;

				m_textRanges[last_range_idx].end = pos;
				last_range_idx = m_textRanges.Count();
				m_textRanges.AddToTail(range);

				if (pos < m_iNameStart)
					m_iNameStart -= 2;
				else if (pos >= m_iNameStart && pos < m_iNameStart + m_iNameLength)
					m_iNameLength -= 2;

				buf2 += 2;
			}
			else if (*buf2 == 2) // COLOR_PLAYERNAME but for GoldSrc
			{
				TextRange range;
				range.start = pos;
				range.color = pChat->GetTextColorForClient(COLOR_PLAYERNAME, clientIndex);
				range.end = pos + m_iNameLength;

				m_textRanges[last_range_idx].end = pos;
				last_range_idx = m_textRanges.Count();
				m_textRanges.AddToTail(range);
				buf2++;
			}
			else
			{
				*str = *buf2;
				str++;
				buf2++;
			}
		}
		*str = '\0';

		if (m_textRanges[last_range_idx].end != len)
		{
			TextRange range;
			range.start = m_textRanges[last_range_idx].end;
			range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
			range.end = len;
			m_textRanges.AddToTail(range);
		}
	}

	Colorize();
}

//-----------------------------------------------------------------------------
// Purpose: Inserts colored text into the RichText control at the given alpha
//-----------------------------------------------------------------------------
void CHudChatLine::Colorize(int alpha)
{
	// clear out text
	SetText("");

	CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());

	if (pChat && pChat->GetChatHistory())
	{
		pChat->GetChatHistory()->InsertString("\n");
	}

	wchar_t wText[4096];
	Color color;
	for (int i = 0; i < m_textRanges.Count(); ++i)
	{
		wchar_t *start = m_text + m_textRanges[i].start;
		int len = m_textRanges[i].end - m_textRanges[i].start + 1;
		if (len > 1 && len <= ARRAYSIZE(wText))
		{
			wcsncpy(wText, start, len);
			wText[len - 1] = 0;
			color = m_textRanges[i].color;
			if (!m_textRanges[i].preserveAlpha)
			{
				color[3] = alpha;
			}

			InsertColorChange(color);
			InsertString(wText);

			CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());

			if (pChat && pChat->GetChatHistory())
			{
				pChat->GetChatHistory()->InsertColorChange(color);
				pChat->GetChatHistory()->InsertString(wText);
				pChat->GetChatHistory()->InsertFade(hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME);

				if (i == m_textRanges.Count() - 1)
				{
					pChat->GetChatHistory()->InsertFade(-1, -1);
				}
			}
		}
	}

	InvalidateLayout(true);
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : CHudChatLine
//-----------------------------------------------------------------------------
CHudChatLine *CHudChat::FindUnusedChatLine(void)
{
	return m_ChatLine;
}

void CHudChat::Send(void)
{
	wchar_t szTextbuf[128];

	m_pChatInput->GetMessageText(szTextbuf, sizeof(szTextbuf));

	char ansi[128];
	g_pVGuiLocalize->ConvertUnicodeToANSI(szTextbuf, ansi, sizeof(ansi));

	int len = Q_strlen(ansi);

	/*
This is a very long string that I am going to attempt to paste into the cs hud chat entry and we will see if it gets cropped or not.
	*/

	// remove the \n
	if (len > 0 && ansi[len - 1] == '\n')
	{
		ansi[len - 1] = '\0';
	}

	if (len > 0)
	{
		char szbuf[144]; // more than 128
		Q_snprintf(szbuf, sizeof(szbuf), "%s \"%s\"", m_nMessageMode == MM_SAY ? "say" : "say_team", ansi);

		gEngfuncs.pfnClientCmd(szbuf);
	}

	m_pChatInput->ClearEntry();
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : vgui2::Panel
//-----------------------------------------------------------------------------
vgui2::Panel *CHudChat::GetInputPanel(void)
{
	return m_pChatInput->GetInputPanel();
}

int CHudChat::MsgFunc_SayText(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int client = READ_BYTE(); // the client who spoke the message
	const char *szString = READ_STRING();

	// print raw chat text
	ChatPrintf(client, "%s", szString);

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::Clear(void)
{
	// Kill input prompt
	StopMessageMode();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *newmap -
//-----------------------------------------------------------------------------
void CHudChat::InitHUDData()
{
	Clear();
	m_flHistoryFadeTime = 0;
}

void CHudChat::LevelShutdown(void)
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CHudChat::ChatPrintf(int iPlayerIndex, const char *fmt, ...)
{
	va_list marker;
	char msg[4096];

	va_start(marker, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, marker);
	va_end(marker);

	// Strip any trailing '\n'
	if (strlen(msg) > 0 && msg[strlen(msg) - 1] == '\n')
	{
		msg[strlen(msg) - 1] = 0;
	}

	// Strip leading \n characters ( or notify/color signifiers ) for empty string check
	char *pmsg = msg;
	while (*pmsg && (*pmsg == '\n' || (*pmsg > 0 && *pmsg < COLOR_MAX) || (*pmsg == '^' && *(pmsg + 1) >= '0' && *(pmsg + 1) <= '9')))
	{
		pmsg++;
	}

	if (!*pmsg)
		return;

	// Now strip just newlines, since we want the color info for printing
	pmsg = msg;
	while (*pmsg && (*pmsg == '\n'))
	{
		pmsg++;
	}

	if (!*pmsg)
		return;

	CHudChatLine *line = FindUnusedChatLine();
	if (!line)
	{
		line = FindUnusedChatLine();
	}

	if (!line)
	{
		return;
	}

	// If a player is muted for voice, also mute them for text because jerks gonna jerk.
	if (cl_mute_all_comms.GetBool() && iPlayerIndex != 0)
	{
		if (GetClientVoiceMgr() && GetClientVoiceMgr()->IsPlayerBlocked(iPlayerIndex))
			return;
	}

	line->SetText("");

	int iNameStart = 0;
	int iNameLength = 0;

	const char *playerName = "Console";
	if (iPlayerIndex != 0)
	{
		playerName = GetPlayerInfo(iPlayerIndex)->Update()->GetName();
	}

	int bufSize = (strlen(pmsg) + 1) * sizeof(wchar_t);
	wchar_t *wbuf = static_cast<wchar_t *>(_alloca(bufSize));
	if (wbuf)
	{
		Color clrNameColor = GetClientColor(iPlayerIndex);

		line->SetExpireTime();

		g_pVGuiLocalize->ConvertANSIToUnicode(pmsg, wbuf, bufSize);

		// find the player's name in the unicode string, in case there is no color markup
		const char *pName = playerName;

		if (pName)
		{
			wchar_t wideName[MAX_PLAYER_NAME];
			g_pVGuiLocalize->ConvertANSIToUnicode(pName, wideName, sizeof(wideName));

			const wchar_t *nameInString = wcsstr(wbuf, wideName);

			if (nameInString)
			{
				iNameStart = (nameInString - wbuf);
				iNameLength = wcslen(wideName);
			}
		}

		line->SetVisible(false);
		line->SetNameStart(iNameStart);
		line->SetNameLength(iNameLength);
		line->SetNameColor(clrNameColor);

		line->InsertAndColorizeText(wbuf, iPlayerIndex);
	}

	PlaySound("misc/talk.wav", 1);

	// Print to console
	time_t now;
	struct tm *current;
	char time_buf[32];
	time(&now);
	current = localtime(&now);
	sprintf(time_buf, "[%02i:%02i:%02i] ", current->tm_hour, current->tm_min, current->tm_sec);
	ConPrintf(m_ConsoleMsgColor, "%s %s\n", time_buf, pmsg);
}

void CHudChatEntry::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER || code == vgui2::KEY_ESCAPE)
	{
		if (code != vgui2::KEY_ESCAPE)
		{
			if (m_pHudChat)
			{
				PostMessage(m_pHudChat, new KeyValues("ChatEntrySend"));
			}

			// HACK: Pressing ENTER key for some reason bugs the mouse:
			// it stays on even after SetMouseInputEnabled(false)
			gHUD.CallOnNextFrame([]() {
				vgui2::surface()->CalculateMouseVisible();
			});
		}
		else
		{
			// Escape shows GameUI
			g_pBaseUI->HideGameUI();
		}

		// End message mode.
		if (m_pHudChat)
		{
			PostMessage(m_pHudChat, new KeyValues("ChatEntryStopMessageMode"));
		}
	}
	else if (code == vgui2::KEY_TAB)
	{
		// Ignore tab, otherwise vgui will screw up the focus.
		return;
	}
	else if (code == vgui2::KEY_BACKQUOTE)
	{
		// Pressing tilde key (~ or `) opens the console.
		g_pBaseUI->HideGameUI();

		// After GameUI was hidden, mouse input is broken
		m_pHudChat->SetKeyBoardInputEnabled(true);
		m_pHudChat->SetMouseInputEnabled(true);
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}
