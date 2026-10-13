#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <string>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/ILocalize.h>
#include "hud.h"
#include "cl_util.h"
#include "pm_shared.h"
#include "scoreboard_data.h"

using namespace bhl::sb;

static_assert(TARGET_PLAYER_LAST - TARGET_PLAYER_FIRST + 1 <= MAX_PLAYERS, "player targets must fit m_PlayerValues");
static_assert(TARGET_TEAM_LAST - TARGET_TEAM_BASE <= MAX_TEAMS, "team targets must fit m_TeamValues");
static_assert(MAX_TEAMS <= MAX_PLAYERS, "ranked teams must fit Ranking::entities");
static_assert(MAX_PLACEHOLDERS <= 32, "missing placeholders must fit a mask");

ConVar hud_scoreboard_custom("hud_scoreboard_custom", "1", FCVAR_ARCHIVE, "Show the custom scoreboard when the server sends one");
ConVar hud_server_elements("hud_server_elements", "1", FCVAR_BHL_ARCHIVE, "Draw HUD elements defined by the server");

CON_COMMAND(hud_scoreboard_dump, "Prints the scoreboard and HUD data of the server to the console")
{
	CScoreboardData::Get().DumpCommand();
}

CON_COMMAND(hud_scoreboard_test, "Fills the scoreboard and HUD with test data: hud_scoreboard_test [off]")
{
	CScoreboardData::Get().TestCommand();
}

CON_COMMAND(hud_server_eval, "Evaluates a HUD template for the current subject: hud_server_eval \"<template>\"")
{
	CScoreboardData::Get().EvalCommand();
}

//-----------------------------------------------------
// CMsgReader
//-----------------------------------------------------
class CScoreboardData::CMsgReader
{
public:
	CMsgReader(const void *pBuf, int iSize)
	    : m_pBuf(static_cast<const unsigned char *>(pBuf))
	    , m_iSize(iSize > 0 ? iSize : 0)
	{
	}

	int GetRemaining() const
	{
		return m_iSize - m_iPos;
	}

	bool ReadByte(int &out)
	{
		if (GetRemaining() < 1)
			return false;

		out = m_pBuf[m_iPos];
		m_iPos++;
		return true;
	}

	bool ReadChar(int &out)
	{
		if (!ReadByte(out))
			return false;

		out = static_cast<signed char>(out);
		return true;
	}

	bool ReadWord(int &out)
	{
		if (GetRemaining() < 2)
			return false;

		out = m_pBuf[m_iPos] | (m_pBuf[m_iPos + 1] << 8);
		m_iPos += 2;
		return true;
	}

	bool ReadShort(int &out)
	{
		if (!ReadWord(out))
			return false;

		out = static_cast<short>(out);
		return true;
	}

	bool ReadLong(int &out)
	{
		if (GetRemaining() < 4)
			return false;

		unsigned int value = 0;

		for (int i = 3; i >= 0; i--)
			value = (value << 8) | m_pBuf[m_iPos + i];

		out = static_cast<int>(value);
		m_iPos += 4;
		return true;
	}

	/**
	 * Reads a NUL-terminated string, cut to the buffer on a UTF-8 boundary.
	 * @param	bStripColors	Removes color codes (4.2): every string but template texts and refs
	 * @return	false if the payload ends before the NUL; buf is then ""
	 */
	bool ReadString(char *buf, int bufSize, bool bStripColors = true)
	{
		int len = 0;
		bool bTruncated = false;
		bool bTerminated = false;

		while (GetRemaining() > 0)
		{
			char c = static_cast<char>(m_pBuf[m_iPos]);
			m_iPos++;

			if (c == '\0')
			{
				bTerminated = true;
				break;
			}

			if (len < bufSize - 1)
				buf[len++] = c;
			else
				bTruncated = true;
		}

		buf[bTerminated ? len : 0] = '\0';

		if (bTruncated)
			Q_UnicodeRepair(buf, STRINGCONVERT_SKIP);

		if (bStripColors)
			RemoveColorCodesInPlace(buf);

		return bTerminated;
	}

	template <size_t N>
	bool ReadString(char (&buf)[N], bool bStripColors = true)
	{
		return ReadString(buf, N, bStripColors);
	}

	//! Returns a reader of the next len bytes and skips them. len must not exceed GetRemaining().
	CMsgReader ReadBlock(int len)
	{
		CMsgReader block(m_pBuf + m_iPos, len);
		m_iPos += len;
		return block;
	}

private:
	const unsigned char *m_pBuf = nullptr;
	int m_iSize = 0;
	int m_iPos = 0;
};

namespace
{

//! Writes a BhlScore message for hud_scoreboard_test
class CMsgWriter
{
public:
	explicit CMsgWriter(E_Op op)
	{
		WriteByte(static_cast<int>(op));
	}

	void WriteByte(int value)
	{
		if (m_iSize >= MAX_MSG_SIZE)
		{
			m_bOverflow = true;
			return;
		}

		m_Buf[m_iSize++] = static_cast<unsigned char>(value);
	}

	void WriteShort(int value)
	{
		WriteByte(value & 0xFF);
		WriteByte((value >> 8) & 0xFF);
	}

	void WriteLong(int value)
	{
		WriteShort(value & 0xFFFF);
		WriteShort((value >> 16) & 0xFFFF);
	}

	void WriteString(const char *str)
	{
		for (; *str; str++)
			WriteByte(*str);

		WriteByte(0);
	}

	void WriteInt(int value)
	{
		WriteByte(static_cast<int>(E_ValueType::Int));

		if (value >= SCHAR_MIN && value <= SCHAR_MAX)
		{
			WriteByte(1);
			WriteByte(value & 0xFF);
		}
		else if (value >= SHRT_MIN && value <= SHRT_MAX)
		{
			WriteByte(2);
			WriteShort(value);
		}
		else
		{
			WriteByte(4);
			WriteLong(value);
		}
	}

	void WriteText(const char *text)
	{
		WriteByte(static_cast<int>(E_ValueType::Text));
		WriteByte(static_cast<int>(strlen(text)) + 1);
		WriteString(text);
	}

	void WriteIcon(const char *sprite, const char *text)
	{
		WriteByte(static_cast<int>(E_ValueType::Icon));
		WriteByte(static_cast<int>(strlen(sprite) + strlen(text)) + 2);
		WriteString(sprite);
		WriteString(text);
	}

	void WriteProgress(int cur, int max)
	{
		WriteByte(static_cast<int>(E_ValueType::Progress));
		WriteByte(4);
		WriteShort(cur);
		WriteShort(max);
	}

	bool IsOverflowed() const
	{
		return m_bOverflow;
	}

	unsigned char *GetData()
	{
		return m_Buf;
	}

