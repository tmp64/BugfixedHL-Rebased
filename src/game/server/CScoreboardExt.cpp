#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"
#include "CBugfixedServer.h"
#include "CScoreboardExt.h"

extern int gmsgBhlScore;

//! Copies a string and truncates it on a UTF-8 character boundary
template <size_t N>
static void CopyString(char (&dst)[N], const char *src)
{
	if (!src)
		src = "";

	int len = (int)strlen(src);

	if (len >= (int)N)
	{
		len = (int)N - 1;

		while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80)
			len--;
	}

	memcpy(dst, src, len);
	dst[len] = '\0';
}

static bool IsValidKey(const char *key)
{
	if (!key)
		return false;

	int len = (int)strlen(key);
	return len >= 1 && len <= bhl::sb::MAX_KEY_LEN && (int)strspn(key, "abcdefghijklmnopqrstuvwxyz0123456789_") == len;
}

static int ClampWord(int value)
{
	return std::clamp(value, 0, 65535);
}

static bool IsByteProgress(int cur, int max)
{
	return cur >= 0 && cur <= 255 && max >= 0 && max <= 255;
}

//----------------------------------------------------------------------------------------------------
// CScoreboardExt
//----------------------------------------------------------------------------------------------------
void CScoreboardExt::ClearAll()
{
	ClearSchema();

	for (char *name : m_szTeamNames)
		name[0] = '\0';

	m_iTeamCount = 0;

	for (client_state_t &state : m_Clients)
		state = client_state_t();
}

void CScoreboardExt::ClearPlayer(int idx)
{
	bool hasCells = false;

	for (value_t &cell : m_Cells[idx])
	{
		if (cell.type != bhl::sb::E_ValueType::None)
		{
			hasCells = true;
			cell = value_t();
		}
	}

	for (client_state_t &state : m_Clients)
	{
		if (m_bActive && (hasCells || state.cellsDirty[idx]))
			state.playersCleared |= 1ULL << (idx - 1);

		state.cellsDirty[idx] = 0;
	}

	m_Clients[idx] = client_state_t();
}

void CScoreboardExt::StartFullSync(int idx, bool delay)
{
	client_state_t &state = m_Clients[idx];
	float nextSendTime = delay ? gpGlobals->time + FULL_SYNC_DELAY : state.nextSendTime;

	state = client_state_t();
	state.nextSendTime = nextSendTime;
	state.isResetPending = true;

	if (!m_bActive)
		return;

	state.isConfigPending = true;
	state.columnsPending = (1u << m_iColumnCount) - 1;

	for (int i = 0; i < m_iListCount; i++)
	{
		if (m_Lists[i].count == 0)
			continue;

		state.listsResized |= 1u << i;

		for (int j = 0; j < m_Lists[i].count; j++)
		{
			if (m_Lists[i].items[j].type != bhl::sb::E_ValueType::None)
				state.listItemsDirty[i] |= 1ULL << j;
		}
	}

	for (int target = 0; target < MAX_TARGETS; target++)
	{
		for (int column = 0; column < m_iColumnCount; column++)
		{
			if (m_Cells[target][column].type != bhl::sb::E_ValueType::None)
				state.cellsDirty[target] |= 1u << column;
		}
	}
}

void CScoreboardExt::StartFrame()
{
	if (!gmsgBhlScore)
		return;

	if (m_bActive)
	{
		for (int i = 0; i < m_iTeamCount; i++)
		{
			int index = g_pGameRules ? g_pGameRules->GetTeamIndex(m_szTeamNames[i]) : -1;
			m_TeamNumbers[i] = index >= 0 && index < MAX_TEAMS ? index + 1 : 0;
		}
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (!serverapi()->IsScoreboardClient(i) || gpGlobals->time < m_Clients[i].nextSendTime)
			continue;

		if (!SendToClient(i))
			m_Clients[i].nextSendTime = gpGlobals->time + BUDGET_DELAY;
	}
}

