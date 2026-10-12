#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/ILocalize.h>
#include "hud.h"
#include "cl_util.h"
#include "scoreboard_data.h"

using namespace bhl::sb;

static_assert(TARGET_PLAYER_LAST - TARGET_PLAYER_FIRST + 1 <= MAX_PLAYERS, "player targets must fit m_PlayerValues");
static_assert(TARGET_TEAM_LAST - TARGET_TEAM_BASE <= MAX_TEAMS, "team targets must fit m_TeamValues");

ConVar hud_scoreboard_custom("hud_scoreboard_custom", "1", FCVAR_ARCHIVE, "Show the custom scoreboard when the server provides one");
ConVar hud_gungame("hud_gungame", "1", FCVAR_BHL_ARCHIVE, "Show the GunGame HUD (requires hud_scoreboard_custom 1)");

CON_COMMAND(hud_scoreboard_dump, "Prints the custom scoreboard data to the console")
{
	CScoreboardData::Get().DumpCommand();
}

CON_COMMAND(hud_scoreboard_test, "Fills the custom scoreboard with test data: hud_scoreboard_test [preset|off] [phase] [phase_left]")
{
	CScoreboardData::Get().TestCommand();
}

namespace
{

//! Reads the payload of a BhlScore message and tracks the remaining size
class CMsgReader
{
public:
	CMsgReader(const void *pBuf, int iSize)
	    : m_pBuf(static_cast<const unsigned char *>(pBuf))
	    , m_iSize(iSize)
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

	//! Reads a NUL-terminated string, truncates it to the buffer and strips color codes.
	//! Returns false if the payload ends before the terminator.
	bool ReadString(char *buf, int bufSize)
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

		buf[len] = '\0';

		if (bTruncated)
			Q_UnicodeRepair(buf, STRINGCONVERT_SKIP);

		RemoveColorCodesInPlace(buf);
		return bTerminated;
	}

	template <size_t N>
	bool ReadString(char (&buf)[N])
	{
		return ReadString(buf, N);
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

const CScoreboardData::Value s_EmptyValue = CScoreboardData::Value();
const CScoreboardData::Column s_EmptyColumn = CScoreboardData::Column();

/**
 * Reads "type len payload".
 * A value of an unknown type, with a wrong len or a broken payload is returned empty.
 * @param	type	Receives the type byte
 * @return	false if the message ends inside the value
 */
bool ReadValue(CMsgReader &reader, CScoreboardData::Value &value, int &type)
{
	int len = 0;
	value = CScoreboardData::Value();

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
	default:
		break;
	}

	if (bValid)
		value.type = static_cast<E_ValueType>(type);
	else
		value = CScoreboardData::Value();

	return true;
}

void ClearValues(CScoreboardData::Value *values)
{
	for (int i = 0; i < MAX_COLUMNS; i++)
		values[i] = CScoreboardData::Value();
}

//! Returns < 0 if a goes before b. Empty values go after filled ones in both directions.
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

bool IsParticipant(int client)
{
	CPlayerInfo *pi = GetPlayerInfo(client);
	return pi->IsConnected() && !pi->IsSpectator();
}

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
	default:
		snprintf(buf, bufSize, "-");
		break;
	}
}

CScoreboardData::Value MakeInt(int n)
{
	CScoreboardData::Value value;
	value.type = E_ValueType::Int;
	value.iValue = n;
	return value;
}

CScoreboardData::Value MakeProgress(int cur, int max)
{
	CScoreboardData::Value value;
	value.type = E_ValueType::Progress;
	value.iValue = cur;
	value.iMax = max;
	return value;
}

CScoreboardData::Value MakeIcon(const char *sprite, const char *text)
{
	CScoreboardData::Value value;
	value.type = E_ValueType::Icon;
	V_strcpy_safe(value.szSprite, sprite);
	V_strcpy_safe(value.szText, text);
	return value;
}

CScoreboardData s_ScoreboardData;

}

//-----------------------------------------------------
// Value, Column
//-----------------------------------------------------
bool CScoreboardData::Value::IsEmpty() const
{
	return type == E_ValueType::None;
}

bool CScoreboardData::Value::GetNumber(int &out) const
{
	if (type != E_ValueType::Int && type != E_ValueType::Progress)
		return false;

	out = iValue;
	return true;
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
	std::fill(std::begin(m_bPlayerConnected), std::end(m_bPlayerConnected), false);
}