	int GetSize() const
	{
		return m_iSize;
	}

private:
	unsigned char m_Buf[MAX_MSG_SIZE];
	int m_iSize = 0;
	bool m_bOverflow = false;
};

//! Client tokens of templates (8.10)
class CTemplateLocalizer : public ITemplateLocalizer
{
public:
	const wchar_t *Find(const char *token) override
	{
		return g_pVGuiLocalize->Find(token);
	}
};

CTemplateLocalizer s_TemplateLocalizer;

const CScoreboardData::Value s_EmptyValue = CScoreboardData::Value();
const CScoreboardData::Column s_EmptyColumn = CScoreboardData::Column();
const CScoreboardData::Element s_EmptyElement = CScoreboardData::Element();

void ClearValues(CScoreboardData::Value *values)
{
	for (int i = 0; i < MAX_COLUMNS; i++)
		values[i] = CScoreboardData::Value();
}

//! Returns < 0 if a goes before b. Empty values go after filled ones and numbers before texts in both directions.
int CompareValues(const CScoreboardData::Value &a, const CScoreboardData::Value &b, bool bAscending)
{
	if (a.IsEmpty() || b.IsEmpty())
		return static_cast<int>(a.IsEmpty()) - static_cast<int>(b.IsEmpty());

	int numA = 0;
	int numB = 0;
	bool bIsNumA = a.GetNumber(numA);
	bool bIsNumB = b.GetNumber(numB);
	int result = 0;

	if (bIsNumA != bIsNumB)
		return bIsNumA ? -1 : 1;

	if (bIsNumA)
	{
		result = (numA > numB) - (numA < numB);
	}
	else
	{
		result = Q_stricmp(a.szText, b.szText);

		if (result == 0)
			result = Q_stricmp(a.szSprite, b.szSprite);
	}

	return bAscending ? result : -result;
}

bool IsTeamNumber(int team)
{
	return team >= 1 && team <= MAX_TEAMS;
}

//! Team number of a connected player, 0 otherwise
int GetPlayerTeamNumber(int client)
{
	CPlayerInfo *pi = GetPlayerInfoSafe(client);
	return pi ? pi->GetTeamNumber() : 0;
}

//! Frags and deaths from ScoreInfo, 0 for a slot without a player
void GetPlayerScore(int client, int &frags, int &deaths)
{
	CPlayerInfo *pi = GetPlayerInfoSafe(client);
	frags = pi ? pi->GetFrags() : 0;
	deaths = pi ? pi->GetDeaths() : 0;
}

int ClampToInt(long long value)
{
	if (value > INT_MAX)
		return INT_MAX;

	if (value < INT_MIN)
		return INT_MIN;

	return static_cast<int>(value);
}

//! The display name without color codes, never looked up as a token (8.10)
bool WritePlayerName(int client, wchar_t *buf, int bufSizeBytes)
{
	CPlayerInfo *pi = GetPlayerInfoSafe(client);

	if (!pi)
		return false;

	char name[128];
	RemoveColorCodes(pi->GetDisplayName(), name, sizeof(name));
	Q_UTF8ToWString(name, buf, bufSizeBytes, STRINGCONVERT_REPLACE);
	return true;
}

//! The team name as the scoreboard team header shows it. False for a team without players.
bool WriteTeamName(int team, wchar_t *buf, int bufSizeBytes)
{
	if (!IsTeamNumber(team))
		return false;

	bool bHasPlayers = false;

	for (int i = 1; i <= MAX_PLAYERS && !bHasPlayers; i++)
		bHasPlayers = GetPlayerTeamNumber(i) == team;

	if (!bHasPlayers)
		return false;

	char name[128];
	RemoveColorCodes(GetTeamInfo(team)->GetDisplayName(), name, sizeof(name));
	const wchar_t *localized = name[0] == '#' ? g_pVGuiLocalize->Find(name) : nullptr;

	if (localized)
		V_wcsncpy(buf, localized, bufSizeBytes);
	else
		Q_UTF8ToWString(name, buf, bufSizeBytes, STRINGCONVERT_REPLACE);

	return true;
}

bool WriteSubjectName(const CScoreboardData::Subject &subject, wchar_t *buf, int bufSizeBytes)
{
	if (subject.IsTeam())
		return WriteTeamName(subject.iTeam, buf, bufSizeBytes);

	return WritePlayerName(subject.iPlayer, buf, bufSizeBytes);
}

//! Number of a built-in value (8.11); false if it is missing or not a number
bool GetBuiltinNumber(E_Builtin builtin, const CScoreboardData::Builtins &b, int &out)
{
	switch (builtin)
	{
	case E_Builtin::Place:
		out = b.iPlace;
		return b.bRanked;
	case E_Builtin::Total:
		out = b.iTotal;
		return true;
	case E_Builtin::State:
		out = b.iState;
		return b.bRanked;
	case E_Builtin::Gap:
		out = b.iGap;
		return b.bRanked && b.iState == 1;
	case E_Builtin::Pass:
		out = ClampToInt(static_cast<long long>(b.iGap) + 1);
		return b.bRanked && b.iState == 1;
	case E_Builtin::Lead:
		out = b.iLead;
		return b.bRanked && (b.iState == 2 || b.iState == 3);
	case E_Builtin::Behind:
		out = b.iBehind;
		return b.bRanked;
	case E_Builtin::Score:
		out = b.iScore;
		return b.bHasScore;
	case E_Builtin::Frags:
		out = b.iFrags;
		return b.bHasSubject;
	case E_Builtin::Deaths:
		out = b.iDeaths;
		return b.bHasSubject;
	default:
		return false;
	}
}

//! A stored value as a template shows it (8.10, "Values are written as")
bool ValueToTemplate(const CScoreboardData::Value &value, TemplateValue &out)
{
	switch (value.type)
	{
	case E_ValueType::Int:
		out.type = E_TemplateValueType::Int;
		out.iValue = value.iValue;
		return true;
	case E_ValueType::Text:
		out.type = E_TemplateValueType::Text;
		CScoreboardData::LocalizeString(value.szText, out.wszText, sizeof(out.wszText));
		return true;
	case E_ValueType::Icon:
		out.type = E_TemplateValueType::Text;
		CScoreboardData::GetIconName(value, out.wszText, sizeof(out.wszText));
		return true;
	case E_ValueType::Progress:
		out.type = E_TemplateValueType::Progress;
		out.iValue = value.iValue;
		out.iMax = value.iMax;
		return true;
	case E_ValueType::Clock:
		out.type = E_TemplateValueType::Clock;
		out.iValue = value.GetClockMs();
		out.bClockDown = IsEnumFlagSet(value.clockFlags, E_ClockFlags::Down);
		return true;
	default:
		return false;
	}
}

//! cur, max and left make an Int of a Progress; any other value is missing with them
bool GetProgressPart(E_RefFormat format, int cur, int max, int &out)
{
	switch (format)
	{
	case E_RefFormat::Cur:
		out = cur;
		return true;
	case E_RefFormat::Max:
		out = max;
		return true;
	case E_RefFormat::Left:
		out = max > cur ? max - cur : 0;
		return true;
	default:
		return false;
	}
}

//! Applies the format of a ref to its resolved value; false makes the value missing
bool ApplyRefFormat(E_RefFormat format, TemplateValue &out)
{
	if (format == E_RefFormat::None)
		return true;

	if (out.type != E_TemplateValueType::Progress || !GetProgressPart(format, out.iValue, out.iMax, out.iValue))
	{
		out.type = E_TemplateValueType::Missing;
		return false;
	}

	out.type = E_TemplateValueType::Int;
	out.iMax = 0;
	return true;
}

//! The first arg of type Icon or nullptr
const CScoreboardData::Value *FindIconArg(const CScoreboardData::Value *args, int count)
{
	for (int i = 0; i < count; i++)
	{
		if (args[i].type == E_ValueType::Icon)
			return &args[i];
	}

	return nullptr;
}

//! The default counter of a Meter without variant 0 (8.7)
const CTemplate &GetMeterCounterTemplate()
{
	static CTemplate s_Counter;

	if (!s_Counter.IsSet())
		s_Counter.Parse(L"{@value:cur} / {@value:max}");

	return s_Counter;
}

//! Values of templates for one subject and element
class CStoreTemplateContext : public ITemplateContext
{
public:
	CStoreTemplateContext(const CScoreboardData &data, const CScoreboardData::Subject &subject, int elementId, const CScoreboardData::TemplateExtras *pExtras)
	    : m_Data(data)
	    , m_Subject(subject)
	    , m_iElementId(elementId)
	    , m_pExtras(pExtras)
	{
	}

	bool GetValue(const TemplateRef &ref, TemplateValue &out) override
	{
		if (!m_pExtras || ref.bFirstRow)
			return m_Data.ResolveRef(ref, m_Subject, m_iElementId, out);

		const CScoreboardData::TemplateExtras &extras = *m_pExtras;
		const CScoreboardData::Value *value = nullptr;
		CScoreboardData::Value index;

		switch (ref.builtin)
		{
		case E_Builtin::Value:
			if (!extras.pValue)
				return m_Data.ResolveRef(ref, m_Subject, m_iElementId, out);

			value = extras.pValue;
			break;
		case E_Builtin::Item:
			value = extras.pItem;
			break;
		case E_Builtin::Index:
			index.type = extras.iIndex > 0 ? E_ValueType::Int : E_ValueType::None;
			index.iValue = extras.iIndex;
			value = &index;
			break;
		case E_Builtin::A:
		case E_Builtin::B:
		case E_Builtin::C:
		case E_Builtin::D:
		{
			int arg = static_cast<int>(ref.builtin) - static_cast<int>(E_Builtin::A);
			value = extras.pArgs && arg < extras.iArgCount ? &extras.pArgs[arg] : nullptr;
			break;
		}
		case E_Builtin::Icon:
			value = extras.pArgs ? FindIconArg(extras.pArgs, extras.iArgCount) : nullptr;
			break;
		default:
			return m_Data.ResolveRef(ref, m_Subject, m_iElementId, out);
		}

		out = TemplateValue();
		return value && ValueToTemplate(*value, out) && ApplyRefFormat(ref.format, out);
	}

	bool GetPlayerName(int idx, wchar_t *buf, int bufLen) override
	{
		return WritePlayerName(idx, buf, bufLen * static_cast<int>(sizeof(wchar_t)));
	}

	bool GetTeamName(int team, wchar_t *buf, int bufLen) override
	{
		return WriteTeamName(team, buf, bufLen * static_cast<int>(sizeof(wchar_t)));
	}

private:
	const CScoreboardData &m_Data;
	CScoreboardData::Subject m_Subject;
	int m_iElementId = -1;
	const CScoreboardData::TemplateExtras *m_pExtras = nullptr;
};

void ValueToDebugString(const CScoreboardData::Value &value, char *buf, int bufSize)
{
	switch (value.type)
	{
	case E_ValueType::Int:
		snprintf(buf, bufSize, "%d", value.iValue);
		break;
	case E_ValueType::Text:
		snprintf(buf, bufSize, "\"%s\"", value.szText);
		break;
	case E_ValueType::Icon:
		snprintf(buf, bufSize, "%s \"%s\"", value.szSprite, value.szText);
		break;
	case E_ValueType::Progress:
		snprintf(buf, bufSize, "%d/%d", value.iValue, value.iMax);
		break;
	case E_ValueType::Clock:
		snprintf(buf, bufSize, "clock %d ms (now %d), flags 0x%x", value.iValue, value.GetClockMs(), static_cast<unsigned int>(value.clockFlags));
		break;
	default:
		snprintf(buf, bufSize, "-");
		break;
	}
}

void SubjectToDebugString(const CScoreboardData::Subject &subject, char *buf, int bufSize)
{
	wchar_t wname[64];
	char name[128];

	if (!WriteSubjectName(subject, wname, sizeof(wname)))
		wname[0] = L'\0';

	Q_WStringToUTF8(wname, name, sizeof(name), STRINGCONVERT_REPLACE);

	if (subject.IsTeam())
		snprintf(buf, bufSize, "team %d \"%s\"", subject.iTeam, name);
	else if (subject.iPlayer)
		snprintf(buf, bufSize, "player %d \"%s\"", subject.iPlayer, name);
	else
		snprintf(buf, bufSize, "none");
}

CScoreboardData s_ScoreboardData;

}

//-----------------------------------------------------
// Value, Column, Element
//-----------------------------------------------------
bool CScoreboardData::Value::IsEmpty() const
{
	return type == E_ValueType::None;
}

bool CScoreboardData::Value::IsRunningClock() const
{
	return type == E_ValueType::Clock && IsEnumFlagSet(clockFlags, E_ClockFlags::Run);
}

int CScoreboardData::Value::GetClockMs() const
{
	if (!IsRunningClock())
		return iValue;

	double elapsed = (static_cast<double>(GetTime()) - flClockTime) * 1000.0;

	if (elapsed < 0)
		elapsed = 0;

	if (IsEnumFlagSet(clockFlags, E_ClockFlags::Down))
	{
		double ms = iValue - elapsed;
		return ms > 0 ? static_cast<int>(ms) : 0;
	}

	double ms = iValue + elapsed;
	return ms < INT_MAX ? static_cast<int>(ms) : INT_MAX;
}

bool CScoreboardData::Value::GetNumber(int &out) const
{
	switch (type)
	{
	case E_ValueType::Int:
	case E_ValueType::Progress:
		out = iValue;
		return true;
	case E_ValueType::Clock:
		out = GetClockSeconds(GetClockMs(), IsEnumFlagSet(clockFlags, E_ClockFlags::Down));
		return true;
	default:
		return false;
	}
}

void CScoreboardData::Value::ToString(wchar_t *buf, int bufSizeBytes) const
{
	int bufLen = bufSizeBytes / static_cast<int>(sizeof(wchar_t));

	if (bufLen <= 0)
		return;

	switch (type)
	{
	case E_ValueType::Int:
		V_snwprintf(buf, bufLen, L"%d", iValue);
		break;
	case E_ValueType::Text:
		LocalizeString(szText, buf, bufSizeBytes);
		break;
	case E_ValueType::Icon:
		GetIconName(*this, buf, bufSizeBytes);
		break;
	case E_ValueType::Progress:
		if (iMax == 0)
			V_snwprintf(buf, bufLen, L"%d", iValue);
		else
			V_snwprintf(buf, bufLen, L"%d/%d", iValue, iMax);
		break;
	case E_ValueType::Clock:
	{
		int seconds = 0;
		GetNumber(seconds);
		FormatTime(seconds, buf, bufLen);
		break;
	}
	default:
		buf[0] = L'\0';
		break;
	}
}