//----------------------------------------------------------------------------------------------------
// IScoreboardExt methods
//----------------------------------------------------------------------------------------------------
void CScoreboardExt::Reset()
{
	ClearSchema();

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		if (i <= gpGlobals->maxClients && serverapi()->IsScoreboardClient(i))
			StartFullSync(i, false);
		else
			m_Clients[i] = client_state_t();
	}
}

void CScoreboardExt::SetConfig(unsigned int hideFlags, const char *preset, const char *title, const char *tag)
{
	m_iHideFlags = hideFlags & 0xFFFF;
	CopyString(m_szPreset, preset);
	CopyString(m_szTitle, title);
	CopyString(m_szTag, tag);

	if (m_bActive)
	{
		for (client_state_t &state : m_Clients)
			state.isConfigPending = true;

		return;
	}

	m_bActive = true;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (serverapi()->IsScoreboardClient(i))
			StartFullSync(i, false);
	}

	serverapi()->QueryUnknownClientCaps();
}

bool CScoreboardExt::AddColumn(const char *key, const char *title, int display, int width, int sort, unsigned int flags)
{
	if (!IsValidKey(key))
		return false;

	int id = FindColumn(key);

	if (id == -1)
	{
		if (m_iColumnCount == bhl::sb::MAX_COLUMNS)
			return false;

		id = m_iColumnCount++;
	}

	column_t &column = m_Columns[id];
	CopyString(column.key, key);
	CopyString(column.title, title);
	column.display = std::clamp(display, 0, 255);
	column.flags = flags & 0xFFFF;
	column.width = std::clamp(width, 0, 255);
	column.sort = std::clamp(sort, 0, 255);

	if (column.display == (int)bhl::sb::E_Display::Builtin)
	{
		for (int target = 0; target < MAX_TARGETS; target++)
			ClearCell(target, id);
	}

	if (m_bActive)
	{
		for (client_state_t &state : m_Clients)
			state.columnsPending |= 1u << id;
	}

	return true;
}

int CScoreboardExt::GetTeamTarget(const char *teamName)
{
	if (!teamName || !teamName[0])
		return -1;

	char name[MAX_TEAM_NAME];
	CopyString(name, teamName);

	for (int i = 0; i < m_iTeamCount; i++)
	{
		if (!_stricmp(m_szTeamNames[i], name))
			return TEAM_TARGET_BASE + i;
	}

	if (m_iTeamCount == MAX_TEAMS)
		return -1;

	CopyString(m_szTeamNames[m_iTeamCount], name);
	return TEAM_TARGET_BASE + m_iTeamCount++;
}

void CScoreboardExt::SetInt(int target, const char *key, int value)
{
	value_t cell;
	cell.type = bhl::sb::E_ValueType::Int;
	cell.value = value;
	SetCell(target, key, cell);
}

void CScoreboardExt::SetText(int target, const char *key, const char *text)
{
	value_t cell;
	cell.type = bhl::sb::E_ValueType::Text;
	CopyString(cell.text, text);
	SetCell(target, key, cell);
}

void CScoreboardExt::SetIcon(int target, const char *key, const char *sprite, const char *text)
{
	value_t cell;
	cell.type = bhl::sb::E_ValueType::Icon;
	CopyString(cell.sprite, sprite);
	CopyString(cell.text, text);
	SetCell(target, key, cell);
}

void CScoreboardExt::SetProgress(int target, const char *key, int cur, int max)
{
	value_t cell;
	cell.type = bhl::sb::E_ValueType::Progress;
	cell.value = cur;
	cell.max = max;
	SetCell(target, key, cell);
}

void CScoreboardExt::Clear(int target, const char *key)
{
	if (key && key[0])
	{
		SetCell(target, key, value_t());
		return;
	}

	int cellTarget = GetCellTarget(target);

	if (cellTarget == -1)
		return;

	for (int column = 0; column < m_iColumnCount; column++)
		ClearCell(cellTarget, column);
}

