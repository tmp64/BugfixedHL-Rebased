#include <algorithm>
#include <climits>
#include <cstring>
#include <iterator>
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

ConVar hud_scoreboard_custom("hud_scoreboard_custom", "1", FCVAR_ARCHIVE, "Show the custom scoreboard when the server sends one");
ConVar hud_server_elements("hud_server_elements", "1", FCVAR_BHL_ARCHIVE, "Draw HUD elements defined by the server");

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

	int GetRemaining() const { return m_iSize - m_iPos; }

	//! Reads a little-endian integer of 1, 2 or 4 bytes, sign-extended if bSigned
	bool ReadInt(int &out, int size, bool bSigned)
	{
		if (GetRemaining() < size)
			return false;

		unsigned int value = 0;

		for (int i = size - 1; i >= 0; i--)
			value = (value << 8) | m_pBuf[m_iPos + i];

		if (bSigned && size < 4 && (value >> (8 * size - 1)))
			value |= ~0u << (8 * size);

		out = static_cast<int>(value);
		m_iPos += size;
		return true;
	}

	bool ReadByte(int &out) { return ReadInt(out, 1, false); }
	bool ReadWord(int &out) { return ReadInt(out, 2, false); }
	bool ReadShort(int &out) { return ReadInt(out, 2, true); }
	bool ReadLong(int &out) { return ReadInt(out, 4, true); }

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

const CScoreboardData::Value s_EmptyValue = CScoreboardData::Value();
const CScoreboardData::Column s_EmptyColumn = CScoreboardData::Column();
const CScoreboardData::Element s_EmptyElement = CScoreboardData::Element();

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

//! Frags and deaths as the scoreboard shows them: ScoreInfo of a player (0 for an empty slot), TeamScore of a team if it overrides them, else the sums of its players
void GetEntityScore(const CScoreboardData::Subject &entity, int &frags, int &deaths)
{
	CPlayerInfo *pi = entity.IsTeam() ? nullptr : GetPlayerInfoSafe(entity.iPlayer);
	frags = pi ? pi->GetFrags() : 0;
	deaths = pi ? pi->GetDeaths() : 0;

	if (!IsTeamNumber(entity.iTeam))
		return;

	CTeamInfo *ti = GetTeamInfo(entity.iTeam);

	if (ti->IsScoreOverriden())
	{
		frags = ti->GetFrags();
		deaths = ti->GetDeaths();
		return;
	}

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *player = GetPlayerInfoSafe(i);

		if (player && player->GetTeamNumber() == entity.iTeam)
		{
			frags += player->GetFrags();
			deaths += player->GetDeaths();
		}
	}
}

//! Participants, or teams with participants, by number; out needs MAX_PLAYERS entries
int CollectEntities(bool bTeams, CScoreboardData::Subject *out)
{
	bool bTeamHasParticipants[MAX_TEAMS + 1] = {};
	int count = 0;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		int team = GetPlayerTeamNumber(i);

		if (!CScoreboardData::IsParticipant(i))
			continue;

		if (!bTeams)
			out[count++] = { i, 0 };
		else if (IsTeamNumber(team))
			bTeamHasParticipants[team] = true;
	}

	for (int i = 1; i <= MAX_TEAMS && bTeams; i++)
	{
		if (bTeamHasParticipants[i])
			out[count++] = { 0, i };
	}

	return count;
}