int CScoreboardData::Column::GetAlign() const
{
	int align = (static_cast<unsigned int>(flags) >> 1) & 3;
	return align == 3 ? 0 : align;
}

bool CScoreboardData::Element::GetParam(E_Param param, int &value) const
{
	int id = static_cast<int>(param);

	if (id < 0 || id > MAX_PARAM_ID || !(iParamMask & ParamBit(id)))
		return false;

	value = params[id];
	return true;
}

bool CScoreboardData::Element::GetSlotColor(int slot, unsigned char out[4]) const
{
	if (slot < 0 || slot >= MAX_COLOR_SLOTS || !(iColorMask & (1u << slot)))
		return false;

	memcpy(out, colors[slot], sizeof(colors[slot]));
	return true;
}

//-----------------------------------------------------
// CScoreboardData
//-----------------------------------------------------
CScoreboardData &CScoreboardData::Get()
{
	return s_ScoreboardData;
}

CScoreboardData::CScoreboardData() = default;

void CScoreboardData::Init()
{
	::HookMessage(MSG_NAME, [](const char *pszName, int iSize, void *pbuf) {
		return CScoreboardData::Get().MsgFunc_BhlScore(pszName, iSize, pbuf);
	});
}

void CScoreboardData::VidInit()
{
	Clear();
	m_bWaitForReset = false;
	std::fill(std::begin(m_bPlayerConnected), std::end(m_bPlayerConnected), false);
}

void CScoreboardData::Frame()
{
	m_iFrame++;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		bool bIsConnected = GetPlayerInfo(i)->IsConnected();

		if (m_bPlayerConnected[i] && !bIsConnected)
		{
			ClearValues(m_PlayerValues[i]);
			m_iSerial++;
		}

		m_bPlayerConnected[i] = bIsConnected;
	}
}

bool CScoreboardData::IsCustomEnabled()
{
	return hud_scoreboard_custom.GetBool();
}

bool CScoreboardData::IsHudEnabled()
{
	return hud_server_elements.GetBool();
}

void CScoreboardData::SetClientCaps(bhl::E_ClientCaps caps)
{
	bool bHasScoreboard = IsEnumFlagSet(caps, bhl::E_ClientCaps::ScoreboardV1);

	if (bHasScoreboard == m_bAcceptMessages)
		return;

	m_bAcceptMessages = bHasScoreboard;
	m_bWaitForReset = bHasScoreboard;

	if (!bHasScoreboard)
		Clear();
}

float CScoreboardData::GetTime()
{
	return gEngfuncs.GetClientTime();
}

bool CScoreboardData::IsActive() const
{
	return m_bConfigReceived && IsCustomEnabled();
}

bool CScoreboardData::IsStdColumnHidden(E_ConfigFlags flag) const
{
	return IsActive() && IsEnumFlagSet(m_ConfigFlags, flag);
}

const CScoreboardData::Column &CScoreboardData::GetColumn(int id) const
{
	if (id < 0 || id >= MAX_COLUMNS)
		return s_EmptyColumn;

	return m_Columns[id];
}

int CScoreboardData::FindColumn(const char *key) const
{
	if (!key)
		return -1;

	for (int i = 0; i < MAX_COLUMNS; i++)
	{
		if (m_Columns[i].bValid && !strcmp(m_Columns[i].szKey, key))
			return i;
	}

	return -1;
}

int CScoreboardData::FindPrimaryColumn() const
{
	for (int i = 0; i < MAX_COLUMNS; i++)
	{
		const Column &column = m_Columns[i];

		if (column.bValid && column.IsPrimary() && !column.IsHidden() && !column.IsBuiltin() && !column.IsBadge())
			return i;
	}

	return -1;
}

int CScoreboardData::GetBadgeColumns(int host, int *ids, int maxIds) const
{
	const Column &hostColumn = GetColumn(host);

	if (!hostColumn.bValid || hostColumn.IsHidden() || hostColumn.IsBuiltin() || hostColumn.IsBadge())
		return 0;

	int count = 0;

	for (int i = 0; i < MAX_COLUMNS && count < maxIds; i++)
	{
		const Column &column = m_Columns[i];

		if (column.bValid && column.IsBadge() && column.iHost == host && !column.IsHidden())
			ids[count++] = i;
	}

	return count;
}

int CScoreboardData::GetSortColumn(int i) const
{
	if (i < 0 || i >= m_iSortColumnCount)
		return -1;

	return m_SortColumns[i];
}

int CScoreboardData::GetFirstSortColumn() const
{
	return m_iSortColumnCount > 0 ? m_SortColumns[0] : -1;
}

const CScoreboardData::Value &CScoreboardData::GetGlobalValue(int col) const
{
	if (col < 0 || col >= MAX_COLUMNS)
		return s_EmptyValue;

	return m_GlobalValues[col];
}

const CScoreboardData::Value &CScoreboardData::GetPlayerValue(int client, int col) const
{
	if (client < 1 || client > MAX_PLAYERS || col < 0 || col >= MAX_COLUMNS || !GetPlayerInfo(client)->IsConnected())
		return s_EmptyValue;

	return m_PlayerValues[client][col];
}

const CScoreboardData::Value &CScoreboardData::GetTeamValue(int team, int col) const
{
	if (!IsTeamNumber(team) || col < 0 || col >= MAX_COLUMNS)
		return s_EmptyValue;

	return m_TeamValues[team][col];
}

const CScoreboardData::List *CScoreboardData::FindList(const char *key) const
{
	if (!key)
		return nullptr;

	for (const List &list : m_Lists)
	{
		if (list.bValid && !strcmp(list.szKey, key))
			return &list;
	}

	return nullptr;
}

bool CScoreboardData::HasRunningClock() const
{
	if (m_iRunningClockSerial == m_iSerial)
		return m_bHasRunningClock;

	auto fnHasClock = [](const Value *values, int count) {
		for (int i = 0; i < count; i++)
		{
			if (values[i].IsRunningClock())
				return true;
		}

		return false;
	};

	bool bHasClock = fnHasClock(m_GlobalValues, MAX_COLUMNS);

	for (int i = 1; i <= MAX_PLAYERS && !bHasClock; i++)
		bHasClock = fnHasClock(m_PlayerValues[i], MAX_COLUMNS);

	for (int i = 1; i <= MAX_TEAMS && !bHasClock; i++)
		bHasClock = fnHasClock(m_TeamValues[i], MAX_COLUMNS);

	for (int i = 0; i < MAX_LISTS && !bHasClock; i++)
		bHasClock = fnHasClock(m_Lists[i].items, m_Lists[i].iCount);

	m_iRunningClockSerial = m_iSerial;
	m_bHasRunningClock = bHasClock;
	return bHasClock;
}

//-----------------------------------------------------
// Order and places
//-----------------------------------------------------
bool CScoreboardData::IsParticipant(int client)
{
	CPlayerInfo *pi = GetPlayerInfoSafe(client);
	return pi && !pi->IsSpectator();
}

int CScoreboardData::ComparePlayers(int a, int b) const
{
	if (a == b)
		return 0;

	for (int i = 0; i < m_iSortColumnCount; i++)
	{
		int col = m_SortColumns[i];
		int result = CompareValues(GetPlayerValue(a, col), GetPlayerValue(b, col), m_Columns[col].IsSortAscending());

		if (result != 0)
			return result;
	}

	int fragsA, deathsA, fragsB, deathsB;
	GetPlayerScore(a, fragsA, deathsA);
	GetPlayerScore(b, fragsB, deathsB);

	if (fragsA != fragsB)
		return fragsA > fragsB ? -1 : 1;

	if (deathsA != deathsB)
		return deathsA < deathsB ? -1 : 1;

	return a < b ? -1 : 1;
}

int CScoreboardData::CompareTeams(int a, int b) const
{
	if (a == b)
		return 0;

	for (int i = 0; i < m_iSortColumnCount; i++)
	{
		int col = m_SortColumns[i];
		int result = CompareValues(GetTeamValue(a, col), GetTeamValue(b, col), m_Columns[col].IsSortAscending());

		if (result != 0)
			return result;
	}

	int fragsA, deathsA, fragsB, deathsB;
	GetTeamScore(a, fragsA, deathsA);
	GetTeamScore(b, fragsB, deathsB);

	if (fragsA != fragsB)
		return fragsA > fragsB ? -1 : 1;

	if (deathsA != deathsB)
		return deathsA < deathsB ? -1 : 1;

	return a < b ? -1 : 1;
}

void CScoreboardData::GetTeamScore(int team, int &frags, int &deaths) const
{
	frags = 0;
	deaths = 0;

	if (!IsTeamNumber(team))
		return;

	CTeamInfo *ti = GetTeamInfo(team);

	if (ti->IsScoreOverriden())
	{
		frags = ti->GetFrags();
		deaths = ti->GetDeaths();
		return;
	}

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfoSafe(i);

		if (pi && pi->GetTeamNumber() == team)
		{
			frags += pi->GetFrags();
			deaths += pi->GetDeaths();
		}
	}
}

int CScoreboardData::GetPlayerPlace(int client) const
{
	if (!IsParticipant(client))
		return 0;

	int col = GetFirstSortColumn();
	int place = 1;

	if (col == -1)
	{
		int frags, deaths;
		GetPlayerScore(client, frags, deaths);

		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			int otherFrags, otherDeaths;
			GetPlayerScore(i, otherFrags, otherDeaths);

			if (IsParticipant(i) && otherFrags > frags)
				place++;
		}

		return place;
	}

	const Value &value = GetPlayerValue(client, col);
	bool bAscending = m_Columns[col].IsSortAscending();

	if (value.IsEmpty())
		return 0;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		if (IsParticipant(i) && CompareValues(GetPlayerValue(i, col), value, bAscending) < 0)
			place++;
	}

	return place;
}