bool CScoreboardExt::ListResize(const char *key, int count)
{
	if (!IsValidKey(key))
		return false;

	int id = FindList(key);

	if (id == -1)
	{
		if (m_iListCount == bhl::sb::MAX_LISTS)
			return false;

		id = m_iListCount++;
		CopyString(m_Lists[id].key, key);
	}

	list_t &list = m_Lists[id];
	count = std::clamp(count, 0, bhl::sb::MAX_LIST_ITEMS);

	if (list.count == count)
		return true;

	for (int i = count; i < list.count; i++)
		list.items[i] = value_t();

	uint64_t itemsMask = count == bhl::sb::MAX_LIST_ITEMS ? ~0ULL : (1ULL << count) - 1;

	// The client still has old values there if it missed the smaller total, so grown items are sent even if empty
	uint64_t grownItems = count > list.count ? itemsMask & ~((1ULL << list.count) - 1) : 0;

	list.count = count;

	if (m_bActive)
	{
		for (client_state_t &state : m_Clients)
		{
			state.listsResized |= 1u << id;
			state.listItemsDirty[id] = (state.listItemsDirty[id] & itemsMask) | grownItems;
		}
	}

	return true;
}

void CScoreboardExt::ListSetInt(const char *key, int index, int value)
{
	value_t item;
	item.type = bhl::sb::E_ValueType::Int;
	item.value = value;
	SetListItem(key, index, item);
}

void CScoreboardExt::ListSetText(const char *key, int index, const char *text)
{
	value_t item;
	item.type = bhl::sb::E_ValueType::Text;
	CopyString(item.text, text);
	SetListItem(key, index, item);
}

void CScoreboardExt::ListSetIcon(const char *key, int index, const char *sprite, const char *text)
{
	value_t item;
	item.type = bhl::sb::E_ValueType::Icon;
	CopyString(item.sprite, sprite);
	CopyString(item.text, text);
	SetListItem(key, index, item);
}

//----------------------------------------------------------------------------------------------------
// State
//----------------------------------------------------------------------------------------------------
void CScoreboardExt::ClearSchema()
{
	m_bActive = false;
	m_iHideFlags = 0;
	m_szPreset[0] = '\0';
	m_szTitle[0] = '\0';
	m_szTag[0] = '\0';

	for (column_t &column : m_Columns)
		column = column_t();

	m_iColumnCount = 0;

	for (list_t &list : m_Lists)
	{
		list.key[0] = '\0';
		list.count = 0;

		for (value_t &item : list.items)
			item = value_t();
	}

	m_iListCount = 0;

	for (auto &cells : m_Cells)
	{
		for (value_t &cell : cells)
			cell = value_t();
	}
}

int CScoreboardExt::GetCellTarget(int target)
{
	if (target == bhl::sb::TARGET_GLOBAL)
		return 0;

	if (target >= 1 && target <= gpGlobals->maxClients)
		return serverapi()->IsClientConnected(target) ? target : -1;

	if (target >= TEAM_TARGET_BASE && target < TEAM_TARGET_BASE + m_iTeamCount)
		return FIRST_TEAM + target - TEAM_TARGET_BASE;

	return -1;
}

int CScoreboardExt::FindColumn(const char *key)
{
	if (!key)
		return -1;

	for (int i = 0; i < m_iColumnCount; i++)
	{
		if (!strcmp(m_Columns[i].key, key))
			return i;
	}

	return -1;
}

int CScoreboardExt::FindList(const char *key)
{
	if (!key)
		return -1;

	for (int i = 0; i < m_iListCount; i++)
	{
		if (!strcmp(m_Lists[i].key, key))
			return i;
	}

	return -1;
}