void CScoreboardData::Frame()
{
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

bool CScoreboardData::IsGunGameHudEnabled()
{
	return IsCustomEnabled() && hud_gungame.GetBool();
}

bool CScoreboardData::IsActive() const
{
	return m_bConfigReceived && IsCustomEnabled();
}

bool CScoreboardData::IsGunGame() const
{
	return IsActive() && !Q_stricmp(m_szPreset, "gungame");
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
		if (m_Columns[i].bValid && m_Columns[i].IsPrimary())
			return i;
	}

	return -1;
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
	if (team < 1 || team > MAX_TEAMS || col < 0 || col >= MAX_COLUMNS)
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

int CScoreboardData::ComparePlayers(int a, int b) const
{
	for (int i = 0; i < m_iSortColumnCount; i++)
	{
		int col = m_SortColumns[i];
		int result = CompareValues(GetPlayerValue(a, col), GetPlayerValue(b, col), m_Columns[col].IsSortAscending());

		if (result != 0)
			return result;
	}

	return 0;
}

int CScoreboardData::CompareTeams(int a, int b) const
{
	for (int i = 0; i < m_iSortColumnCount; i++)
	{
		int col = m_SortColumns[i];
		int result = CompareValues(GetTeamValue(a, col), GetTeamValue(b, col), m_Columns[col].IsSortAscending());

		if (result != 0)
			return result;
	}

	return 0;
}

int CScoreboardData::GetPlayerPlace(int client, int *pTotal) const
{
	int col = GetFirstSortColumn();
	const Value &value = GetPlayerValue(client, col);
	bool bAscending = GetColumn(col).IsSortAscending();
	int total = 0;
	int ahead = 0;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		if (!IsParticipant(i))
			continue;

		total++;

		if (CompareValues(GetPlayerValue(i, col), value, bAscending) < 0)
			ahead++;
	}

	if (pTotal)
		*pTotal = total;

	if (value.IsEmpty() || !IsParticipant(client))
		return 0;

	return ahead + 1;
}

int CScoreboardData::GetTeamPlace(int team, int *pTotal) const
{
	bool bHasParticipants[MAX_TEAMS + 1] = {};

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		if (!IsParticipant(i))
			continue;

		int playerTeam = GetPlayerInfo(i)->GetTeamNumber();

		if (playerTeam >= 1 && playerTeam <= MAX_TEAMS)
			bHasParticipants[playerTeam] = true;
	}

	int col = GetFirstSortColumn();
	const Value &value = GetTeamValue(team, col);
	bool bAscending = GetColumn(col).IsSortAscending();
	int total = 0;
	int ahead = 0;

	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		if (!bHasParticipants[i])
			continue;

		total++;

		if (CompareValues(GetTeamValue(i, col), value, bAscending) < 0)
			ahead++;
	}

	if (pTotal)
		*pTotal = total;

	if (value.IsEmpty() || !bHasParticipants[team])
		return 0;

	return ahead + 1;
}

const char *CScoreboardData::GetPluralSuffix(int n)
{
	const wchar_t *rule = g_pVGuiLocalize->Find("BHL_PluralRule");
	n = abs(n);

	if (rule && !wcscmp(rule, L"ru"))
	{
		if (n % 10 == 1 && n % 100 != 11)
			return "One";

		if (n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14))
			return "Few";

		return "Many";
	}

	return n == 1 ? "One" : "Many";
}