int CScoreboardData::GetTeamPlace(int team) const
{
	bool bHasParticipants[MAX_TEAMS + 1] = {};

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		int playerTeam = GetPlayerTeamNumber(i);

		if (IsParticipant(i) && IsTeamNumber(playerTeam))
			bHasParticipants[playerTeam] = true;
	}

	if (!IsTeamNumber(team) || !bHasParticipants[team])
		return 0;

	int col = GetFirstSortColumn();
	int place = 1;

	if (col == -1)
	{
		int frags, deaths;
		GetTeamScore(team, frags, deaths);

		for (int i = 1; i <= MAX_TEAMS; i++)
		{
			int otherFrags, otherDeaths;
			GetTeamScore(i, otherFrags, otherDeaths);

			if (bHasParticipants[i] && otherFrags > frags)
				place++;
		}

		return place;
	}

	const Value &value = GetTeamValue(team, col);
	bool bAscending = m_Columns[col].IsSortAscending();

	if (value.IsEmpty())
		return 0;

	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		if (bHasParticipants[i] && CompareValues(GetTeamValue(i, col), value, bAscending) < 0)
			place++;
	}

	return place;
}

bool CScoreboardData::IsTeamMode() const
{
	return GetRanking().bTeamMode;
}

CScoreboardData::Subject CScoreboardData::GetFirstRow() const
{
	return GetRanking().firstRow;
}

int CScoreboardData::GetOrderedEntities(Subject *out, int max) const
{
	const Ranking &r = GetRanking();
	int count = max > 0 ? min(r.iCount, max) : 0;
	std::copy(r.entities, r.entities + count, out);
	return count;
}

CScoreboardData::Subject CScoreboardData::GetRankedEntity(const Subject &subject) const
{
	if (subject.IsTeam())
		return subject;

	Subject entity;

	if (!subject.iPlayer)
		return entity;

	if (!IsTeamMode())
	{
		entity.iPlayer = subject.iPlayer;
		return entity;
	}

	// A player without a team has no ranked entity in team mode
	int team = GetPlayerTeamNumber(subject.iPlayer);

	if (IsTeamNumber(team))
		entity.iTeam = team;

	return entity;
}

CScoreboardData::Builtins CScoreboardData::GetBuiltins(const Subject &subject) const
{
	for (const BuiltinsCacheEntry &entry : m_BuiltinsCache)
	{
		if (entry.iFrame == m_iFrame && entry.iSerial == m_iSerial && entry.subject == subject)
			return entry.builtins;
	}

	BuiltinsCacheEntry &entry = m_BuiltinsCache[m_iNextBuiltinsCache];
	m_iNextBuiltinsCache = (m_iNextBuiltinsCache + 1) % BUILTINS_CACHE_SIZE;
	entry.iFrame = m_iFrame;
	entry.iSerial = m_iSerial;
	entry.subject = subject;
	entry.builtins = ComputeBuiltins(subject);
	return entry.builtins;
}

const CScoreboardData::Ranking &CScoreboardData::GetRanking() const
{
	Ranking &r = m_Ranking;

	if (r.iFrame == m_iFrame && r.iSerial == m_iSerial)
		return r;

	r = Ranking();
	r.iFrame = m_iFrame;
	r.iSerial = m_iSerial;

	int sortCol = GetFirstSortColumn();
	bool bTeamHasParticipants[MAX_TEAMS + 1] = {};
	bool bHasTeamParticipant = false;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		int team = GetPlayerTeamNumber(i);

		if (IsParticipant(i) && IsTeamNumber(team))
		{
			bTeamHasParticipants[team] = true;
			bHasTeamParticipant = true;
		}
	}

	// 6.4: with sort keys the team cells decide, without them the players' teams
	if (sortCol != -1)
	{
		for (int i = 1; i <= MAX_TEAMS && !r.bTeamMode; i++)
			r.bTeamMode = bTeamHasParticipants[i] && !GetTeamValue(i, sortCol).IsEmpty();

		r.iSign = m_Columns[sortCol].IsSortAscending() ? -1 : 1;
	}
	else
	{
		r.bTeamMode = bHasTeamParticipant;
	}

	if (r.bTeamMode)
	{
		for (int i = 1; i <= MAX_TEAMS; i++)
		{
			if (bTeamHasParticipants[i])
				r.entities[r.iCount++].iTeam = i;
		}
	}
	else
	{
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			if (IsParticipant(i))
				r.entities[r.iCount++].iPlayer = i;
		}
	}

	std::sort(r.entities, r.entities + r.iCount, [this](const Subject &a, const Subject &b) {
		return CompareEntities(a, b) < 0;
	});

	for (int i = 0; i < r.iCount; i++)
		r.bHasNumber[i] = GetEntityNumber(r.entities[i], sortCol, r.numbers[i]);

	if (r.iCount > 0)
		r.firstRow = r.entities[0];

	return r;
}

CScoreboardData::Builtins CScoreboardData::ComputeBuiltins(const Subject &subject) const
{
	Builtins b;
	const Ranking &r = GetRanking();
	b.iTotal = r.iCount;

	if (subject.IsTeam())
	{
		if (!IsTeamNumber(subject.iTeam))
			return b;

		GetTeamScore(subject.iTeam, b.iFrags, b.iDeaths);
	}
	else
	{
		if (!GetPlayerInfoSafe(subject.iPlayer))
			return b;

		GetPlayerScore(subject.iPlayer, b.iFrags, b.iDeaths);
	}

	b.bHasSubject = true;

	Subject entity = GetRankedEntity(subject);
	int own = -1;

	for (int i = 0; i < r.iCount && own == -1; i++)
	{
		if (r.entities[i] == entity)
			own = i;
	}

	if (own == -1 || !r.bHasNumber[own])
		return b;

	b.bRanked = true;
	b.bHasScore = true;
	b.iScore = r.numbers[own];

	long long ownNumber = static_cast<long long>(r.numbers[own]) * r.iSign;
	long long best = ownNumber;
	long long gap = 0;
	long long lead = -1;
	int rival = -1;
	int rivals = 0;
	bool bTied = false;

	for (int i = 0; i < r.iCount; i++)
	{
		if (i == own || !r.bHasNumber[i])
			continue;

		long long number = static_cast<long long>(r.numbers[i]) * r.iSign;
		rivals++;

		if (number > best)
			best = number;

		if (number > ownNumber)
		{
			b.iPlace++;

			// Among equal gaps the last one in the scoreboard order: the row right above the subject
			long long diff = number - ownNumber;

			if (rival == -1 || diff < gap || (diff == gap && CompareEntities(r.entities[i], r.entities[rival]) > 0))
			{
				rival = i;
				gap = diff;
			}
		}
		else if (number == ownNumber)
		{
			bTied = true;
		}
		else if (lead == -1 || ownNumber - number < lead)
		{
			lead = ownNumber - number;
		}
	}

	b.iPlace++;
	b.iBehind = ClampToInt(best - ownNumber);

	if (rivals == 0)
	{
		b.iState = 0;
	}
	else if (rival != -1)
	{
		b.iState = 1;
		b.rival = r.entities[rival];
		b.iGap = ClampToInt(gap);
	}
	else if (bTied)
	{
		b.iState = 2;
		b.iLead = 0;
	}
	else
	{
		b.iState = 3;
		b.iLead = ClampToInt(lead);
	}

	return b;
}

bool CScoreboardData::GetEntityNumber(const Subject &entity, int sortCol, int &out) const
{
	if (sortCol == -1)
	{
		int deaths;

		if (entity.IsTeam())
			GetTeamScore(entity.iTeam, out, deaths);
		else
			GetPlayerScore(entity.iPlayer, out, deaths);

		return true;
	}

	const Value &value = entity.IsTeam() ? GetTeamValue(entity.iTeam, sortCol) : GetPlayerValue(entity.iPlayer, sortCol);
	return value.GetNumber(out);
}

int CScoreboardData::CompareEntities(const Subject &a, const Subject &b) const
{
	return a.IsTeam() ? CompareTeams(a.iTeam, b.iTeam) : ComparePlayers(a.iPlayer, b.iPlayer);
}

//-----------------------------------------------------
// HUD elements
//-----------------------------------------------------
const CScoreboardData::Element &CScoreboardData::GetElement(int id) const
{
	if (id < 0 || id >= MAX_ELEMENTS)
		return s_EmptyElement;

	return m_Elements[id];
}

bool CScoreboardData::IsElementVisible(int id) const
{
	return GetElement(id).bValid && (m_iVisibleMask & (1u << id));
}

bool CScoreboardData::IsStdPartHidden(E_HudStd part) const
{
	return (m_iHiddenStdParts & static_cast<unsigned int>(part)) && IsHudEnabled();
}

CScoreboardData::Subject CScoreboardData::GetViewerSubject() const
{
	int client = 0;

	switch (g_iUser1)
	{
	case OBS_NONE:
	{
		CPlayerInfo *pi = GetThisPlayerInfo();
		client = pi ? pi->GetIndex() : 0;
		break;
	}
	case OBS_CHASE_LOCKED:
	case OBS_CHASE_FREE:
	case OBS_IN_EYE:
	case OBS_MAP_CHASE:
		client = g_iUser2;
		break;
	default:
		// OBS_ROAMING and OBS_MAP_FREE look at nobody
		break;
	}

	Subject subject;

	if (IsParticipant(client))
		subject.iPlayer = client;

	return subject;
}

bool CScoreboardData::GetElementSubject(int id, Subject &out) const
{
	const Element &element = GetElement(id);
	out = Subject();

	if (!element.bValid)
		return false;

	if (element.HasFlag(E_ElementFlags::Leader))
	{
		out = GetFirstRow();
		return out.IsValid();
	}

	out = GetViewerSubject();
	return out.IsValid() || !element.HasFlag(E_ElementFlags::Subject);
}

const CScoreboardData::Value &CScoreboardData::GetSubjectValue(const Subject &subject, int col) const
{
	if (subject.iPlayer)
	{
		const Value &value = GetPlayerValue(subject.iPlayer, col);

		if (!value.IsEmpty())
			return value;

		// The player's team cells, in team mode or not (8.10)
		const Value &teamValue = GetTeamValue(GetPlayerTeamNumber(subject.iPlayer), col);

		if (!teamValue.IsEmpty())
			return teamValue;
	}
	else if (subject.iTeam)
	{
		const Value &value = GetTeamValue(subject.iTeam, col);

		if (!value.IsEmpty())
			return value;
	}

	return GetGlobalValue(col);
}

const TemplateRef *CScoreboardData::GetValueRef(int elementId) const
{
	const Element &element = GetElement(elementId);

	if (!element.bValid || !element.valueRef.IsSet() || element.valueRef.builtin == E_Builtin::Value)
		return nullptr;

	return &element.valueRef;
}

