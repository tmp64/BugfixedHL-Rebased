//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include <ctime>
#include <FileSystem.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>
#include "hud.h"
#include "cl_util.h"
#include "client_vgui.h"
#include "vgui/client_viewport.h"
#include "vgui/score_panel.h"
#include "chat.h"
#include "parsemsg.h"
#include "cl_voice_status.h"
#include "results.h"
#include "hud/ag/ag_location.h"
#include "gameui/gameui_viewport.h"

ConVar hud_saytext("hud_saytext", "1", FCVAR_BHL_ARCHIVE, "Enable/disable display of new chat messages");
ConVar hud_saytext_time("hud_saytext_time", "12", FCVAR_BHL_ARCHIVE, "How long for new messages should stay on the screen");
ConVar hud_saytext_sound("hud_saytext_sound", "1", FCVAR_BHL_ARCHIVE, "Play sound on new chat message");
ConVar cl_mute_all_comms("cl_mute_all_comms", "1", FCVAR_BHL_ARCHIVE, "If 1, then all communications from a player will be blocked when that player is muted, including chat messages.");

constexpr const char CHAT_SOUND_FILE[] = "misc/talk.wav";
constexpr const char CHAT_SOUND_FALLBACK[] = "misc/talk_bhl_fallback.wav";

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

	if (hud_saytext.GetBool())
	{
		m_flExpireTime = m_flStartTime + hud_saytext_time.GetFloat();
	}
	else
	{
		// Expire immediately
		m_flExpireTime = m_flStartTime;
	}
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
	m_pInput->SetMaximumCharCount(MAX_CHAT_INPUT_STRING_LEN);
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