void CScoreboardData::LocalizeString(const char *str, wchar_t *buf, int bufSizeBytes)
{
	if (str[0] == '#' && str[1])
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

void CScoreboardData::DumpCommand()
{
	ConPrintf("Scoreboard: active %d, config %d, hud_scoreboard_custom %d, hud_gungame %d, serial %d\n",
	    IsActive(), m_bConfigReceived, IsCustomEnabled(), IsGunGameHudEnabled(), m_iSerial);
	ConPrintf("Config: flags 0x%x, preset \"%s\", title \"%s\", tag \"%s\"\n",
	    static_cast<unsigned int>(m_ConfigFlags), m_szPreset, m_szTitle, m_szTag);

	ConPrintf("Columns:\n");

	for (int i = 0; i < MAX_COLUMNS; i++)
	{
		const Column &column = m_Columns[i];

		if (column.bValid)
		{
			ConPrintf("  %2d: key \"%s\", title \"%s\", display %d, flags 0x%x, width %d, sort 0x%02x\n",
			    i, column.szKey, column.szTitle, static_cast<int>(column.display), static_cast<unsigned int>(column.flags), column.iWidth, column.iSort);
		}
	}

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
}

void CScoreboardData::TestCommand()
{
	const char *preset = gEngfuncs.Cmd_Argc() >= 2 ? gEngfuncs.Cmd_Argv(1) : "gungame";

	Clear();

	if (!strcmp(preset, "off"))
	{
		ConPrintf("Scoreboard test data removed.\n");
		return;
	}

	enum
	{
		COL_RANK = 0,
		COL_LEVEL,
		COL_WEAPON,
		COL_PROGRESS,
		COL_KILLS,
		COL_DEATHS,
		COL_SCORE,
		COL_PHASE,
		COL_PHASE_LEFT,
	};

	struct TestColumn
	{
		const char *pszKey;
		const char *pszTitle;
		E_Display display;
		int iWidth;
		int iSort;
		E_ColumnFlags flags;
	};

	static const TestColumn s_Columns[] = {
		{ "rank", "", E_Display::Builtin, 0, 0, E_ColumnFlags::None },
		{ "level", "#BHL_SB_Level", E_Display::Steps, 60, 0, E_ColumnFlags::Primary },
		{ "weapon", "#BHL_SB_Weapon", E_Display::Icon, 120, 0, E_ColumnFlags::None },
		{ "progress", "#BHL_SB_Progress", E_Display::Meter, 90, 0, E_ColumnFlags::None },
		{ "kills", "#BHL_SB_Kills", E_Display::Text, 60, 2, E_ColumnFlags::AlignRight },
		{ "deaths", "", E_Display::Builtin, 0, 0, E_ColumnFlags::AlignRight },
		{ "score", "", E_Display::Text, 0, 1, E_ColumnFlags::Hidden },
		{ "phase", "", E_Display::Text, 0, 0, E_ColumnFlags::Hidden },
		{ "phase_left", "", E_Display::Text, 0, 0, E_ColumnFlags::Hidden },
	};

	struct TestLevel
	{
		const char *pszSprite;
		const char *pszName;
		int iKills;
	};

	static const TestLevel s_Levels[] = {
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

	constexpr int LEVEL_COUNT = static_cast<int>(std::size(s_Levels));

	m_bConfigReceived = true;
	m_ConfigFlags = static_cast<E_ConfigFlags>(static_cast<unsigned int>(E_ConfigFlags::HideFrags) | static_cast<unsigned int>(E_ConfigFlags::HideEff) | static_cast<unsigned int>(E_ConfigFlags::HideSteamID));
	V_strcpy_safe(m_szPreset, preset);
	V_strcpy_safe(m_szTitle, "GUNGAME");
	V_strcpy_safe(m_szTag, gHUD.m_Teamplay ? "TEAM" : "FFA");

	for (int i = 0; i < static_cast<int>(std::size(s_Columns)); i++)
	{
		Column &column = m_Columns[i];
		column.bValid = true;
		column.display = s_Columns[i].display;
		column.flags = s_Columns[i].flags;
		column.iWidth = s_Columns[i].iWidth;
		column.iSort = s_Columns[i].iSort;
		V_strcpy_safe(column.szKey, s_Columns[i].pszKey);
		V_strcpy_safe(column.szTitle, s_Columns[i].pszTitle);
	}

	UpdateSortColumns();

	List &levels = m_Lists[0];
	levels.bValid = true;
	levels.iCount = LEVEL_COUNT;
	V_strcpy_safe(levels.szKey, "levels");

	for (int i = 0; i < LEVEL_COUNT; i++)
		levels.items[i] = MakeIcon(s_Levels[i].pszSprite, s_Levels[i].pszName);

	m_GlobalValues[COL_PHASE] = MakeInt(gEngfuncs.Cmd_Argc() >= 3 ? atoi(gEngfuncs.Cmd_Argv(2)) : 0);
	m_GlobalValues[COL_PHASE_LEFT] = MakeInt(gEngfuncs.Cmd_Argc() >= 4 ? atoi(gEngfuncs.Cmd_Argv(3)) : 0);

	int playerCount = 0;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfoSafe(i);

		if (!pi)
			continue;

		// The first player is on the last level, the third and the fourth are tied
		int seed = playerCount == 3 ? 2 : playerCount;
		int level = LEVEL_COUNT - 1 - (seed * 3) % LEVEL_COUNT;
		int kills = seed % s_Levels[level].iKills;
		int score = kills;

		for (int j = 0; j < level; j++)
			score += s_Levels[j].iKills;

		Value *values = m_PlayerValues[i];
		values[COL_LEVEL] = MakeProgress(level + 1, LEVEL_COUNT);
		values[COL_WEAPON] = levels.items[level];
		values[COL_PROGRESS] = MakeProgress(kills, s_Levels[level].iKills);
		values[COL_KILLS] = MakeInt(score + seed % 3);
		values[COL_SCORE] = MakeInt(score);

		int team = pi->GetTeamNumber();

		if (team >= 1 && team <= MAX_TEAMS && CompareValues(values[COL_SCORE], m_TeamValues[team][COL_SCORE], false) < 0)
			std::copy(values, values + MAX_COLUMNS, m_TeamValues[team]);

		playerCount++;
	}

	m_iSerial++;
	ConPrintf("Scoreboard test data: %d players, preset \"%s\".\n", playerCount, m_szPreset);
}

void CScoreboardData::Clear()
{
	m_bConfigReceived = false;
	m_ConfigFlags = E_ConfigFlags::None;
	m_szPreset[0] = '\0';
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

	m_iSortColumnCount = 0;
	m_iSerial++;
	m_iSchemaSerial++;
}

void CScoreboardData::UpdateSortColumns()
{
	m_iSortColumnCount = 0;

	for (int i = 0; i < MAX_COLUMNS; i++)
	{
		const Column &column = m_Columns[i];

		if (column.bValid && column.display != E_Display::Builtin && column.GetSortPriority() > 0)
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
		return m_PlayerValues[target];

	if (target >= TARGET_TEAM_FIRST && target <= TARGET_TEAM_LAST)
		return m_TeamValues[target - TARGET_TEAM_BASE];

	return nullptr;
}

int CScoreboardData::MsgFunc_BhlScore(const char *pszName, int iSize, void *pbuf)
{
	CMsgReader reader(pbuf, iSize);
	int op = 0;

	if (!reader.ReadByte(op))
		return 1;

	switch (static_cast<E_Op>(op))
	{
	case E_Op::Reset:
	{
		int version = 0;

		if (reader.ReadByte(version) && version >= 1)
			Clear();

		break;
	}
	case E_Op::Config:
	{
		int flags = 0;
		char preset[sizeof(m_szPreset)];
		char title[sizeof(m_szTitle)];
		char tag[sizeof(m_szTag)];

		if (!reader.ReadWord(flags) || !reader.ReadString(preset) || !reader.ReadString(title) || !reader.ReadString(tag))
			break;

		m_bConfigReceived = true;
		m_ConfigFlags = static_cast<E_ConfigFlags>(flags);
		V_strcpy_safe(m_szPreset, preset);
		V_strcpy_safe(m_szTitle, title);
		V_strcpy_safe(m_szTag, tag);
		m_iSerial++;
		m_iSchemaSerial++;
		break;
	}
	case E_Op::Column:
	{
		int id = 0;
		int display = 0;
		int flags = 0;
		int width = 0;
		int sort = 0;
		Column column;

		if (!reader.ReadByte(id) || !reader.ReadByte(display) || !reader.ReadWord(flags) || !reader.ReadByte(width) || !reader.ReadByte(sort)
		    || !reader.ReadString(column.szKey) || !reader.ReadString(column.szTitle))
			break;

		if (id >= MAX_COLUMNS)
			break;

		column.bValid = true;
		column.display = display <= static_cast<int>(E_Display::Steps) ? static_cast<E_Display>(display) : E_Display::Text;
		column.flags = static_cast<E_ColumnFlags>(flags);
		column.iWidth = width;
		column.iSort = sort;
		m_Columns[id] = column;
		UpdateSortColumns();
		m_iSerial++;
		m_iSchemaSerial++;
		break;
	}
	case E_Op::Values:
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

				if (!reader.ReadByte(col) || !ReadValue(reader, value, type))
					return 1;

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

		break;
	}
	case E_Op::List:
	{
		int id = 0;
		int total = 0;
		int start = 0;
		char key[MAX_KEY_LEN + 1];

		if (!reader.ReadByte(id) || !reader.ReadByte(total) || !reader.ReadByte(start) || !reader.ReadString(key) || id >= MAX_LISTS)
			break;

		List &list = m_Lists[id];
		total = min(total, MAX_LIST_ITEMS);

		for (int i = total; i < list.iCount; i++)
			list.items[i] = Value();

		list.bValid = total > 0;
		list.iCount = total;
		V_strcpy_safe(list.szKey, total > 0 ? key : "");
		m_iSerial++;

		for (int i = start; reader.GetRemaining() > 0; i++)
		{
			int type = 0;
			Value value;

			if (!ReadValue(reader, value, type))
				break;

			if (i < total)
				list.items[i] = value;
		}

		break;
	}
	default:
		break;
	}

	return 1;
}