bool CScoreboardData::ResolveRef(const TemplateRef &ref, const Subject &subject, int elementId, TemplateValue &out) const
{
	out = TemplateValue();

	if (!ref.IsSet() || ref.builtin == E_Builtin::Unknown)
		return false;

	Subject target = subject;

	if (ref.bFirstRow)
	{
		target = GetFirstRow();

		if (!target.IsValid())
			return false;
	}

	switch (ref.builtin)
	{
	case E_Builtin::None:
		if (!ValueToTemplate(GetSubjectValue(target, FindColumn(ref.szKey)), out))
			return false;

		break;
	case E_Builtin::Value:
	{
		// One level only. With -1 a nested "@value" can't resolve, so this never recurses further.
		const TemplateRef *valueRef = GetValueRef(elementId);

		if (!valueRef || !ResolveRef(*valueRef, target, -1, out))
			return false;

		break;
	}
	case E_Builtin::Name:
		if (!WriteSubjectName(target, out.wszText, sizeof(out.wszText)))
			return false;

		out.type = E_TemplateValueType::Text;
		break;
	case E_Builtin::Rival:
	{
		Builtins b = GetBuiltins(target);

		if (!b.bRanked || b.iState != 1 || !WriteSubjectName(b.rival, out.wszText, sizeof(out.wszText)))
			return false;

		out.type = E_TemplateValueType::Text;
		break;
	}
	default:
		if (!GetBuiltinNumber(ref.builtin, GetBuiltins(target), out.iValue))
			return false;

		out.type = E_TemplateValueType::Int;
		break;
	}

	return ApplyRefFormat(ref.format, out);
}

bool CScoreboardData::GetElementValue(int id, const Subject &subject, Value &out) const
{
	out = Value();
	const TemplateRef *ref = GetValueRef(id);

	if (!ref || ref->builtin == E_Builtin::Unknown)
		return false;

	Subject target = subject;

	if (ref->bFirstRow)
	{
		target = GetFirstRow();

		if (!target.IsValid())
			return false;
	}

	int number = 0;

	if (ref->builtin == E_Builtin::None)
	{
		out = GetSubjectValue(target, FindColumn(ref->szKey));
	}
	else if (GetBuiltinNumber(ref->builtin, GetBuiltins(target), number))
	{
		out.type = E_ValueType::Int;
		out.iValue = number;
	}

	return !out.IsEmpty() && FormatValue(ref->format, out);
}

bool CScoreboardData::FormatValue(E_RefFormat format, Value &value)
{
	if (format == E_RefFormat::None)
		return true;

	int number = 0;
	bool bIsPart = value.type == E_ValueType::Progress && GetProgressPart(format, value.iValue, value.iMax, number);
	value = Value();

	if (!bIsPart)
		return false;

	value.type = E_ValueType::Int;
	value.iValue = number;
	return true;
}

int CScoreboardData::GetElementVariant(int id, const Subject &subject) const
{
	const Element &element = GetElement(id);

	if (!element.bValid)
		return -1;

	switch (element.kind)
	{
	case E_ElementKind::Icon:
	case E_ElementKind::Meter:
		return 0;
	case E_ElementKind::Text:
		break;
	default:
		return -1;
	}

	int variant = 0;

	if (element.selectRef.IsSet())
	{
		TemplateValue value;

		if (!ResolveRef(element.selectRef, subject, id, value) || !value.GetNumber(variant))
			return -1;
	}

	if (variant < 0 || variant >= MAX_VARIANTS || !m_Templates[id][variant].IsSet())
		return -1;

	return variant;
}

bool CScoreboardData::GetElementText(int id, const Subject &subject, wchar_t *buf, int bufSizeBytes) const
{
	if (!buf || bufSizeBytes < static_cast<int>(sizeof(wchar_t)))
		return false;

	buf[0] = L'\0';
	int variant = GetElementVariant(id, subject);

	if (variant < 0)
		return false;

	const CTemplate &tpl = m_Templates[id][variant];

	if (!tpl.IsSet())
	{
		if (m_Elements[id].kind != E_ElementKind::Meter)
			return false;

		return EvaluateTemplate(GetMeterCounterTemplate(), subject, id, buf, bufSizeBytes);
	}

	return EvaluateTemplate(tpl, subject, id, buf, bufSizeBytes);
}

const CTemplate *CScoreboardData::GetTemplate(int id, int variant) const
{
	if (id < 0 || id >= MAX_ELEMENTS || variant < 0 || variant >= MAX_VARIANTS || !m_Templates[id][variant].IsSet())
		return nullptr;

	return &m_Templates[id][variant];
}

bool CScoreboardData::EvaluateTemplate(const CTemplate &tpl, const Subject &subject, int elementId, wchar_t *buf, int bufSizeBytes, unsigned int *pMissing, const TemplateExtras *pExtras) const
{
	if (pMissing)
		*pMissing = 0;

	int bufLen = bufSizeBytes / static_cast<int>(sizeof(wchar_t));

	if (!buf || bufLen <= 0)
		return false;

	CStoreTemplateContext ctx(*this, subject, elementId, pExtras);
	return tpl.Evaluate(ctx, buf, bufLen, pMissing);
}

int CScoreboardData::GetHudLines(int elementId, const HudLine **out) const
{
	int count = 0;

	for (int i = 0; i < MAX_HUD_LINES; i++)
	{
		const HudLine &line = m_HudLines[(m_iNextHudLine + i) % MAX_HUD_LINES];

		if (line.iElement == elementId)
			out[count++] = &line;
	}

	return count;
}

int CScoreboardData::GetBannerRow(E_Place place, wchar_t *buf, int bufSizeBytes) const
{
	if (!buf || bufSizeBytes < static_cast<int>(sizeof(wchar_t)))
		return -1;

	buf[0] = L'\0';

	if (!IsBannerPlace(place))
		return -1;

	for (int i = 0; i < MAX_ELEMENTS; i++)
	{
		const Element &element = m_Elements[i];
		Subject subject;

		if (!element.bValid || element.kind != E_ElementKind::Text || element.place != place || !IsElementVisible(i) || !GetElementSubject(i, subject))
			continue;

		if (GetElementText(i, subject, buf, bufSizeBytes))
			return i;
	}

	return -1;
}

//-----------------------------------------------------
// Text helpers
//-----------------------------------------------------
void CScoreboardData::LocalizeString(const char *str, wchar_t *buf, int bufSizeBytes)
{
	if (!buf || bufSizeBytes < static_cast<int>(sizeof(wchar_t)))
		return;

	if (!str)
		str = "";

	if (IsClientToken(str))
	{
		const wchar_t *localized = g_pVGuiLocalize->Find(str);

		if (localized)
		{
			V_wcsncpy(buf, localized, bufSizeBytes);
			return;
		}

		str++;
	}

	Q_UTF8ToWString(str, buf, bufSizeBytes, STRINGCONVERT_REPLACE);
}

void CScoreboardData::GetIconName(const Value &value, wchar_t *buf, int bufSizeBytes)
{
	if (!buf || bufSizeBytes < static_cast<int>(sizeof(wchar_t)))
		return;

	if (!strncmp(value.szSprite, "d_", 2))
	{
		char token[64];
		snprintf(token, sizeof(token), "BHL_Weapon_%s", value.szSprite + 2);
		const wchar_t *name = g_pVGuiLocalize->Find(token);

		if (name)
		{
			V_wcsncpy(buf, name, bufSizeBytes);
			return;
		}
	}

	LocalizeString(value.szText, buf, bufSizeBytes);
}