void CScoreboardExt::SetCell(int target, const char *key, const value_t &value)
{
	int cellTarget = GetCellTarget(target);
	int column = FindColumn(key);

	if (cellTarget == -1 || column == -1 || m_Columns[column].display == (int)bhl::sb::E_Display::Builtin)
		return;

	value_t &cell = m_Cells[cellTarget][column];

	if (IsEqual(cell, value))
		return;

	cell = value;

	if (m_bActive)
	{
		for (client_state_t &state : m_Clients)
			state.cellsDirty[cellTarget] |= 1u << column;
	}
}

void CScoreboardExt::ClearCell(int target, int column)
{
	value_t &cell = m_Cells[target][column];

	if (cell.type == bhl::sb::E_ValueType::None)
		return;

	cell = value_t();

	if (m_bActive)
	{
		for (client_state_t &state : m_Clients)
			state.cellsDirty[target] |= 1u << column;
	}
}

void CScoreboardExt::SetListItem(const char *key, int index, const value_t &value)
{
	int id = FindList(key);

	if (id == -1 || index < 0 || index >= m_Lists[id].count)
		return;

	value_t &item = m_Lists[id].items[index];

	if (IsEqual(item, value))
		return;

	item = value;

	if (m_bActive)
	{
		for (client_state_t &state : m_Clients)
			state.listItemsDirty[id] |= 1ULL << index;
	}
}

//----------------------------------------------------------------------------------------------------
// Sending
//----------------------------------------------------------------------------------------------------
bool CScoreboardExt::SendToClient(int idx)
{
	client_state_t &state = m_Clients[idx];
	edict_t *pEdict = INDEXENT(idx);

	m_iBytesLeft = FRAME_BUDGET;

	if (state.isResetPending)
	{
		if (!BeginMessage(pEdict, 2))
			return false;

		WRITE_BYTE((int)bhl::sb::E_Op::Reset);
		WRITE_BYTE(bhl::sb::PROTOCOL_VERSION);
		MESSAGE_END();
		state.isResetPending = false;
	}

	if (!m_bActive)
		return true;

	UpdateClientTeams(state);

	if (state.isConfigPending)
	{
		int size = 3 + (int)strlen(m_szPreset) + 1 + (int)strlen(m_szTitle) + 1 + (int)strlen(m_szTag) + 1;

		if (!BeginMessage(pEdict, size))
			return false;

		WRITE_BYTE((int)bhl::sb::E_Op::Config);
		WRITE_SHORT(m_iHideFlags);
		WRITE_STRING(m_szPreset);
		WRITE_STRING(m_szTitle);
		WRITE_STRING(m_szTag);
		MESSAGE_END();
		state.isConfigPending = false;
	}

	for (int i = 0; i < m_iColumnCount; i++)
	{
		if (!(state.columnsPending & (1u << i)))
			continue;

		if (!SendColumn(pEdict, i))
			return false;

		state.columnsPending &= ~(1u << i);
	}

	for (int i = 0; i < m_iListCount; i++)
	{
		if (!SendList(pEdict, state, i))
			return false;
	}

	return SendValues(pEdict, state);
}

void CScoreboardExt::UpdateClientTeams(client_state_t &state)
{
	// Without mp_teamlist team indexes change when teams appear and disappear
	for (int i = 0; i < m_iTeamCount; i++)
	{
		int number = m_TeamNumbers[i];

		if (state.teamNumbers[i] == number)
			continue;

		if (state.teamNumbers[i] != 0)
			state.teamsCleared |= 1ULL << (state.teamNumbers[i] - 1);

		state.teamNumbers[i] = number;
		state.cellsDirty[FIRST_TEAM + i] = 0;

		if (number == 0)
			continue;

		for (int column = 0; column < m_iColumnCount; column++)
		{
			if (m_Cells[FIRST_TEAM + i][column].type != bhl::sb::E_ValueType::None)
				state.cellsDirty[FIRST_TEAM + i] |= 1u << column;
		}
	}
}

bool CScoreboardExt::BeginMessage(edict_t *pEdict, int size)
{
	if (size > m_iBytesLeft)
		return false;

	m_iBytesLeft -= size;
	MESSAGE_BEGIN(MSG_ONE, gmsgBhlScore, NULL, pEdict);
	return true;
}