void CHudChatHistory::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
	{
		// Hide the chatbox
		static_cast<CHudChat *>(GetParent())->StopMessageMode();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
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
	m_DefTextColor = pScheme->GetColor("ChatTextColor", NoTeamColor::Orange);
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

static CmdFunction s_pfnEngineMsgMode = nullptr;
static CmdFunction s_pfnEngineMsgMode2 = nullptr;

static void MessageModeVGUI2()
{
	if (gEngfuncs.GetMaxClients() == 1)
		return;

	if (gEngfuncs.Cmd_Argc() != 1)
	{
		s_pfnEngineMsgMode();
		return;
	}

	CHudChat::Get()->StartMessageMode(MM_SAY);
}

static void MessageMode2VGUI2()
{
	if (gEngfuncs.GetMaxClients() == 1)
		return;

	if (gEngfuncs.Cmd_Argc() != 1)
	{
		s_pfnEngineMsgMode2();
		return;
	}

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
		s_pfnEngineMsgMode = msgMode->function;
		s_pfnEngineMsgMode2 = msgMode2->function;
		msgMode->function = MessageModeVGUI2;
		msgMode2->function = MessageMode2VGUI2;
	}

	char buf[128];
	snprintf(buf, sizeof(buf), "sound/%s", CHAT_SOUND_FILE);
	if (g_pFullFileSystem->FileExists(buf))
		m_pszChatSoundPath = CHAT_SOUND_FILE;
	else
		m_pszChatSoundPath = CHAT_SOUND_FALLBACK;
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
		GetChatHistory()->ResetAllFades(true);
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

	// Move chat to front so it isn't obscured by the scoreboard in intermission
	MoveToFront();

	m_flHistoryFadeTime = gEngfuncs.GetAbsoluteTime() + CHAT_HISTORY_FADE_TIME;

	CGameUIViewport::Get()->PreventEscapeToShow(true);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::StopMessageMode(void)
{
	CGameUIViewport::Get()->PreventEscapeToShow(false);

	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	if (GetChatHistory())
	{
		GetChatHistory()->SetPaintBorderEnabled(false);
		GetChatHistory()->GotoTextEnd();
		GetChatHistory()->SetMouseInputEnabled(false);
		GetChatHistory()->SetVerticalScrollbar(false);
		GetChatHistory()->ResetAllFades(false, true, CHAT_HISTORY_FADE_TIME);
		GetChatHistory()->SelectNoText();
	}

	//Clear the entry since we wont need it anymore.
	m_pChatInput->ClearEntry();
	m_pChatInput->SetVisible(false);
	m_pChatInput->SetKeyBoardInputEnabled(false);
	m_pChatInput->SetMouseInputEnabled(false);

	// Move scoreboard to front if it is active so it isn't obscured by chat text
	if (g_pViewport->GetScoreBoard()->IsVisible())
		g_pViewport->GetScoreBoard()->MoveToFront();

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
	return m_DefTextColor;
}

//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor(int clientIndex)
{
	if (CPlayerInfo *pi = GetPlayerInfoSafe(clientIndex))
		return g_pViewport->GetTeamColor(pi->Update()->GetTeamNumber());

	return g_pViewport->GetTeamColor(0);
}

//-----------------------------------------------------------------------------
// Purpose: Parses a line of text for color markup and inserts it via Colorize()
//-----------------------------------------------------------------------------
void CHudChatLine::InsertAndColorizeText(wchar_t *buf, int clientIndex)
{
	// buf will look something like this
	// \x02PlayerName: A chat message!\0
	//     ^~~~~~~~~~^~~~~~~~~~~~~~~~~
	// <player color>   <default color>
	// \x02 is COLOR_PLAYERNAME

	// m_textRanges contains TextRanges.
	// Each TextRange describes color of a substring of the chat message.
	//
	// start - index of the first char of the substring
	// end - index AFTER the last char of the substring
	// color - color of the substring

	if (m_text)
	{
		delete[] m_text;
		m_text = NULL;
	}
	m_textRanges.RemoveAll();

	CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());

	if (pChat == NULL)
		return;

	// Color the message
	{
		wchar_t *buf2 = buf;
		int len = wcslen(buf2);
		m_text = new wchar_t[len + 1];

		wchar_t *str = m_text;

		// Add initial color of the message
		{
			TextRange range;
			range.start = 0;
			range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
			range.end = len;
			m_textRanges.AddToTail(range);
		}

		int last_range_idx = 0;
		int is_player_msg = *buf2 == 2 ? 1 : 0;

		while (*buf2)
		{
			int pos = str - m_text;

			// Reset color after player name to default
			if (pos == m_iNameStart + m_iNameLength && is_player_msg) // The only color is player name
			{
				TextRange range;
				range.start = pos;
				range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
				range.end = len;

				m_textRanges[last_range_idx].end = pos;

				m_textRanges.AddToTail(range);

				last_range_idx = m_textRanges.Count() - 1;
			}

			if (IsColorCode(buf2)) // Parse colorcodes
			{
				TextRange range;
				int idx = *(buf2 + 1) - '0';
				if (idx == 0 || idx == 9)
				{
					if (pos <= m_iNameStart + m_iNameLength && is_player_msg)
						range.color = pChat->GetTextColorForClient(COLOR_PLAYERNAME, clientIndex);
					else
						range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
				}
				else
				{
					range.color = gHUD.GetColorCodeColor(idx);
				}
				range.start = pos;
				range.end = len;

				m_textRanges[last_range_idx].end = pos;
				m_textRanges.AddToTail(range);
				last_range_idx = m_textRanges.Count() - 1;

				if (pos >= m_iNameStart && pos < m_iNameStart + m_iNameLength)
					m_iNameLength -= 2;

				buf2 += 2;
			}
			// m_iNameLength > 0 fixes the miniag issue too, but with the drawback of not coloring
			// the player name according to the player team
			else if (*buf2 == COLOR_PLAYERNAME && pos == 0 && m_iNameLength > 0) // Color of the player name
			{
				TextRange range;
				range.start = pos;
				range.color = pChat->GetTextColorForClient(COLOR_PLAYERNAME, clientIndex);
				range.end = len;

				m_textRanges[last_range_idx].end = pos;
				m_textRanges.AddToTail(range);
				last_range_idx = m_textRanges.Count() - 1;

				// shift name start position since we are removing a character
				m_iNameStart--;
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

		// Add final text range if need to
		if (m_textRanges[last_range_idx].end != len)
		{
			TextRange range;
			range.start = m_textRanges[last_range_idx].end;
			range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
			range.end = len;
			m_textRanges.AddToTail(range);
		}
	}

	// Add text to the history as described in m_textRanges
	Colorize();

	// Color range debugging
	// Change 0 to 1 to enable.
	// Make sure to disable it before commiting.
#if 0
	std::wstring str = std::wstring(m_text);
	for (int i = 0; i < m_textRanges.Count(); i++)
	{
		ConPrintf("%2d. start: %3d end: %3d color: [%3d %3d %3d] %ls\n",
			i + 1, m_textRanges[i].start, m_textRanges[i].end,
			m_textRanges[i].color.r(), m_textRanges[i].color.g(), m_textRanges[i].color.b(),
			str.substr(m_textRanges[i].start, m_textRanges[i].end - m_textRanges[i].start).c_str());

	}
	ConPrintf("m_text %ls\n", m_text);
#endif
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

				if (hud_saytext.GetBool())
					pChat->GetChatHistory()->InsertFade(hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME);
				else
					pChat->GetChatHistory()->InsertFade(0, 0);

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
	wchar_t szTextbuf[MAX_CHAT_INPUT_STRING_LEN + 1];

	m_pChatInput->GetMessageText(szTextbuf, sizeof(szTextbuf));

	char ansi[2 * MAX_CHAT_INPUT_STRING_LEN + 1];
	g_pVGuiLocalize->ConvertUnicodeToANSI(szTextbuf, ansi, sizeof(ansi));

	int len = Q_strlen(ansi);

	/*
This is a very long string that I am going to attempt to paste into the cs hud chat entry and we will see if it gets cropped or not.
	*/

	// remove the \n
	if (len > 0 && ansi[len - 1] == '\n')
	{
		ansi[len - 1] = '\0';
		len--;
	}

	char *pstr = ansi;

	while (len > MAX_CHAT_STRING_LEN)
	{
		char buf[MAX_CHAT_STRING_LEN + 1];

		// Split ansi - take only MAX_CHAT_STRING_LEN chars
		Q_strncpy(buf, pstr, MAX_CHAT_STRING_LEN + 1);
		pstr += MAX_CHAT_STRING_LEN;
		len -= MAX_CHAT_STRING_LEN;

		// Check if we split a multibyte char
		int mblen = 0;
		if ((buf[MAX_CHAT_STRING_LEN - 1] & 0b11000000) == 0b11000000)
		{
			// Beginning of multibyte char
			mblen = 1;
		}
		else if ((buf[MAX_CHAT_STRING_LEN - 1] & 0b11000000) == 0b10000000)
		{
			// Middle of multibyte char
			mblen = 1;
			for (; mblen <= 6 && (buf[MAX_CHAT_STRING_LEN - mblen] & 0b11000000) != 0b11000000; mblen++)
				;
		}

		// Leave mblen chars for the next iteration
		if (mblen > 0)
		{
			len += mblen;
			pstr -= mblen;
			buf[MAX_CHAT_STRING_LEN - mblen] = '\0';
		}

		// Say the string
		char szbuf[144]; // more than 128
		Q_snprintf(szbuf, sizeof(szbuf), "%s \"%s\"", m_nMessageMode == MM_SAY ? "say" : "say_team", buf);
		gEngfuncs.pfnClientCmd(szbuf);
	}

	if (len > 0)
	{
		char szbuf[144]; // more than 128
		Q_snprintf(szbuf, sizeof(szbuf), "%s \"%s\"", m_nMessageMode == MM_SAY ? "say" : "say_team", pstr);

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

	// Substitute location
	AgHudLocation::Get()->ParseAndEditSayString(iPlayerIndex, msg, sizeof(msg));

	// Replace server name with real name
	CPlayerInfo *pi = GetPlayerInfoSafe(iPlayerIndex);
	if (pi)
		pi->Update();

	if (pi && pi->HasRealName())
	{
		const char *realname = pi->GetDisplayName();
		int realnamelen = strlen(realname);

		// Find player name
		const char *nameinmsg = strstr(msg, pi->GetName());
		int namelen = 0;

		if (!nameinmsg)
		{
			// Try name without color codes (miniAG bug)
			const char *strippedname = RemoveColorCodes(pi->GetName());
			nameinmsg = strstr(msg, strippedname);

			if (nameinmsg)
				namelen = strlen(strippedname);
		}
		else
		{
			namelen = strlen(pi->GetName());
		}

		if (namelen > 0)
		{
			int namestart = nameinmsg - msg;
			int nameend = namestart + namelen;
			int realnameend = namestart + realnamelen;

			// Move part after the name to where it will be after replace
			memmove(msg + realnameend, msg + nameend, std::min(sizeof(msg) - nameend + 1, sizeof(msg) - realnameend - 1));

			// Replace name with realname
			memcpy(msg + namestart, realname, realnamelen);
		}
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
	if (GetThisPlayerInfo() && cl_mute_all_comms.GetBool() && iPlayerIndex != 0 && iPlayerIndex != GetThisPlayerInfo()->GetIndex())
	{
		if (GetClientVoiceMgr() && GetClientVoiceMgr()->IsPlayerBlocked(iPlayerIndex))
			return;
	}

	line->SetText("");

	int iNameStart = 0;
	int iNameLength = 0;

	const char *playerName = "Console";
	if (CPlayerInfo *pi = GetPlayerInfoSafe(iPlayerIndex))
	{
		playerName = pi->Update()->GetDisplayName();
	}

	int msglen = strlen(pmsg);
	int bufSize = (msglen + 1) * sizeof(wchar_t);
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
			// miniag issue: server-side is giving a name with colorcodes while say message doesn't have them
			// server-side will give the name with colors removed after first name change
			// so until that, we need to remove them by ourselves and try to find again
			if (pi && !pi->HasRealName() && !strstr(pmsg, playerName))
				pName = RemoveColorCodes(pName);

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

	if (hud_saytext.GetBool() && hud_saytext_sound.GetFloat() > 0)
		PlaySound(m_pszChatSoundPath, hud_saytext_sound.GetFloat());

	// Print to console
	time_t now;
	struct tm *current;
	char time_buf[32];
	time(&now);
	current = localtime(&now);
	sprintf(time_buf, "[%02i:%02i:%02i] ", current->tm_hour, current->tm_min, current->tm_sec);

	if (gHUD.GetColorCodeAction() == ColorCodeAction::Handle || gHUD.GetColorCodeAction() == ColorCodeAction::Strip)
		ConPrintf(m_ConsoleMsgColor, "%s %s\n", time_buf, RemoveColorCodes(pmsg));
	else
		ConPrintf(m_ConsoleMsgColor, "%s %s\n", time_buf, pmsg);

	bool isPlayerChat = (pmsg[0] == COLOR_PLAYERNAME);
	CResults::Get().AddLog(time_buf, isPlayerChat);
	// For player messages first bytes needs to be skipped since it's COLOR_PLAYERNAME and not text
	CResults::Get().AddLog(pmsg + (isPlayerChat ? 1 : 0), isPlayerChat);

	if (pmsg[msglen - 1] != '\n')
		CResults::Get().AddLog("\n", isPlayerChat);
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
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}