//-----------------------------------------------------
// Debug commands
//-----------------------------------------------------
void CScoreboardData::DumpCommand()
{
	static const char *const s_KindNames[] = { "None", "Text", "Meter", "Strip", "Icon", "Board", "Feed", "Box" };
	static_assert(std::size(s_KindNames) == static_cast<size_t>(LAST_ELEMENT_KIND) + 1, "every kind needs a name");
	static const char *const s_PlaceNames[] = { "Screen", "Parent", "NumbersRow", "BannerTop", "BannerBottom", "ParentLeft", "ParentRight" };

	ConPrintf("Scoreboard: active %d, config %d, hud_scoreboard_custom %d, hud_server_elements %d\n",
	    IsActive(), m_bConfigReceived, IsCustomEnabled(), IsHudEnabled());
	ConPrintf("Messages: accepted %d, waiting for Reset %d; serial %d, schema %d, HUD %d\n",
	    m_bAcceptMessages, m_bWaitForReset, m_iSerial, m_iSchemaSerial, m_iHudSerial);
	ConPrintf("Config: flags 0x%x, title \"%s\", tag \"%s\"\n", static_cast<unsigned int>(m_ConfigFlags), m_szTitle, m_szTag);

	ConPrintf("Columns:\n");

	for (int i = 0; i < MAX_COLUMNS; i++)
	{
		const Column &column = m_Columns[i];

		if (column.bValid)
		{
			ConPrintf("  %2d: key \"%s\", title \"%s\", display %d, flags 0x%x, width %d, sort 0x%02x, host %d\n",
			    i, column.szKey, column.szTitle, static_cast<int>(column.display), static_cast<unsigned int>(column.flags), column.iWidth, column.iSort, column.iHost);
		}
	}

	ConPrintf("Sort keys:");

	for (int i = 0; i < m_iSortColumnCount; i++)
		ConPrintf(" %s", m_Columns[m_SortColumns[i]].szKey);

	ConPrintf("\n");

	auto fnPrintCells = [this](const char *target, const Value *values) {
		bool bHasHeader = false;

		for (int i = 0; i < MAX_COLUMNS; i++)
		{
			if (values[i].IsEmpty())
				continue;

			if (!bHasHeader)
			{
				ConPrintf("%s:\n", target);
				bHasHeader = true;
			}

			char buf[128];
			ValueToDebugString(values[i], buf, sizeof(buf));
			ConPrintf("  %2d %-15s %s\n", i, m_Columns[i].szKey, buf);
		}
	};

	fnPrintCells("Whole game", m_GlobalValues);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i);
		char target[128];
		snprintf(target, sizeof(target), "Player %d \"%s\"", i, pi->IsConnected() ? pi->GetDisplayName(true) : "(disconnected)");
		fnPrintCells(target, m_PlayerValues[i]);
	}

	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		char target[128];
		snprintf(target, sizeof(target), "Team %d \"%s\"", i, GetTeamInfo(i)->GetDisplayName());
		fnPrintCells(target, m_TeamValues[i]);
	}

	ConPrintf("Lists:\n");

	for (int i = 0; i < MAX_LISTS; i++)
	{
		const List &list = m_Lists[i];

		if (!list.bValid)
			continue;

		ConPrintf("  %d: key \"%s\", %d items\n", i, list.szKey, list.iCount);

		for (int j = 0; j < list.iCount; j++)
		{
			char buf[128];
			ValueToDebugString(list.items[j], buf, sizeof(buf));
			ConPrintf("    %2d: %s\n", j, buf);
		}
	}

	ConPrintf("Elements:\n");

	for (int i = 0; i < MAX_ELEMENTS; i++)
	{
		const Element &element = m_Elements[i];

		if (!element.bValid)
			continue;

		ConPrintf("  %2d: %s, visible %d, flags 0x%x, x %d, y %d, align %d/%d, %s, parent %d, gap %d, color %d %d %d %d, font %d\n",
		    i, s_KindNames[static_cast<int>(element.kind)], IsElementVisible(i), static_cast<unsigned int>(element.flags), element.x, element.y,
		    static_cast<int>(element.hAlign), static_cast<int>(element.vAlign), s_PlaceNames[static_cast<int>(element.place)], element.iParent,
		    element.iGap, element.r, element.g, element.b, element.a, static_cast<int>(element.font));
		ConPrintf("      select \"%s\", value \"%s\", list \"%s\"", element.szSelect, element.szValue, element.szList);

		for (int j = 0; j <= MAX_PARAM_ID; j++)
		{
			const ParamInfo *info = FindParamInfo(j);
			int value = 0;

			if (info && element.GetParam(info->id, value))
				ConPrintf(", %s %d", info->pszName, value);
		}

		for (int j = 0; j < MAX_COLOR_SLOTS; j++)
		{
			unsigned char color[4];

			if (element.GetSlotColor(j, color))
				ConPrintf(", slot %d color %d %d %d %d", j, color[0], color[1], color[2], color[3]);
		}

		ConPrintf("\n");

		for (int j = 0; j < MAX_VARIANTS; j++)
		{
			const CTemplate &tpl = m_Templates[i][j];

			if (!tpl.IsSet())
				continue;

			char source[MAX_EVAL_LEN * 4];
			Q_WStringToUTF8(tpl.GetSource(), source, sizeof(source), STRINGCONVERT_REPLACE);

			if (tpl.HasError())
				ConPrintf("      %d: \"%s\", error: %s\n", j, source, tpl.GetError());
			else
				ConPrintf("      %d: \"%s\"\n", j, source);
		}
	}

	ConPrintf("HUD state: visible 0x%08x, hidden parts 0x%x\n", m_iVisibleMask, m_iHiddenStdParts);

	char buf[160];
	Subject viewer = GetViewerSubject();
	SubjectToDebugString(viewer, buf, sizeof(buf));
	ConPrintf("Subject: %s; team mode %d; ", buf, IsTeamMode());
	SubjectToDebugString(GetFirstRow(), buf, sizeof(buf));
	ConPrintf("first row: %s\n", buf);

	Builtins b = GetBuiltins(viewer);
	ConPrintf("Built-in values: total %d", b.iTotal);

	if (b.bHasSubject)
		ConPrintf(", frags %d, deaths %d", b.iFrags, b.iDeaths);

	if (b.bRanked)
		ConPrintf(", place %d, state %d, behind %d, score %d", b.iPlace, b.iState, b.iBehind, b.iScore);

	if (b.bRanked && b.iState == 1)
	{
		SubjectToDebugString(b.rival, buf, sizeof(buf));
		ConPrintf(", rival %s, gap %d", buf, b.iGap);
	}

	if (b.bRanked && (b.iState == 2 || b.iState == 3))
		ConPrintf(", lead %d", b.iLead);

	ConPrintf("\n");
}

void CScoreboardData::TestCommand()
{
	auto fnSend = [this](CMsgWriter &msg) {
		if (msg.IsOverflowed())
			ConPrintf("hud_scoreboard_test: message %d is too long\n", msg.GetData()[0]);
		else
			MsgFunc_BhlScore(MSG_NAME, msg.GetSize(), msg.GetData());
	};

	if (!m_bAcceptMessages)
	{
		ConPrintf("hud_scoreboard_test: needs hud_scoreboard_custom 1 or hud_server_elements 1\n");
		return;
	}

	CMsgWriter reset(E_Op::Reset);
	reset.WriteByte(PROTOCOL_VERSION);
	fnSend(reset);

	if (gEngfuncs.Cmd_Argc() >= 2 && !strcmp(gEngfuncs.Cmd_Argv(1), "off"))
	{
		ConPrintf("Scoreboard test data removed.\n");
		return;
	}

	enum
	{
		COL_RANK = 0,
		COL_STAGE,
		COL_ITEM,
		COL_PROGRESS,
		COL_SCORE,
		COL_FINAL,
	};

	struct TestColumn
	{
		const char *pszKey;
		const char *pszTitle;
		E_Display display;
		E_ColumnFlags flags;
		int iWidth;
		int iSort;
		int iHost;
	};

	static const TestColumn s_Columns[] = {
		{ "rank", "", E_Display::Builtin, E_ColumnFlags::None, 0, 0, NO_ID },
		{ "stage", "#BHL_SB_Level", E_Display::Steps, static_cast<E_ColumnFlags>(static_cast<unsigned int>(E_ColumnFlags::MarkLast) | static_cast<unsigned int>(E_ColumnFlags::Primary)), 60, 0, NO_ID },
		{ "item", "#BHL_SB_Weapon", E_Display::Icon, E_ColumnFlags::None, 120, 0, NO_ID },
		{ "progress", "#BHL_SB_Progress", E_Display::Meter, E_ColumnFlags::None, 90, 0, NO_ID },
		{ "score", "", E_Display::Text, E_ColumnFlags::Hidden, 0, 1, NO_ID },
		{ "final", "", E_Display::Badge, E_ColumnFlags::None, 0, 0, COL_ITEM },
	};

	struct TestItem
	{
		const char *pszSprite;
		const char *pszName;
		int iKills;
	};

	static const TestItem s_Items[] = {
		{ "d_9mmhandgun", "Glock", 2 },
		{ "d_357", "Python", 2 },
		{ "d_9mmAR", "MP5", 3 },
		{ "d_shotgun", "Shotgun", 2 },
		{ "d_crossbow", "Crossbow", 2 },
		{ "d_rpg_rocket", "RPG", 2 },
		{ "d_gauss", "Gauss", 2 },
		{ "d_egon", "Egon", 12 },
		{ "d_hornet", "Hornet", 2 },
		{ "d_snark", "Snark", 2 },
		{ "d_tripmine", "Tripmine", 1 },
		{ "d_satchel", "Satchel", 1 },
		{ "d_grenade", "Grenade", 1 },
		{ "d_crowbar", "Crowbar", 1 },
	};

	constexpr int ITEM_COUNT = static_cast<int>(std::size(s_Items));

	CMsgWriter config(E_Op::Config);
	config.WriteShort(static_cast<int>(E_ConfigFlags::All));
	config.WriteString("");
	config.WriteString("TEST");
	fnSend(config);

	for (int i = 0; i < static_cast<int>(std::size(s_Columns)); i++)
	{
		const TestColumn &column = s_Columns[i];
		CMsgWriter msg(E_Op::Column);
		msg.WriteByte(i);
		msg.WriteByte(static_cast<int>(column.display));
		msg.WriteShort(static_cast<int>(column.flags));
		msg.WriteByte(column.iWidth);
		msg.WriteByte(column.iSort);
		msg.WriteString(column.pszKey);
		msg.WriteString(column.pszTitle);
		msg.WriteByte(column.iHost);
		fnSend(msg);
	}

	// A long list is split into several messages
	constexpr int ITEMS_PER_MESSAGE = 6;

	for (int start = 0; start < ITEM_COUNT; start += ITEMS_PER_MESSAGE)
	{
		CMsgWriter list(E_Op::List);
		list.WriteByte(0);
		list.WriteByte(ITEM_COUNT);
		list.WriteByte(start);
		list.WriteString("items");

		for (int i = start; i < start + ITEMS_PER_MESSAGE && i < ITEM_COUNT; i++)
			list.WriteIcon(s_Items[i].pszSprite, s_Items[i].pszName);

		fnSend(list);
	}

	// The first player is on the last level, the third and the fourth are tied
	auto fnWriteCells = [&](CMsgWriter &msg, int seed) {
		int level = ITEM_COUNT - 1 - (seed * 3) % ITEM_COUNT;
		int kills = seed % s_Items[level].iKills;
		int score = kills;

		for (int i = 0; i < level; i++)
			score += s_Items[i].iKills;

		msg.WriteByte(COL_FINAL - COL_STAGE + 1);
		msg.WriteByte(COL_STAGE);
		msg.WriteProgress(level + 1, ITEM_COUNT);
		msg.WriteByte(COL_ITEM);
		msg.WriteIcon(s_Items[level].pszSprite, s_Items[level].pszName);
		msg.WriteByte(COL_PROGRESS);
		msg.WriteProgress(kills, s_Items[level].iKills);
		msg.WriteByte(COL_SCORE);
		msg.WriteInt(score);
		msg.WriteByte(COL_FINAL);

		if (level == ITEM_COUNT - 1)
		{
			msg.WriteText("#BHL_SB_Final");
		}
		else
		{
			msg.WriteByte(static_cast<int>(E_ValueType::None));
			msg.WriteByte(0);
		}
	};

	int playerCount = 0;
	int teamSeed[MAX_TEAMS + 1];
	std::fill(std::begin(teamSeed), std::end(teamSeed), -1);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfoSafe(i);

		if (!pi)
			continue;

		int seed = playerCount == 3 ? 2 : playerCount;
		CMsgWriter msg(E_Op::Values);
		msg.WriteByte(TARGET_PLAYER_FIRST + i - 1);
		fnWriteCells(msg, seed);
		fnSend(msg);

		// A team copies the cells of its first player
		int team = pi->GetTeamNumber();

		if (IsTeamNumber(team) && teamSeed[team] == -1)
			teamSeed[team] = seed;

		playerCount++;
	}

	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		if (teamSeed[i] == -1)
			continue;

		CMsgWriter msg(E_Op::Values);
		msg.WriteByte(TARGET_TEAM_BASE + i);
		fnWriteCells(msg, teamSeed[i]);
		fnSend(msg);
	}

	enum
	{
		EL_PLACE = 0,
		EL_RIVAL,
		EL_METER,
		EL_STRIP,
		EL_BANNER_TOP,
		EL_BANNER_BOTTOM,
		EL_COUNT,
	};

	constexpr unsigned int SCREEN_FLAGS = static_cast<unsigned int>(E_ElementFlags::HudColor) | static_cast<unsigned int>(E_ElementFlags::Subject) | static_cast<unsigned int>(E_ElementFlags::NoScoreboard);
	constexpr unsigned int BANNER_FLAGS = static_cast<unsigned int>(E_ElementFlags::HudColor) | static_cast<unsigned int>(E_ElementFlags::Leader);

	auto fnElement = [&](int id, E_ElementKind kind, unsigned int flags, float y, E_Place place, int parent, int gap, E_Font font, const char *select, const char *value, const char *listKey) {
		CMsgWriter msg(E_Op::Element);
		msg.WriteByte(id);
		msg.WriteByte(static_cast<int>(kind));
		msg.WriteShort(static_cast<int>(flags));
		msg.WriteShort(POS_CENTER);
		msg.WriteShort(PositionToWire(y));
		msg.WriteByte(0);
		msg.WriteByte(static_cast<int>(place));
		msg.WriteByte(parent);
		msg.WriteByte(gap);

		// rgb follows hud_color, alpha 255
		for (int color : { 0, 0, 0, 255 })
			msg.WriteByte(color);

		msg.WriteByte(static_cast<int>(font));
		msg.WriteString(select);
		msg.WriteString(value);
		msg.WriteString(listKey);
		msg.WriteByte(0);
		fnSend(msg);
	};

	auto fnTemplate = [&](int id, int variant, const char *text) {
		CMsgWriter msg(E_Op::Template);
		msg.WriteByte(id);
		msg.WriteByte(variant);
		msg.WriteString(text);
		fnSend(msg);
	};

	fnElement(EL_PLACE, E_ElementKind::Text, SCREEN_FLAGS, 0.025f, E_Place::Screen, NO_ID, 0, E_Font::Large, "", "", "");
	fnElement(EL_RIVAL, E_ElementKind::Text, SCREEN_FLAGS, -1.0f, E_Place::Parent, EL_PLACE, 2, E_Font::Text, "@state", "", "");
	fnElement(EL_METER, E_ElementKind::Meter, SCREEN_FLAGS, -1.0f, E_Place::Parent, EL_RIVAL, 6, E_Font::Text, "", "progress", "");
	fnElement(EL_STRIP, E_ElementKind::Strip, SCREEN_FLAGS, -1.0f, E_Place::NumbersRow, NO_ID, 3, E_Font::Text, "", "stage", "items");
	fnElement(EL_BANNER_TOP, E_ElementKind::Text, BANNER_FLAGS, -1.0f, E_Place::BannerTop, NO_ID, 0, E_Font::Text, "stage:left", "", "");
	fnElement(EL_BANNER_BOTTOM, E_ElementKind::Text, BANNER_FLAGS, -1.0f, E_Place::BannerBottom, NO_ID, 0, E_Font::Text, "stage:left", "", "");

	fnTemplate(EL_PLACE, 0, "#BHL_HUD_Place");
	fnTemplate(EL_RIVAL, 0, "#BHL_HUD_Waiting");
	fnTemplate(EL_RIVAL, 1, "#BHL_HUD_Overtake");
	fnTemplate(EL_RIVAL, 2, "#BHL_HUD_Tied");
	fnTemplate(EL_RIVAL, 3, "#BHL_HUD_Lead");
	fnTemplate(EL_BANNER_TOP, 0, "#BHL_SB_Final");
	fnTemplate(EL_BANNER_BOTTOM, 0, "{@name:20} \xC2\xB7 {item}");

	CMsgWriter state(E_Op::HudState);
	state.WriteLong((1 << EL_COUNT) - 1);
	state.WriteShort(static_cast<int>(E_HudStd::Timer) | static_cast<int>(E_HudStd::NextMap) | static_cast<int>(E_HudStd::CustomTimers));
	fnSend(state);

	ConPrintf("Scoreboard test data: %d players.\n", playerCount);
}