bool CScoreboardExt::SendColumn(edict_t *pEdict, int id)
{
	const column_t &column = m_Columns[id];
	int size = 7 + (int)strlen(column.key) + 1 + (int)strlen(column.title) + 1;

	if (!BeginMessage(pEdict, size))
		return false;

	WRITE_BYTE((int)bhl::sb::E_Op::Column);
	WRITE_BYTE(id);
	WRITE_BYTE(column.display);
	WRITE_SHORT(column.flags);
	WRITE_BYTE(column.width);
	WRITE_BYTE(column.sort);
	WRITE_STRING(column.key);
	WRITE_STRING(column.title);
	MESSAGE_END();
	return true;
}

bool CScoreboardExt::SendList(edict_t *pEdict, client_state_t &state, int id)
{
	const list_t &list = m_Lists[id];
	uint64_t &dirty = state.listItemsDirty[id];
	unsigned int resized = 1u << id;
	int headerSize = 4 + (int)strlen(list.key) + 1;

	while (dirty || (state.listsResized & resized))
	{
		int start = 0;
		int end = 0;
		int size = headerSize;

		if (dirty)
		{
			int last = bhl::sb::MAX_LIST_ITEMS - 1;

			while (!(dirty & (1ULL << start)))
				start++;

			while (!(dirty & (1ULL << last)))
				last--;

			// Clean items between dirty ones are sent again to keep the run contiguous
			end = start;

			while (end <= last && size + GetValueSize(list.items[end]) <= bhl::sb::MAX_MSG_SIZE)
				size += GetValueSize(list.items[end++]);
		}

		if (!BeginMessage(pEdict, size))
			return false;

		WRITE_BYTE((int)bhl::sb::E_Op::List);
		WRITE_BYTE(id);
		WRITE_BYTE(list.count);
		WRITE_BYTE(start);
		WRITE_STRING(list.key);

		for (int i = start; i < end; i++)
		{
			WriteValue(list.items[i]);
			dirty &= ~(1ULL << i);
		}

		MESSAGE_END();
		state.listsResized &= ~resized;
	}

	return true;
}

bool CScoreboardExt::SendValues(edict_t *pEdict, client_state_t &state)
{
	m_iMsgCellCount = 0;
	m_iMsgSize = 1;

	// Clears go first: a team number may already be reused by another team
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		if ((state.playersCleared & (1ULL << (i - 1))) && !AddValuesCell(pEdict, state, i, -1, bhl::sb::ALL_COLUMNS))
			return false;
	}

	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		if ((state.teamsCleared & (1ULL << (i - 1))) && !AddValuesCell(pEdict, state, bhl::sb::TARGET_TEAM_BASE + i, -1, bhl::sb::ALL_COLUMNS))
			return false;
	}

	for (int target = 0; target < MAX_TARGETS; target++)
	{
		unsigned int dirty = state.cellsDirty[target];

		if (!dirty)
			continue;

		int wire = target;

		if (target >= FIRST_TEAM)
		{
			int number = state.teamNumbers[target - FIRST_TEAM];

			if (number == 0)
			{
				state.cellsDirty[target] = 0;
				continue;
			}

			wire = bhl::sb::TARGET_TEAM_BASE + number;
		}

		for (int column = 0; column < m_iColumnCount; column++)
		{
			if ((dirty & (1u << column)) && !AddValuesCell(pEdict, state, wire, target, column))
				return false;
		}
	}

	return FlushValues(pEdict, state);
}