int ClampToInt(long long value)
{
	return static_cast<int>(clamp<long long>(value, INT_MIN, INT_MAX));
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

//! Display name of an Icon value (10.3): BHL_Weapon_<sprite without "d_"> for standard HL sprites, else the localized text
void GetIconName(const CScoreboardData::Value &value, wchar_t *buf, int bufSizeBytes)
{
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

	CScoreboardData::LocalizeString(value.szText, buf, bufSizeBytes);
}

//! A stored value as a template shows it (8.10, "Values are written as")
bool ValueToTemplate(const CScoreboardData::Value &value, TemplateValue &out)
{
	value.ToString(out.wszText, sizeof(out.wszText));
	out.bIsNumber = value.GetNumber(out.iNumber);
	out.iMs = value.GetClockMs();
	out.bClockDown = value.type == E_ValueType::Clock && IsEnumFlagSet(value.clockFlags, E_ClockFlags::Down);
	return !value.IsEmpty();
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

		CScoreboardData::Value formatted = value ? *value : CScoreboardData::Value();
		out = TemplateValue();
		return CScoreboardData::FormatValue(ref.format, formatted) && ValueToTemplate(formatted, out);
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

CScoreboardData s_ScoreboardData;

}

//-----------------------------------------------------
// Value, Column, Element
//-----------------------------------------------------
int CScoreboardData::Value::GetClockMs() const
{
	if (!IsRunningClock())
		return iValue;

	double elapsed = std::max((static_cast<double>(GetTime()) - flClockTime) * 1000.0, 0.0);

	if (IsEnumFlagSet(clockFlags, E_ClockFlags::Down))
		return static_cast<int>(std::max(0.0, iValue - elapsed));

	return static_cast<int>(std::min(static_cast<double>(INT_MAX), iValue + elapsed));
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

	if (id < 0 || id > MAX_PARAM_ID || !(iParamMask & (1ULL << id)))
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
			std::fill_n(m_PlayerValues[i], MAX_COLUMNS, Value());
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

const CScoreboardData::Column &CScoreboardData::GetColumn(int id) const
{
	if (id < 0 || id >= MAX_COLUMNS)
		return s_EmptyColumn;

	return m_Columns[id];
}

int CScoreboardData::FindColumn(const char *key) const
{
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
		return std::any_of(values, values + count, [](const Value &value) { return value.IsRunningClock(); });
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

int CScoreboardData::CompareEntities(const Subject &a, const Subject &b) const
{
	if (a == b)
		return 0;

	for (int i = 0; i < m_iSortColumnCount; i++)
	{
		int col = m_SortColumns[i];
		int result = CompareValues(GetEntityValue(a, col), GetEntityValue(b, col), m_Columns[col].IsSortAscending());

		if (result != 0)
			return result;
	}

	int fragsA, deathsA, fragsB, deathsB;
	GetEntityScore(a, fragsA, deathsA);
	GetEntityScore(b, fragsB, deathsB);

	if (fragsA != fragsB)
		return fragsA > fragsB ? -1 : 1;

	if (deathsA != deathsB)
		return deathsA < deathsB ? -1 : 1;

	// One of the two numbers is always 0
	return a.iPlayer + a.iTeam < b.iPlayer + b.iTeam ? -1 : 1;
}

int CScoreboardData::GetPlace(const Subject &entity) const
{
	Subject entities[MAX_PLAYERS];
	int count = CollectEntities(entity.IsTeam(), entities);
	int col = GetFirstSortColumn();

	if (std::find(entities, entities + count, entity) == entities + count || (col != -1 && GetEntityValue(entity, col).IsEmpty()))
		return 0;

	int frags, deaths;
	GetEntityScore(entity, frags, deaths);
	int place = 1;

	for (int i = 0; i < count; i++)
	{
		int otherFrags, otherDeaths;
		GetEntityScore(entities[i], otherFrags, otherDeaths);

		if (col == -1 ? otherFrags > frags : CompareValues(GetEntityValue(entities[i], col), GetEntityValue(entity, col), m_Columns[col].IsSortAscending()) < 0)
			place++;
	}

	return place;
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
	if (subject.IsTeam() || !subject.iPlayer || !IsTeamMode())
		return subject;

	// A player without a team has no ranked entity in team mode
	int team = GetPlayerTeamNumber(subject.iPlayer);
	return { 0, IsTeamNumber(team) ? team : 0 };
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
	r.iCount = CollectEntities(true, r.entities);

	// 6.4: with sort keys the team cells decide, without them the players' teams
	for (int i = 0; i < r.iCount && !r.bTeamMode; i++)
		r.bTeamMode = sortCol == -1 || !GetTeamValue(r.entities[i].iTeam, sortCol).IsEmpty();

	if (!r.bTeamMode)
		r.iCount = CollectEntities(false, r.entities);

	if (sortCol != -1)
		r.iSign = m_Columns[sortCol].IsSortAscending() ? -1 : 1;

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
	int frags = 0;
	int deaths = 0;
	b.Set(E_Builtin::Total, r.iCount);

	if (subject.IsTeam() ? !IsTeamNumber(subject.iTeam) : !GetPlayerInfoSafe(subject.iPlayer))
		return b;

	GetEntityScore(subject, frags, deaths);
	b.Set(E_Builtin::Frags, frags);
	b.Set(E_Builtin::Deaths, deaths);

	int own = static_cast<int>(std::find(r.entities, r.entities + r.iCount, GetRankedEntity(subject)) - r.entities);

	if (own == r.iCount || !r.bHasNumber[own])
		return b;

	b.Set(E_Builtin::Score, r.numbers[own]);

	long long ownNumber = static_cast<long long>(r.numbers[own]) * r.iSign;
	long long best = ownNumber;
	long long gap = 0;
	long long lead = -1;
	int rival = -1;
	int place = 1;
	bool bTied = false;

	for (int i = 0; i < r.iCount; i++)
	{
		if (i == own || !r.bHasNumber[i])
			continue;

		long long number = static_cast<long long>(r.numbers[i]) * r.iSign;

		if (number > best)
			best = number;

		if (number > ownNumber)
		{
			place++;

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

	b.Set(E_Builtin::Place, place);
	b.Set(E_Builtin::Behind, ClampToInt(best - ownNumber));

	// 0 no rivals, 1 a rival ahead, 2 tied for the first place, 3 leading
	b.Set(E_Builtin::State, rival != -1 ? 1 : (bTied ? 2 : (lead != -1 ? 3 : 0)));

	if (rival != -1)
	{
		b.rival = r.entities[rival];
		b.Set(E_Builtin::Gap, ClampToInt(gap));
		b.Set(E_Builtin::Pass, ClampToInt(gap + 1));
	}
	else if (bTied || lead != -1)
	{
		b.Set(E_Builtin::Lead, bTied ? 0 : ClampToInt(lead));
	}

	return b;
}

bool CScoreboardData::GetEntityNumber(const Subject &entity, int sortCol, int &out) const
{
	if (sortCol != -1)
		return GetEntityValue(entity, sortCol).GetNumber(out);

	int deaths;
	GetEntityScore(entity, out, deaths);
	return true;
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

CScoreboardData::Subject CScoreboardData::GetViewerSubject() const
{
	CPlayerInfo *pi = GetThisPlayerInfo();
	int client = 0;

	// OBS_ROAMING and OBS_MAP_FREE look at nobody
	if (g_iUser1 == OBS_NONE)
		client = pi ? pi->GetIndex() : 0;
	else if (g_iUser1 == OBS_CHASE_LOCKED || g_iUser1 == OBS_CHASE_FREE || g_iUser1 == OBS_IN_EYE || g_iUser1 == OBS_MAP_CHASE)
		client = g_iUser2;

	return { IsParticipant(client) ? client : 0, 0 };
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
	const Value &value = GetPlayerValue(subject.iPlayer, col);

	if (!value.IsEmpty())
		return value;

	// The player's team cells, in team mode or not (8.10); a team subject starts here
	const Value &teamValue = GetTeamValue(subject.iPlayer ? GetPlayerTeamNumber(subject.iPlayer) : subject.iTeam, col);
	return teamValue.IsEmpty() ? GetGlobalValue(col) : teamValue;
}

const TemplateRef *CScoreboardData::GetValueRef(int elementId) const
{
	const Element &element = GetElement(elementId);

	if (!element.bValid || !element.valueRef.IsSet() || element.valueRef.builtin == E_Builtin::Value)
		return nullptr;

	return &element.valueRef;
}

bool CScoreboardData::GetRefValue(const TemplateRef &ref, const Subject &subject, int elementId, Value &out) const
{
	const TemplateRef *valueRef = ref.builtin == E_Builtin::Value ? GetValueRef(elementId) : nullptr;
	Subject target = ref.bFirstRow ? GetFirstRow() : subject;
	out = Value();

	if (!ref.IsSet() || ref.builtin == E_Builtin::Unknown || (ref.bFirstRow && !target.IsValid()))
		return false;

	// One level only. With -1 a nested "@value" can't resolve, so this never recurses further.
	if (valueRef)
	{
		GetRefValue(*valueRef, target, -1, out);
	}
	else if (ref.builtin == E_Builtin::None)
	{
		out = GetSubjectValue(target, FindColumn(ref.szKey));
	}
	else
	{
		Builtins b = GetBuiltins(target);
		out.type = b.Has(ref.builtin) ? E_ValueType::Int : E_ValueType::None;
		out.iValue = b.Get(ref.builtin);
	}

	return !out.IsEmpty() && FormatValue(ref.format, out);
}

bool CScoreboardData::ResolveRef(const TemplateRef &ref, const Subject &subject, int elementId, TemplateValue &out) const
{
	const TemplateRef *valueRef = ref.builtin == E_Builtin::Value ? GetValueRef(elementId) : nullptr;
	const TemplateRef &inner = valueRef ? *valueRef : ref;
	Value value;
	out = TemplateValue();

	if (inner.builtin != E_Builtin::Name && inner.builtin != E_Builtin::Rival)
		return GetRefValue(ref, subject, elementId, value) && ValueToTemplate(value, out);

	// Names are text, never a stored value or a number (8.10); the rival is nobody unless @state is 1
	Subject target = (ref.bFirstRow || inner.bFirstRow) ? GetFirstRow() : subject;
	Subject named = inner.builtin == E_Builtin::Rival ? GetBuiltins(target).rival : target;
	return ref.format == E_RefFormat::None && inner.format == E_RefFormat::None
	    && (named.IsTeam() ? WriteTeamName(named.iTeam, out.wszText, sizeof(out.wszText)) : WritePlayerName(named.iPlayer, out.wszText, sizeof(out.wszText)));
}

bool CScoreboardData::GetElementValue(int id, const Subject &subject, Value &out) const
{
	return GetRefValue(GetElement(id).valueRef, subject, id, out);
}

bool CScoreboardData::FormatValue(E_RefFormat format, Value &value)
{
	if (format == E_RefFormat::None)
		return true;

	bool bIsProgress = value.type == E_ValueType::Progress;
	int number = format == E_RefFormat::Cur ? value.iValue : (format == E_RefFormat::Max ? value.iMax : (value.iMax > value.iValue ? value.iMax - value.iValue : 0));
	value = Value();

	if (!bIsProgress)
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
		Value value;

		if (!GetRefValue(element.selectRef, subject, id, value) || !value.GetNumber(variant))
			return -1;
	}

	return GetTemplate(id, variant) ? variant : -1;
}

bool CScoreboardData::GetElementText(int id, const Subject &subject, wchar_t *buf, int bufSizeBytes) const
{
	buf[0] = L'\0';
	int variant = GetElementVariant(id, subject);
	const CTemplate *tpl = GetTemplate(id, variant);

	// A Meter without variant 0 shows the default counter (8.7)
	if (!tpl && variant >= 0 && m_Elements[id].kind == E_ElementKind::Meter)
		tpl = &GetMeterCounterTemplate();

	return tpl && EvaluateTemplate(*tpl, subject, id, buf, bufSizeBytes);
}

const CTemplate *CScoreboardData::GetTemplate(int id, int variant) const
{
	if (id < 0 || id >= MAX_ELEMENTS || variant < 0 || variant >= MAX_VARIANTS || !m_Templates[id][variant].IsSet())
		return nullptr;

	return &m_Templates[id][variant];
}

bool CScoreboardData::EvaluateTemplate(const CTemplate &tpl, const Subject &subject, int elementId, wchar_t *buf, int bufSizeBytes, const TemplateExtras *pExtras) const
{
	CStoreTemplateContext ctx(*this, subject, elementId, pExtras);
	return tpl.Evaluate(ctx, buf, bufSizeBytes / static_cast<int>(sizeof(wchar_t)));
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
	buf[0] = L'\0';

	for (int i = 0; i < MAX_ELEMENTS; i++)
	{
		const Element &element = m_Elements[i];
		Subject subject;

		if (element.kind != E_ElementKind::Text || element.place != place || !IsElementVisible(i) || !GetElementSubject(i, subject))
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

//-----------------------------------------------------
// Messages
//-----------------------------------------------------
void CScoreboardData::Clear()
{
	m_bConfigReceived = false;
	m_ConfigFlags = E_ConfigFlags::None;
	m_szTitle[0] = '\0';
	m_szTag[0] = '\0';

	std::fill(std::begin(m_Columns), std::end(m_Columns), Column());
	std::fill(std::begin(m_GlobalValues), std::end(m_GlobalValues), Value());

	for (Value *values : m_PlayerValues)
		std::fill_n(values, MAX_COLUMNS, Value());

	for (Value *values : m_TeamValues)
		std::fill_n(values, MAX_COLUMNS, Value());

	std::fill(std::begin(m_Lists), std::end(m_Lists), List());

	for (int i = 0; i < MAX_ELEMENTS; i++)
		RemoveElement(i);

	m_iVisibleMask = 0;
	m_iHiddenStdParts = 0;
	m_iSortColumnCount = 0;
	m_iSerial++;
	m_iSchemaSerial++;
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

	// Indexed by E_Op; other ops are reserved and ignored
	using FnRead = void (CScoreboardData::*)(CMsgReader &reader);
	static constexpr FnRead s_Readers[] = {
		&CScoreboardData::ReadReset, &CScoreboardData::ReadConfig, &CScoreboardData::ReadColumn,
		&CScoreboardData::ReadValues, &CScoreboardData::ReadList, &CScoreboardData::ReadElement,
		&CScoreboardData::ReadTemplate, &CScoreboardData::ReadHudState, &CScoreboardData::ReadEvent
	};
	static_assert(std::size(s_Readers) == static_cast<size_t>(E_Op::Event) + 1, "every op needs a reader");

	if (op < static_cast<int>(std::size(s_Readers)))
		(this->*s_Readers[op])(reader);

	return 1;
}

void CScoreboardData::ReadReset(CMsgReader &)
{
	Clear();
	m_bWaitForReset = false;
}

void CScoreboardData::ReadConfig(CMsgReader &reader)
{
	int flags = 0;

	if (!reader.ReadWord(flags))
		return;

	// A missing title or tag is ""
	reader.ReadString(m_szTitle, sizeof(m_szTitle));
	reader.ReadString(m_szTag, sizeof(m_szTag));
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
	int host = NO_ID;
	Column column;

	if (!reader.ReadByte(id) || !reader.ReadByte(display) || !reader.ReadWord(flags) || !reader.ReadByte(column.iWidth) || !reader.ReadByte(column.iSort)
	    || !reader.ReadString(column.szKey, sizeof(column.szKey)) || id >= MAX_COLUMNS)
		return;

	// A missing title is "", a missing host is NO_ID
	reader.ReadString(column.szTitle, sizeof(column.szTitle));
	reader.ReadByte(host);

	column.bValid = true;
	column.display = display <= static_cast<int>(E_Display::Badge) ? static_cast<E_Display>(display) : E_Display::Text;
	column.flags = static_cast<E_ColumnFlags>(flags & static_cast<int>(E_ColumnFlags::All));
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
		bValid = (len == 1 || len == 2 || len == 4) && payload.ReadInt(value.iValue, len, true);
		break;
	case E_ValueType::Text:
		bValid = len <= MAX_TEXT_LEN + 1 && payload.ReadString(value.szText, sizeof(value.szText));
		break;
	case E_ValueType::Icon:
		bValid = len <= 2 * (MAX_TEXT_LEN + 1) && payload.ReadString(value.szSprite, sizeof(value.szSprite)) && payload.ReadString(value.szText, sizeof(value.szText));
		break;
	case E_ValueType::Progress:
		bValid = (len == 2 || len == 4) && payload.ReadInt(value.iValue, len / 2, false) && payload.ReadInt(value.iMax, len / 2, false);
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

	if (!reader.ReadByte(target))
		return;

	Value *values = GetTargetValues(target);
	int col = 0;
	int type = 0;
	Value value;

	// A message that ends inside a cell keeps the cells already applied
	while (values && reader.ReadByte(col) && ReadValue(reader, value, type))
	{
		if (col == ALL_COLUMNS && type == static_cast<int>(E_ValueType::None))
		{
			std::fill_n(values, MAX_COLUMNS, Value());
			m_iSerial++;
		}
		else if (col < MAX_COLUMNS)
		{
			values[col] = value;
			m_iSerial++;
		}
	}
}

void CScoreboardData::ReadList(CMsgReader &reader)
{
	int id = 0;
	int total = 0;
	int start = 0;
	char key[MAX_KEY_LEN + 1];

	if (!reader.ReadByte(id) || !reader.ReadByte(total) || !reader.ReadByte(start) || !reader.ReadString(key, sizeof(key)) || id >= MAX_LISTS)
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

	// None or an unknown kind removes
	if (!IsKnownKind(kind))
	{
		if (id < MAX_ELEMENTS)
		{
			RemoveElement(id);
			m_iSerial++;
		}

		return;
	}

	Element element;
	int flags = 0;
	int align = 0;
	int place = 0;
	int parent = 0;
	int font = 0;

	if (id >= MAX_ELEMENTS || !reader.ReadWord(flags) || !reader.ReadShort(element.x) || !reader.ReadShort(element.y) || !reader.ReadByte(align)
	    || !reader.ReadByte(place) || !reader.ReadByte(parent) || !reader.ReadByte(element.iGap) || !reader.ReadByte(element.r)
	    || !reader.ReadByte(element.g) || !reader.ReadByte(element.b) || !reader.ReadByte(element.a) || !reader.ReadByte(font))
		return;

	element.bValid = true;
	element.kind = static_cast<E_ElementKind>(kind);
	element.flags = static_cast<E_ElementFlags>(flags & static_cast<int>(E_ElementFlags::All));
	element.hAlign = align & 3;
	element.vAlign = (align >> 2) & 3;
	element.place = place <= static_cast<int>(E_Place::ParentRight) ? static_cast<E_Place>(place) : E_Place::Screen;
	element.iParent = IsParentPlace(element.place) && parent < MAX_ELEMENTS ? parent : -1;
	element.font = font < FONT_COUNT ? static_cast<E_Font>(font) : E_Font::Text;

	// Refs are parsed in full, so a ref longer than MAX_REF_LEN stays invalid
	char str[MAX_MSG_SIZE + 1];
	reader.ReadString(str, sizeof(str), false);
	ParseRef(str, element.selectRef);

	reader.ReadString(str, sizeof(str), false);
	ParseRef(str, element.valueRef);

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
		element.iParamMask |= 1ULL << param;
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
}

void CScoreboardData::ReadTemplate(CMsgReader &reader)
{
	int id = 0;
	int variant = 0;
	char text[MAX_MSG_SIZE + 1];

	if (!reader.ReadByte(id) || !reader.ReadByte(variant) || !reader.ReadString(text, sizeof(text), false) || id >= MAX_ELEMENTS
	    || variant >= MAX_VARIANTS || !m_Elements[id].bValid)
		return;

	CTemplate &tpl = m_Templates[id][variant];
	const wchar_t *localized = IsClientToken(text) ? g_pVGuiLocalize->Find(text) : nullptr;

	// The text of a client token is parsed, never looked up again (8.10)
	if (localized)
		tpl.Parse(localized);
	else if (text[0])
		tpl.Parse(text);
	else
		tpl.Clear();

	m_iSerial++;
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

	if (!EvaluateTemplate(*tpl, subject, id, line.wszText, sizeof(line.wszText), &extras) && (!icon || !icon->szSprite[0]))
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