void CScoreboardData::EvalCommand()
{
	if (gEngfuncs.Cmd_Argc() < 2)
	{
		ConPrintf("Usage: hud_server_eval \"<template>\"\n");
		return;
	}

	// The engine splits unquoted text at braces and colons, so the template should be quoted
	std::string text = gEngfuncs.Cmd_Argv(1);

	for (int i = 2; i < gEngfuncs.Cmd_Argc(); i++)
	{
		text += ' ';
		text += gEngfuncs.Cmd_Argv(i);
	}

	CTemplate tpl;

	if (!tpl.Parse(text.c_str(), &s_TemplateLocalizer))
	{
		ConPrintf("Parse error: %s\n", tpl.GetError());
		return;
	}

	Subject subject = GetViewerSubject();
	wchar_t wbuf[TEMPLATE_BUF_LEN];
	char buf[TEMPLATE_BUF_LEN * 4];
	unsigned int missing = 0;
	EvaluateTemplate(tpl, subject, -1, wbuf, sizeof(wbuf), &missing);

	SubjectToDebugString(subject, buf, sizeof(buf));
	ConPrintf("Subject: %s\n", buf);
	Q_WStringToUTF8(wbuf, buf, sizeof(buf), STRINGCONVERT_REPLACE);
	ConPrintf("Result: \"%s\"\n", buf);

	for (int i = 0; i < tpl.GetPlaceholderCount(); i++)
	{
		if (missing & (1u << i))
		{
			Q_WStringToUTF8(tpl.GetPlaceholderSource(i), buf, sizeof(buf), STRINGCONVERT_REPLACE);
			ConPrintf("Missing: %s\n", buf);
		}
	}
}

//-----------------------------------------------------
// Messages
//-----------------------------------------------------
void CScoreboardData::Clear()
{
	m_bConfigReceived = false;
	m_ConfigFlags = E_ConfigFlags::None;
	m_szTitle[0] = '\0';
	m_szTag[0] = '\0';

	for (Column &column : m_Columns)
		column = Column();

	ClearValues(m_GlobalValues);

	for (Value *values : m_PlayerValues)
		ClearValues(values);

	for (Value *values : m_TeamValues)
		ClearValues(values);

	for (List &list : m_Lists)
		list = List();

	for (int i = 0; i < MAX_ELEMENTS; i++)
		RemoveElement(i);

	m_iVisibleMask = 0;
	m_iHiddenStdParts = 0;
	m_iSortColumnCount = 0;
	m_iSerial++;
	m_iSchemaSerial++;
	m_iHudSerial++;
	m_iResetCount++;
}

void CScoreboardData::UpdateSortColumns()
{
	m_iSortColumnCount = 0;

	for (int i = 0; i < MAX_COLUMNS; i++)
	{
		const Column &column = m_Columns[i];

		if (column.bValid && !column.IsBuiltin() && !column.IsBadge() && column.GetSortPriority() > 0)
			m_SortColumns[m_iSortColumnCount++] = i;
	}

	std::stable_sort(m_SortColumns, m_SortColumns + m_iSortColumnCount, [this](int a, int b) {
		return m_Columns[a].GetSortPriority() < m_Columns[b].GetSortPriority();
	});
}

CScoreboardData::Value *CScoreboardData::GetTargetValues(int target)
{
	if (target == TARGET_GLOBAL)
		return m_GlobalValues;

	if (target >= TARGET_PLAYER_FIRST && target <= TARGET_PLAYER_LAST)
		return m_PlayerValues[target - TARGET_PLAYER_FIRST + 1];

	if (target >= TARGET_TEAM_FIRST && target <= TARGET_TEAM_LAST)
		return m_TeamValues[target - TARGET_TEAM_BASE];

	return nullptr;
}

void CScoreboardData::RemoveElement(int id)
{
	m_Elements[id] = Element();
	m_iElementRemovals[id]++;

	for (CTemplate &tpl : m_Templates[id])
		tpl.Clear();

	for (HudLine &line : m_HudLines)
	{
		if (line.iElement == id)
			line = HudLine();
	}
}

int CScoreboardData::MsgFunc_BhlScore(const char *pszName, int iSize, void *pbuf)
{
	if (!m_bAcceptMessages || !pbuf)
		return 1;

	CMsgReader reader(pbuf, iSize);
	int op = 0;

	if (!reader.ReadByte(op))
		return 1;

	// After ScoreboardV1 came back only a full sync, which starts with Reset, may fill the store
	if (m_bWaitForReset && op != static_cast<int>(E_Op::Reset))
		return 1;

	switch (static_cast<E_Op>(op))
	{
	case E_Op::Reset:
		ReadReset(reader);
		break;
	case E_Op::Config:
		ReadConfig(reader);
		break;
	case E_Op::Column:
		ReadColumn(reader);
		break;
	case E_Op::Values:
		ReadValues(reader);
		break;
	case E_Op::List:
		ReadList(reader);
		break;
	case E_Op::Element:
		ReadElement(reader);
		break;
	case E_Op::Template:
		ReadTemplate(reader);
		break;
	case E_Op::HudState:
		ReadHudState(reader);
		break;
	case E_Op::Event:
		ReadEvent(reader);
		break;
	default:
		break;
	}

	return 1;
}

void CScoreboardData::ReadReset(CMsgReader &reader)
{
	int version = 0;

	if (!reader.ReadByte(version) || version < 1)
		return;

	Clear();
	m_bWaitForReset = false;
}

void CScoreboardData::ReadConfig(CMsgReader &reader)
{
	int flags = 0;

	if (!reader.ReadWord(flags))
		return;

	// A missing title or tag is ""
	reader.ReadString(m_szTitle);
	reader.ReadString(m_szTag);
	m_bConfigReceived = true;
	m_ConfigFlags = static_cast<E_ConfigFlags>(flags & static_cast<int>(E_ConfigFlags::All));
	m_iSerial++;
	m_iSchemaSerial++;
}

void CScoreboardData::ReadColumn(CMsgReader &reader)
{
	int id = 0;
	int display = 0;
	int flags = 0;
	int width = 0;
	int sort = 0;
	int host = NO_ID;
	Column column;

	if (!reader.ReadByte(id) || !reader.ReadByte(display) || !reader.ReadWord(flags) || !reader.ReadByte(width) || !reader.ReadByte(sort)
	    || !reader.ReadString(column.szKey) || id >= MAX_COLUMNS)
		return;

	// A missing title is "", a missing host is NO_ID
	reader.ReadString(column.szTitle);
	reader.ReadByte(host);

	column.bValid = true;
	column.display = display <= static_cast<int>(E_Display::Badge) ? static_cast<E_Display>(display) : E_Display::Text;
	column.flags = static_cast<E_ColumnFlags>(flags & static_cast<int>(E_ColumnFlags::All));
	column.iWidth = width;
	column.iSort = sort;
	column.iHost = column.IsBadge() && host < MAX_COLUMNS ? host : -1;
	m_Columns[id] = column;
	UpdateSortColumns();
	m_iSerial++;
	m_iSchemaSerial++;
}