bool CScoreboardExt::AddValuesCell(edict_t *pEdict, client_state_t &state, int wire, int target, int column)
{
	int cellSize = 1 + (target == -1 ? 2 : GetValueSize(m_Cells[target][column]));
	bool isSameGroup = m_iMsgCellCount > 0 && m_MsgCells[m_iMsgCellCount - 1].wire == wire;
	int size = isSameGroup ? cellSize : cellSize + 2;

	if (m_iMsgSize + size > bhl::sb::MAX_MSG_SIZE)
	{
		if (!FlushValues(pEdict, state))
			return false;

		size = cellSize + 2;
	}

	msg_cell_t &cell = m_MsgCells[m_iMsgCellCount++];
	cell.wire = wire;
	cell.target = target;
	cell.column = column;
	m_iMsgSize += size;
	return true;
}

bool CScoreboardExt::FlushValues(edict_t *pEdict, client_state_t &state)
{
	if (m_iMsgCellCount == 0)
		return true;

	if (!BeginMessage(pEdict, m_iMsgSize))
		return false;

	WRITE_BYTE((int)bhl::sb::E_Op::Values);

	for (int i = 0; i < m_iMsgCellCount;)
	{
		int wire = m_MsgCells[i].wire;
		int count = 1;

		while (i + count < m_iMsgCellCount && m_MsgCells[i + count].wire == wire)
			count++;

		WRITE_BYTE(wire);
		WRITE_BYTE(count);

		for (; count > 0; count--, i++)
		{
			const msg_cell_t &cell = m_MsgCells[i];
			WRITE_BYTE(cell.column);

			if (cell.target != -1)
			{
				WriteValue(m_Cells[cell.target][cell.column]);
				state.cellsDirty[cell.target] &= ~(1u << cell.column);
			}
			else
			{
				WriteValue(value_t());

				if (wire > bhl::sb::TARGET_TEAM_BASE)
					state.teamsCleared &= ~(1ULL << (wire - bhl::sb::TARGET_TEAM_FIRST));
				else
					state.playersCleared &= ~(1ULL << (wire - bhl::sb::TARGET_PLAYER_FIRST));
			}
		}
	}

	MESSAGE_END();
	m_iMsgCellCount = 0;
	m_iMsgSize = 1;
	return true;
}

bool CScoreboardExt::IsEqual(const value_t &a, const value_t &b)
{
	return a.type == b.type && a.value == b.value && a.max == b.max && !strcmp(a.text, b.text) && !strcmp(a.sprite, b.sprite);
}

int CScoreboardExt::GetValueSize(const value_t &value)
{
	int len = 0;

	switch (value.type)
	{
	case bhl::sb::E_ValueType::Int:
		if (value.value >= -128 && value.value <= 127)
			len = 1;
		else if (value.value >= -32768 && value.value <= 32767)
			len = 2;
		else
			len = 4;
		break;
	case bhl::sb::E_ValueType::Text:
		len = (int)strlen(value.text) + 1;
		break;
	case bhl::sb::E_ValueType::Icon:
		len = (int)strlen(value.sprite) + 1 + (int)strlen(value.text) + 1;
		break;
	case bhl::sb::E_ValueType::Progress:
		len = IsByteProgress(value.value, value.max) ? 2 : 4;
		break;
	}

	return 2 + len;
}

void CScoreboardExt::WriteValue(const value_t &value)
{
	int len = GetValueSize(value) - 2;

	WRITE_BYTE((int)value.type);
	WRITE_BYTE(len);

	switch (value.type)
	{
	case bhl::sb::E_ValueType::Int:
		if (len == 1)
			WRITE_CHAR(value.value);
		else if (len == 2)
			WRITE_SHORT(value.value);
		else
			WRITE_LONG(value.value);
		break;
	case bhl::sb::E_ValueType::Text:
		WRITE_STRING(value.text);
		break;
	case bhl::sb::E_ValueType::Icon:
		WRITE_STRING(value.sprite);
		WRITE_STRING(value.text);
		break;
	case bhl::sb::E_ValueType::Progress:
		if (len == 2)
		{
			WRITE_BYTE(value.value);
			WRITE_BYTE(value.max);
		}
		else
		{
			WRITE_SHORT(ClampWord(value.value));
			WRITE_SHORT(ClampWord(value.max));
		}
		break;
	}
}