bool CScoreboardData::ReadValue(CMsgReader &reader, Value &value, int &type)
{
	int len = 0;
	value = Value();

	if (!reader.ReadByte(type) || !reader.ReadByte(len) || len > reader.GetRemaining())
		return false;

	CMsgReader payload = reader.ReadBlock(len);
	bool bValid = false;

	switch (static_cast<E_ValueType>(type))
	{
	case E_ValueType::Int:
		if (len == 1)
			bValid = payload.ReadChar(value.iValue);
		else if (len == 2)
			bValid = payload.ReadShort(value.iValue);
		else if (len == 4)
			bValid = payload.ReadLong(value.iValue);
		break;
	case E_ValueType::Text:
		bValid = len <= MAX_TEXT_LEN + 1 && payload.ReadString(value.szText);
		break;
	case E_ValueType::Icon:
		bValid = len <= 2 * (MAX_TEXT_LEN + 1) && payload.ReadString(value.szSprite) && payload.ReadString(value.szText);
		break;
	case E_ValueType::Progress:
		if (len == 2)
			bValid = payload.ReadByte(value.iValue) && payload.ReadByte(value.iMax);
		else if (len == 4)
			bValid = payload.ReadWord(value.iValue) && payload.ReadWord(value.iMax);
		break;
	case E_ValueType::Clock:
	{
		int flags = 0;
		bValid = len == CLOCK_VALUE_LEN && payload.ReadLong(value.iValue) && payload.ReadByte(flags);
		value.clockFlags = static_cast<E_ClockFlags>(flags & static_cast<int>(E_ClockFlags::All));
		value.flClockTime = GetTime();
		break;
	}
	default:
		break;
	}

	if (bValid)
		value.type = static_cast<E_ValueType>(type);
	else
		value = Value();

	return true;
}

void CScoreboardData::ReadValues(CMsgReader &reader)
{
	int target = 0;
	int count = 0;

	while (reader.ReadByte(target) && reader.ReadByte(count))
	{
		Value *values = GetTargetValues(target);

		for (int i = 0; i < count; i++)
		{
			int col = 0;
			int type = 0;
			Value value;

			// A message that ends inside a cell keeps the cells already applied
			if (!reader.ReadByte(col) || !ReadValue(reader, value, type))
				return;

			if (!values)
				continue;

			if (col == ALL_COLUMNS && type == static_cast<int>(E_ValueType::None))
			{
				ClearValues(values);
				m_iSerial++;
			}
			else if (col < MAX_COLUMNS)
			{
				values[col] = value;
				m_iSerial++;
			}
		}
	}
}

void CScoreboardData::ReadList(CMsgReader &reader)
{
	int id = 0;
	int total = 0;
	int start = 0;
	char key[MAX_KEY_LEN + 1];

	if (!reader.ReadByte(id) || !reader.ReadByte(total) || !reader.ReadByte(start) || !reader.ReadString(key) || id >= MAX_LISTS)
		return;

	List &list = m_Lists[id];
	m_iSerial++;

	if (total == 0)
	{
		list = List();
		return;
	}

	if (!list.bValid || strcmp(list.szKey, key))
	{
		list = List();
		list.bValid = true;
		V_strcpy_safe(list.szKey, key);
	}

	total = min(total, MAX_LIST_ITEMS);

	for (int i = total; i < list.iCount; i++)
		list.items[i] = Value();

	list.iCount = total;

	for (int i = start; reader.GetRemaining() > 0; i++)
	{
		int type = 0;
		Value value;

		if (!ReadValue(reader, value, type))
			break;

		if (i < total)
			list.items[i] = value;
	}
}

void CScoreboardData::ReadElement(CMsgReader &reader)
{
	int id = 0;
	int kind = 0;

	if (!reader.ReadByte(id) || !reader.ReadByte(kind))
		return;

	if (!IsKnownKind(kind))
	{
		// None or an unknown kind removes; 0xFF removes all only with None
		if (id == ALL_ELEMENTS && kind == static_cast<int>(E_ElementKind::None))
		{
			for (int i = 0; i < MAX_ELEMENTS; i++)
				RemoveElement(i);
		}
		else if (id < MAX_ELEMENTS)
		{
			RemoveElement(id);
		}
		else
		{
			return;
		}

		m_iSerial++;
		m_iHudSerial++;
		return;
	}

	int flags = 0;
	int x = 0;
	int y = 0;
	int align = 0;
	int place = 0;
	int parent = 0;
	int gap = 0;
	int r = 0;
	int g = 0;
	int b = 0;
	int a = 0;
	int font = 0;

	if (id >= MAX_ELEMENTS || !reader.ReadWord(flags) || !reader.ReadShort(x) || !reader.ReadShort(y) || !reader.ReadByte(align)
	    || !reader.ReadByte(place) || !reader.ReadByte(parent) || !reader.ReadByte(gap) || !reader.ReadByte(r) || !reader.ReadByte(g)
	    || !reader.ReadByte(b) || !reader.ReadByte(a) || !reader.ReadByte(font))
		return;

	Element element;
	element.bValid = true;
	element.kind = static_cast<E_ElementKind>(kind);
	element.flags = static_cast<E_ElementFlags>(flags & static_cast<int>(E_ElementFlags::All));
	element.x = x;
	element.y = y;
	element.hAlign = GetHAlign(align);
	element.vAlign = GetVAlign(align);
	element.place = place <= static_cast<int>(E_Place::ParentRight) ? static_cast<E_Place>(place) : E_Place::Screen;
	element.iParent = IsParentPlace(element.place) && parent < MAX_ELEMENTS ? parent : -1;
	element.iGap = gap;
	element.r = r;
	element.g = g;
	element.b = b;
	element.a = a;
	element.font = font < FONT_COUNT ? static_cast<E_Font>(font) : E_Font::Text;

	// Refs are parsed in full, so a ref longer than its buffer stays invalid
	char str[MAX_MSG_SIZE + 1];
	reader.ReadString(str, sizeof(str), false);
	ParseRef(str, element.selectRef);
	V_strcpy_safe(element.szSelect, str);
	Q_UnicodeRepair(element.szSelect, STRINGCONVERT_SKIP);

	reader.ReadString(str, sizeof(str), false);
	ParseRef(str, element.valueRef);
	V_strcpy_safe(element.szValue, str);
	Q_UnicodeRepair(element.szValue, STRINGCONVERT_SKIP);

	// A list key that no list can have counts as no list
	reader.ReadString(str, sizeof(str), false);

	if (strlen(str) <= static_cast<size_t>(MAX_KEY_LEN))
		V_strcpy_safe(element.szList, str);

	// Every param is read, so that the color block after them is found
	int count = 0;
	reader.ReadByte(count);

	for (int i = 0; i < count && reader.GetRemaining() >= 3; i++)
	{
		int param = 0;
		int value = 0;
		reader.ReadByte(param);
		reader.ReadShort(value);

		const ParamInfo *info = FindParamInfo(element.kind, param);

		if (!info)
			continue;

		element.params[param] = clamp(value, info->iMin, info->iMax);
		element.iParamMask |= ParamBit(param);
	}

	int colors = 0;
	reader.ReadByte(colors);

	for (int i = 0; i < colors && reader.GetRemaining() >= 5; i++)
	{
		int slot = 0;
		int rgba[4] = {};
		reader.ReadByte(slot);

		for (int &component : rgba)
			reader.ReadByte(component);

		if (slot >= MAX_COLOR_SLOTS)
			continue;

		for (int j = 0; j < 4; j++)
			element.colors[slot][j] = static_cast<unsigned char>(rgba[j]);

		element.iColorMask |= 1u << slot;
	}

	m_Elements[id] = element;
	m_iSerial++;
	m_iHudSerial++;
}

void CScoreboardData::ReadTemplate(CMsgReader &reader)
{
	int id = 0;
	int variant = 0;
	char text[MAX_MSG_SIZE + 1];

	if (!reader.ReadByte(id) || !reader.ReadByte(variant) || !reader.ReadString(text, sizeof(text), false))
		return;

	if (id >= MAX_ELEMENTS || variant >= MAX_VARIANTS || !m_Elements[id].bValid)
		return;

	CTemplate &tpl = m_Templates[id][variant];

	if (text[0])
		tpl.Parse(text, &s_TemplateLocalizer);
	else
		tpl.Clear();

	m_iSerial++;
	m_iHudSerial++;
}

void CScoreboardData::ReadHudState(CMsgReader &reader)
{
	int visible = 0;
	int hide = 0;

	if (!reader.ReadLong(visible))
		return;

	reader.ReadWord(hide);
	m_iVisibleMask = static_cast<unsigned int>(visible);
	m_iHiddenStdParts = static_cast<unsigned int>(hide) & static_cast<unsigned int>(E_HudStd::All);
	m_iSerial++;
	m_iHudSerial++;
}

void CScoreboardData::ReadEvent(CMsgReader &reader)
{
	int id = 0;
	int variant = 0;
	int life = 0;
	int count = 0;

	if (!reader.ReadByte(id) || !reader.ReadByte(variant) || !reader.ReadWord(life) || !reader.ReadByte(count))
		return;

	const CTemplate *tpl = GetTemplate(id, variant);

	if (!tpl || m_Elements[id].kind != E_ElementKind::Feed)
		return;

	// An arg of an unknown type keeps its position as an empty value
	Value args[MAX_EVENT_ARGS];
	int argCount = min(count, MAX_EVENT_ARGS);

	for (int i = 0; i < argCount; i++)
	{
		int type = 0;

		if (!ReadValue(reader, args[i], type))
			return;
	}

	TemplateExtras extras;
	extras.pArgs = args;
	extras.iArgCount = argCount;
	const Value *icon = FindIconArg(args, argCount);

	HudLine line;
	Subject subject;
	GetElementSubject(id, subject);

	if (!EvaluateTemplate(*tpl, subject, id, line.wszText, sizeof(line.wszText), nullptr, &extras) && (!icon || !icon->szSprite[0]))
		return;

	line.iElement = id;
	line.iSeq = ++m_iLastHudLineSeq;
	line.iVariant = variant;
	line.flTime = GetTime();
	line.flLife = life / 1000.0f;

	if (icon)
		line.icon = *icon;

	m_HudLines[m_iNextHudLine] = line;
	m_iNextHudLine = (m_iNextHudLine + 1) % MAX_HUD_LINES;
}
