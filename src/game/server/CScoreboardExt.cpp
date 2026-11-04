#include <algorithm>
#include <climits>
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

//! Index of the item with the key among the first count, -1 if none; "" matches nothing
template <typename T>
static int FindKey(const T *items, int count, const char *key)
{
	if (!key || !key[0])
		return -1;

	for (int i = 0; i < count; i++)
	{
		if (!strcmp(items[i].key, key))
			return i;
	}

	return -1;
}

//! Returns a mask of the lowest count bits; count may be the width of T
template <typename T>
static T GetLowBits(int count)
{
	return count >= (int)(sizeof(T) * 8) ? ~T(0) : (T(1) << count) - 1;
}

static int ClampByte(int value)
{
	return std::clamp(value, 0, 255);
}

static int ClampWord(int value)
{
	return std::clamp(value, 0, 65535);
}

//----------------------------------------------------------------------------------------------------
// CScoreboardExt
//----------------------------------------------------------------------------------------------------
void CScoreboardExt::ClearAll()
{
	ClearState();
	m_iTeamCount = 0;
	std::fill(std::begin(m_Clients), std::end(m_Clients), client_state_t());
	std::fill(std::begin(m_Pacing), std::end(m_Pacing), pacing_t());
}

void CScoreboardExt::ClearPlayer(int idx)
{
	bool hasCells = GetFilledCells(idx) != 0;
	std::fill(std::begin(m_Cells[idx]), std::end(m_Cells[idx]), value_t());

	for (client_state_t &state : m_Clients)
	{
		if (hasCells || state.cellsDirty[idx])
			state.targetsCleared.set(idx);

		state.cellsDirty[idx] = 0;
	}

	// Overrides of the slot only change what this client gets, and its sync state starts anew
	m_PlayerHud[idx] = player_hud_t();
	m_Clients[idx] = client_state_t();
	m_Pacing[idx] = pacing_t();
	m_Events[idx].clear();
}

void CScoreboardExt::StartFullSync(int idx, bool delay)
{
	client_state_t &state = m_Clients[idx];

	if (delay)
		m_Pacing[idx].nextSendTime = gpGlobals->time + FULL_SYNC_DELAY;

	state = client_state_t();
	state.isResetPending = true;

	if (!m_bActive)
		return;

	state.isConfigPending = m_bHasConfig;
	state.columnsPending = GetLowBits<unsigned int>(m_iColumnCount);

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
		state.cellsDirty[target] = GetFilledCells(target);

	// Free ids and empty variants send nothing
	state.elementsPending = ~0u;
	memset(state.templatesPending, 0xFF, sizeof(state.templatesPending));
}

void CScoreboardExt::StartFrame()
{
	if (!gmsgBhlScore)
		return;

	for (int i = 0; i < m_iTeamCount; i++)
	{
		int index = g_pGameRules ? g_pGameRules->GetTeamIndex(m_szTeamNames[i]) : -1;
		m_TeamNumbers[i] = index >= 0 && index < MAX_TEAMS ? index + 1 : 0;
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		pacing_t &pacing = m_Pacing[i];

		if (!serverapi()->IsScoreboardClient(i) || gpGlobals->time < pacing.nextSendTime)
			continue;

		// A budget per period, not per frame: the rate must not grow with the server fps
		if (gpGlobals->time >= pacing.periodEnd)
		{
			pacing.periodEnd = gpGlobals->time + BUDGET_PERIOD;
			pacing.bytesLeft = BUDGET;
		}

		m_iBytesLeft = pacing.bytesLeft;

		if (!SendToClient(i))
			pacing.nextSendTime = pacing.periodEnd;

		pacing.bytesLeft = m_iBytesLeft;
	}
}

//----------------------------------------------------------------------------------------------------
// IScoreboardExt methods
//----------------------------------------------------------------------------------------------------
void CScoreboardExt::Reset()
{
	ClearState();

	for (int i = 1; i <= gpGlobals->maxClients; i++)
		StartFullSync(i, false);
}

void CScoreboardExt::SetConfig(unsigned int hideFlags, const char *title, const char *tag)
{
	char newTitle[sizeof(m_szTitle)];
	char newTag[sizeof(m_szTag)];

	hideFlags &= (unsigned int)bhl::sb::E_ConfigFlags::All;
	CopyString(newTitle, title);
	CopyString(newTag, tag);

	if (m_bHasConfig && hideFlags == m_iHideFlags && !strcmp(newTitle, m_szTitle) && !strcmp(newTag, m_szTag))
		return;

	Activate();

	m_bHasConfig = true;
	m_iHideFlags = hideFlags;
	CopyString(m_szTitle, newTitle);
	CopyString(m_szTag, newTag);

	for (client_state_t &state : m_Clients)
		state.isConfigPending = true;
}

bool CScoreboardExt::AddColumn(const char *key, const char *title, int display, int width, int sort, unsigned int flags)
{
	if (!bhl::sb::IsValidKey(key) || display < (int)bhl::sb::E_Display::Text || display > (int)bhl::sb::E_Display::Steps)
		return false;

	column_t column;
	CopyString(column.key, key);
	CopyString(column.title, title);
	column.display = display;
	column.flags = flags & (unsigned int)bhl::sb::E_ColumnFlags::All;
	column.width = ClampByte(width);
	column.sort = ClampByte(sort);
	return SetColumn(column);
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
	SetCell(target, key, { bhl::sb::E_ValueType::Int, value });
}

void CScoreboardExt::SetText(int target, const char *key, const char *text)
{
	SetCell(target, key, MakeValue(bhl::sb::E_ValueType::Text, "", text));
}

void CScoreboardExt::SetIcon(int target, const char *key, const char *sprite, const char *text)
{
	SetCell(target, key, MakeValue(bhl::sb::E_ValueType::Icon, sprite, text));
}

void CScoreboardExt::SetProgress(int target, const char *key, int cur, int max)
{
	SetCell(target, key, { bhl::sb::E_ValueType::Progress, ClampWord(cur), ClampWord(max) });
}

void CScoreboardExt::Clear(int target, const char *key)
{
	for (int column = 0; column < m_iColumnCount; column++)
	{
		if (!key || !key[0] || !strcmp(m_Columns[column].key, key))
			SetCell(target, m_Columns[column].key, value_t());
	}
}

bool CScoreboardExt::ListResize(const char *key, int count)
{
	if (!bhl::sb::IsValidKey(key))
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

	uint64_t itemsMask = GetLowBits<uint64_t>(count);

	// The client still has old values there if it missed the smaller total, so grown items are sent even if empty
	uint64_t grownItems = count > list.count ? itemsMask & ~GetLowBits<uint64_t>(list.count) : 0;

	list.count = count;

	for (client_state_t &state : m_Clients)
	{
		state.listsResized |= 1u << id;
		state.listItemsDirty[id] = (state.listItemsDirty[id] & itemsMask) | grownItems;
	}

	return true;
}

void CScoreboardExt::ListSetInt(const char *key, int index, int value)
{
	SetListItem(key, index, { bhl::sb::E_ValueType::Int, value });
}

void CScoreboardExt::ListSetText(const char *key, int index, const char *text)
{
	SetListItem(key, index, MakeValue(bhl::sb::E_ValueType::Text, "", text));
}

void CScoreboardExt::ListSetIcon(const char *key, int index, const char *sprite, const char *text)
{
	SetListItem(key, index, MakeValue(bhl::sb::E_ValueType::Icon, sprite, text));
}

void CScoreboardExt::SetClock(int target, const char *key, int ms, unsigned int flags)
{
	flags &= (unsigned int)bhl::sb::E_ClockFlags::All;

	// A running clock runs from this call, so a call with the same values restarts it
	SetCell(target, key, { bhl::sb::E_ValueType::Clock, ms, 0, flags, (flags & (unsigned int)bhl::sb::E_ClockFlags::Run) ? gpGlobals->time : 0.0f });
}

bool CScoreboardExt::AddBadge(const char *key, const char *host)
{
	if (!bhl::sb::IsValidKey(key) || !bhl::sb::IsValidKey(host))
		return false;

	column_t column;
	CopyString(column.key, key);
	CopyString(column.host, host);
	column.display = (int)bhl::sb::E_Display::Badge;
	return SetColumn(column);
}

bool CScoreboardExt::HudAdd(const char *key, int kind, float x, float y, int font, int align, unsigned int flags)
{
	if (!bhl::sb::IsValidKey(key) || !bhl::sb::IsKnownKind(kind))
		return false;

	int id = FindElement(key);
	bool isNew = id == -1;

	if (isNew)
	{
		id = FindFreeElementId();

		if (id == -1)
			return false;
	}

	Activate();

	element_t &element = m_Elements[id];
	element_def_t def = element.def;

	if (!isNew && def.kind != kind)
	{
		DropTexts(id);
		def.select[0] = '\0';
		def.value[0] = '\0';
		def.list[0] = '\0';
		def.paramsSet = 0;
		def.slotsSet = 0;
	}

	def.kind = kind;
	def.flags = flags & (unsigned int)bhl::sb::E_ElementFlags::All & ~(unsigned int)bhl::sb::E_ElementFlags::HudColor;
	def.x = bhl::sb::PositionToWire(x);
	def.y = bhl::sb::PositionToWire(y);
	def.font = font >= 0 && font < bhl::sb::FONT_COUNT ? font : (int)bhl::sb::E_Font::Text;
	def.align = align & bhl::sb::ALIGN_MASK;

	if (isNew)
	{
		CopyString(element.key, key);
		m_iVisible |= 1u << id;
		MarkChildren(key);
	}

	return SetElementDef(id, def);
}

bool CScoreboardExt::HudSetPlace(const char *key, int place, const char *parent, int gap)
{
	int id = FindElement(key);

	if (id == -1 || place < (int)bhl::sb::E_Place::Screen || place > (int)bhl::sb::E_Place::ParentRight || (parent && parent[0] && !bhl::sb::IsValidKey(parent)))
		return false;

	element_def_t def = m_Elements[id].def;
	def.place = place;
	CopyString(def.parent, bhl::sb::IsParentPlace((bhl::sb::E_Place)place) ? parent : "");
	def.gap = ClampByte(gap);
	return SetElementDef(id, def);
}

bool CScoreboardExt::HudSetColor(const char *key, int r, int g, int b, int a)
{
	int id = FindElement(key);

	if (id == -1)
		return false;

	element_def_t def = m_Elements[id].def;
	def.isHudColor = r < 0;
	def.color[0] = def.isHudColor ? 0 : ClampByte(r);
	def.color[1] = def.isHudColor ? 0 : ClampByte(g);
	def.color[2] = def.isHudColor ? 0 : ClampByte(b);
	def.color[3] = ClampByte(a);
	return SetElementDef(id, def);
}

bool CScoreboardExt::HudBind(const char *key, const char *value, const char *list, const char *select)
{
	int id = FindElement(key);

	if (id == -1
	    || (value && value[0] && !bhl::sb::IsValidRef(value))
	    || (list && list[0] && !bhl::sb::IsValidKey(list))
	    || (select && select[0] && !bhl::sb::IsValidRef(select)))
		return false;

	element_def_t def = m_Elements[id].def;
	CopyString(def.value, value);
	CopyString(def.list, list);
	CopyString(def.select, select);
	return SetElementDef(id, def);
}

bool CScoreboardExt::HudSetParam(const char *key, int param, int value)
{
	int id = FindElement(key);

	if (id == -1)
		return false;

	element_def_t def = m_Elements[id].def;
	const bhl::sb::ParamInfo *info = bhl::sb::FindParamInfo((bhl::sb::E_ElementKind)def.kind, param);

	if (!info)
		return false;

	def.paramsSet |= 1ULL << param;
	def.params[param] = std::clamp(value, info->iMin, info->iMax);
	return SetElementDef(id, def);
}

bool CScoreboardExt::HudSetText(int player, const char *key, int variant, const char *text)
{
	int id = FindElement(key);

	if (!text)
		text = "";

	if (id == -1 || variant < 0 || variant >= bhl::sb::MAX_VARIANTS || !IsValidHudPlayer(player) || strlen(text) > bhl::sb::MAX_TEMPLATE_LEN)
		return false;

	element_t &element = m_Elements[id];
	uint8_t bit = 1 << variant;

	if (player == 0)
	{
		// The default wins over every player's own text, even when it doesn't change (5.4)
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			if (GetText(i, id, variant) != text)
				m_Clients[i].templatesPending[id] |= bit;

			m_PlayerHud[i].texts[id][variant].reset();
		}

		element.texts[variant] = text;
		return true;
	}

	std::optional<std::string> &own = m_PlayerHud[player].texts[id][variant];

	if (GetText(player, id, variant) != text)
		m_Clients[player].templatesPending[id] |= bit;

	if (element.texts[variant] == text)
		own.reset();
	else
		own = text;

	return true;
}

bool CScoreboardExt::HudShow(int player, const char *key, bool show)
{
	int id = FindElement(key);

	if (id == -1 || !IsValidHudPlayer(player))
		return false;

	unsigned int bit = 1u << id;

	if (player == 0)
	{
		m_iVisible = show ? m_iVisible | bit : m_iVisible & ~bit;

		for (player_hud_t &hud : m_PlayerHud)
			hud.visibleToggled &= ~bit;

		return true;
	}

	if (show == ((m_iVisible & bit) != 0))
		m_PlayerHud[player].visibleToggled &= ~bit;
	else
		m_PlayerHud[player].visibleToggled |= bit;

	return true;
}

void CScoreboardExt::HudRemove(const char *key)
{
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		if (m_Elements[id].key[0] && (!key || !key[0] || !strcmp(m_Elements[id].key, key)))
			RemoveElement(id);
	}
}

void CScoreboardExt::HudHideStd(unsigned int parts, bool hide)
{
	parts &= (unsigned int)bhl::sb::E_HudStd::All;

	unsigned int hidden = hide ? m_iHiddenStd | parts : m_iHiddenStd & ~parts;

	if (hidden == m_iHiddenStd)
		return;

	Activate();
	m_iHiddenStd = hidden;
}

bool CScoreboardExt::HudSetSlotColor(const char *key, int slot, int r, int g, int b, int a)
{
	int id = FindElement(key);

	if (id == -1 || slot < 0 || slot >= bhl::sb::MAX_COLOR_SLOTS)
		return false;

	element_def_t def = m_Elements[id].def;

	if (r < 0)
	{
		def.slotsSet &= ~(1u << slot);
	}
	else
	{
		def.slotsSet |= 1u << slot;
		def.slotColors[slot][0] = ClampByte(r);
		def.slotColors[slot][1] = ClampByte(g);
		def.slotColors[slot][2] = ClampByte(b);
		def.slotColors[slot][3] = ClampByte(a);
	}

	return SetElementDef(id, def);
}

bool CScoreboardExt::HudEvent(int player, const char *key, int variant, int lifeMs, const int *args, int argCount, const char *iconSprite, const char *iconText)
{
	int id = FindElement(key);

	if (id == -1 || m_Elements[id].def.kind != (int)bhl::sb::E_ElementKind::Feed || variant < 0 || variant >= bhl::sb::MAX_VARIANTS
	    || argCount < 0 || argCount > bhl::sb::MAX_EVENT_INTS || (argCount > 0 && !args) || !IsValidHudPlayer(player))
		return false;

	event_t event;
	CopyString(event.key, key);
	event.variant = variant;
	event.lifeMs = ClampWord(lifeMs);
	event.time = gpGlobals->time;

	for (int i = 0; i < argCount; i++)
		event.args[i] = { bhl::sb::E_ValueType::Int, args[i] };

	event.argCount = argCount;

	if ((iconSprite && iconSprite[0]) || (iconText && iconText[0]))
		event.args[event.argCount++] = MakeValue(bhl::sb::E_ValueType::Icon, iconSprite, iconText);

	if (GetEventSize(event) > bhl::sb::MAX_MSG_SIZE)
		return false;

	// Events are not state: clients that are not ready now never get this one
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if ((player != 0 && player != i) || !serverapi()->IsScoreboardClient(i) || !IsEnumFlagSet(serverapi()->GetClientCaps(i), bhl::E_ClientCaps::HudV1))
			continue;

		if ((int)m_Events[i].size() == MAX_QUEUED_EVENTS)
			m_Events[i].pop_front();

		m_Events[i].push_back(event);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------
// State
//----------------------------------------------------------------------------------------------------
void CScoreboardExt::ClearState()
{
	m_bActive = false;
	m_bHasConfig = false;
	m_iColumnCount = 0;
	std::fill(std::begin(m_Lists), std::end(m_Lists), list_t());
	m_iListCount = 0;

	for (auto &cells : m_Cells)
		std::fill(std::begin(cells), std::end(cells), value_t());

	std::fill(std::begin(m_Elements), std::end(m_Elements), element_t());
	m_iVisible = 0;
	m_iHiddenStd = 0;
	std::fill(std::begin(m_PlayerHud), std::end(m_PlayerHud), player_hud_t());

	for (std::deque<event_t> &queue : m_Events)
		queue.clear();
}

void CScoreboardExt::Activate()
{
	if (m_bActive)
		return;

	m_bActive = true;

	// Drops what was marked while inactive; clients still in the wait after InitHUD keep it
	for (int i = 1; i <= gpGlobals->maxClients; i++)
		StartFullSync(i, false);

	serverapi()->QueryUnknownClientCaps();
}

bool CScoreboardExt::IsValidHudPlayer(int player)
{
	return player == 0 || (player >= 1 && player <= gpGlobals->maxClients && serverapi()->IsClientConnected(player));
}

int CScoreboardExt::FindColumn(const char *key)
{
	return FindKey(m_Columns, m_iColumnCount, key);
}

int CScoreboardExt::FindList(const char *key)
{
	return FindKey(m_Lists, m_iListCount, key);
}

int CScoreboardExt::FindElement(const char *key)
{
	return FindKey(m_Elements, bhl::sb::MAX_ELEMENTS, key);
}

int CScoreboardExt::FindFreeElementId()
{
	// A new element under an id the client still has something of (the old element, its visibility bit,
	// a child placed at it) would show that old state until everything of the new one arrives. When every
	// free id is held, the lowest is reused: SendElements still drops the old templates with a removal.
	unsigned int held = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (!serverapi()->IsScoreboardClient(i))
			continue;

		const client_state_t &state = m_Clients[i];
		held |= state.elementsSent | state.sentVisible;

		for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
		{
			if ((state.elementsSent & (1u << id)) && state.sentParents[id] != bhl::sb::NO_ID)
				held |= 1u << state.sentParents[id];
		}
	}

	int fallback = -1;

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		if (m_Elements[id].key[0])
			continue;

		if (!(held & (1u << id)))
			return id;

		if (fallback == -1)
			fallback = id;
	}

	return fallback;
}

bool CScoreboardExt::SetColumn(const column_t &definition)
{
	int id = FindColumn(definition.key);
	bool isNew = id == -1;

	if (isNew)
	{
		if (m_iColumnCount == bhl::sb::MAX_COLUMNS)
			return false;

		id = m_iColumnCount++;
	}
	else if (IsEqual(m_Columns[id], definition))
	{
		return true;
	}

	m_Columns[id] = definition;

	if (definition.display == (int)bhl::sb::E_Display::Builtin)
	{
		for (int target = 0; target < MAX_TARGETS; target++)
			StoreCell(target, id, value_t());
	}

	// Badges that follow a new key were sent without their host
	unsigned int columns = 1u << id;

	if (isNew)
	{
		for (int i = 0; i < m_iColumnCount; i++)
		{
			if (!strcmp(m_Columns[i].host, definition.key))
				columns |= 1u << i;
		}
	}

	for (client_state_t &state : m_Clients)
		state.columnsPending |= columns;

	return true;
}

void CScoreboardExt::SetCell(int target, const char *key, const value_t &value)
{
	int column = FindColumn(key);

	if (column == -1 || m_Columns[column].display == (int)bhl::sb::E_Display::Builtin)
		return;

	if (target >= TEAM_TARGET_BASE && target < TEAM_TARGET_BASE + m_iTeamCount)
		StoreCell(FIRST_TEAM + target - TEAM_TARGET_BASE, column, value);
	else if (target == bhl::sb::TARGET_GLOBAL || (target >= 1 && target <= gpGlobals->maxClients && serverapi()->IsClientConnected(target)))
		StoreCell(target, column, value);
}

void CScoreboardExt::StoreCell(int target, int column, const value_t &value)
{
	value_t &cell = m_Cells[target][column];

	if (IsEqual(cell, value))
		return;

	cell = value;

	for (client_state_t &state : m_Clients)
		state.cellsDirty[target] |= 1u << column;
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

	for (client_state_t &state : m_Clients)
		state.listItemsDirty[id] |= 1ULL << index;
}

unsigned int CScoreboardExt::GetFilledCells(int target)
{
	unsigned int cells = 0;

	for (int column = 0; column < m_iColumnCount; column++)
	{
		if (m_Cells[target][column].type != bhl::sb::E_ValueType::None)
			cells |= 1u << column;
	}

	return cells;
}

bool CScoreboardExt::SetElementDef(int id, const element_def_t &def)
{
	if (GetElementSize(def) > bhl::sb::MAX_MSG_SIZE)
		return false;

	if (IsEqual(m_Elements[id].def, def))
		return true;

	m_Elements[id].def = def;
	MarkElement(id);
	return true;
}

void CScoreboardExt::RemoveElement(int id)
{
	unsigned int bit = 1u << id;

	MarkChildren(m_Elements[id].key);
	DropTexts(id);
	m_Elements[id] = element_t();
	m_iVisible &= ~bit;

	for (player_hud_t &hud : m_PlayerHud)
		hud.visibleToggled &= ~bit;

	MarkElement(id);
}

void CScoreboardExt::DropTexts(int id)
{
	for (std::string &text : m_Elements[id].texts)
		text.clear();

	for (player_hud_t &hud : m_PlayerHud)
	{
		for (std::optional<std::string> &text : hud.texts[id])
			text.reset();
	}

	// A new definition keeps the client's templates: they are dropped by a removal sent before it
	for (client_state_t &state : m_Clients)
		state.textsDropped |= 1u << id;
}

const std::string &CScoreboardExt::GetText(int player, int id, int variant)
{
	const std::optional<std::string> &own = m_PlayerHud[player].texts[id][variant];
	return own ? *own : m_Elements[id].texts[variant];
}

void CScoreboardExt::MarkElement(int id)
{
	for (client_state_t &state : m_Clients)
		state.elementsPending |= 1u << id;
}

void CScoreboardExt::MarkChildren(const char *parent)
{
	for (int i = 0; i < bhl::sb::MAX_ELEMENTS; i++)
	{
		if (m_Elements[i].key[0] && !strcmp(m_Elements[i].def.parent, parent))
			MarkElement(i);
	}
}

//----------------------------------------------------------------------------------------------------
// Sending
//----------------------------------------------------------------------------------------------------
bool CScoreboardExt::SendToClient(int idx)
{
	client_state_t &state = m_Clients[idx];
	edict_t *pEdict = INDEXENT(idx);

	if (state.isResetPending)
	{
		if (!BeginMessage(pEdict, 1, bhl::sb::E_Op::Reset))
			return false;

		MESSAGE_END();
		state.isResetPending = false;
	}

	if (!m_bActive)
		return true;

	// Without mp_teamlist team indexes change when teams appear and disappear
	for (int i = 0; i < m_iTeamCount; i++)
	{
		if (state.teamNumbers[i] == m_TeamNumbers[i])
			continue;

		if (state.teamNumbers[i] != 0)
			state.targetsCleared.set(bhl::sb::TARGET_TEAM_BASE + state.teamNumbers[i]);

		state.teamNumbers[i] = m_TeamNumbers[i];
		state.cellsDirty[FIRST_TEAM + i] = GetFilledCells(FIRST_TEAM + i);
	}

	// Elements before their templates, templates before HudState shows them, events last
	return SendHeader(pEdict, state)
	    && SendLists(pEdict, state)
	    && SendValues(pEdict, state)
	    && SendElements(pEdict, state)
	    && SendTemplates(pEdict, idx, state)
	    && SendHudState(pEdict, idx, state)
	    && SendEvents(pEdict, idx, state);
}

bool CScoreboardExt::BeginMessage(edict_t *pEdict, int size, bhl::sb::E_Op op)
{
	if (size > m_iBytesLeft)
		return false;

	m_iBytesLeft -= size;
	MESSAGE_BEGIN(MSG_ONE, gmsgBhlScore, NULL, pEdict);
	WRITE_BYTE((int)op);
	return true;
}

bool CScoreboardExt::SendHeader(edict_t *pEdict, client_state_t &state)
{
	if (state.isConfigPending && m_bHasConfig)
	{
		int size = 3 + (int)strlen(m_szTitle) + 1 + (int)strlen(m_szTag) + 1;

		if (!BeginMessage(pEdict, size, bhl::sb::E_Op::Config))
			return false;

		WRITE_SHORT(m_iHideFlags);
		WRITE_STRING(m_szTitle);
		WRITE_STRING(m_szTag);
		MESSAGE_END();
	}

	state.isConfigPending = false;

	for (int i = 0; i < m_iColumnCount; i++)
	{
		if (!(state.columnsPending & (1u << i)))
			continue;

		const column_t &column = m_Columns[i];
		int size = 7 + (int)strlen(column.key) + 1 + (int)strlen(column.title) + 1 + 1;
		int host = column.display == (int)bhl::sb::E_Display::Badge ? FindColumn(column.host) : -1;

		if (!BeginMessage(pEdict, size, bhl::sb::E_Op::Column))
			return false;

		// AddBadge leaves flags, width and sort of a Badge at 0
		WRITE_BYTE(i);
		WRITE_BYTE(column.display);
		WRITE_SHORT(column.flags);
		WRITE_BYTE(column.width);
		WRITE_BYTE(column.sort);
		WRITE_STRING(column.key);
		WRITE_STRING(column.title);
		WRITE_BYTE(host == -1 ? bhl::sb::NO_ID : host);
		MESSAGE_END();
		state.columnsPending &= ~(1u << i);
	}

	return true;
}

bool CScoreboardExt::SendLists(edict_t *pEdict, client_state_t &state)
{
	for (int id = 0; id < m_iListCount; id++)
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
				while (!(dirty & (1ULL << start)))
					start++;

				// Up to the last dirty item: clean items between dirty ones are sent again to keep the run contiguous
				end = start;

				while ((dirty & ~GetLowBits<uint64_t>(end)) && size + GetValueSize(list.items[end]) <= bhl::sb::MAX_MSG_SIZE)
					size += GetValueSize(list.items[end++]);
			}

			if (!BeginMessage(pEdict, size, bhl::sb::E_Op::List))
				return false;

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
	}

	return true;
}

bool CScoreboardExt::SendValues(edict_t *pEdict, client_state_t &state)
{
	// Clears go first: a team number may already be reused by another team
	for (int wire = 0; wire < (int)state.targetsCleared.size(); wire++)
	{
		if (!state.targetsCleared[wire])
			continue;

		if (!BeginMessage(pEdict, 5, bhl::sb::E_Op::Values))
			return false;

		WRITE_BYTE(wire);
		WRITE_BYTE(bhl::sb::ALL_COLUMNS);
		WriteValue(value_t());
		MESSAGE_END();
		state.targetsCleared.reset(wire);
	}

	for (int target = 0; target < MAX_TARGETS; target++)
	{
		unsigned int &dirty = state.cellsDirty[target];
		int wire = target;

		if (target >= FIRST_TEAM)
		{
			int number = state.teamNumbers[target - FIRST_TEAM];

			// Its cells are marked again when the team gets a number
			if (number == 0)
				continue;

			wire = bhl::sb::TARGET_TEAM_BASE + number;
		}

		// One target per message with the cells that fit; the first one always does
		while (dirty)
		{
			unsigned int cells = 0;
			int size = 2;

			for (int column = 0; column < bhl::sb::MAX_COLUMNS; column++)
			{
				if (!(dirty & (1u << column)))
					continue;

				int cellSize = 1 + GetValueSize(m_Cells[target][column]);

				if (size + cellSize > bhl::sb::MAX_MSG_SIZE)
					break;

				cells |= 1u << column;
				size += cellSize;
			}

			if (!BeginMessage(pEdict, size, bhl::sb::E_Op::Values))
				return false;

			WRITE_BYTE(wire);

			for (int column = 0; column < bhl::sb::MAX_COLUMNS; column++)
			{
				if (cells & (1u << column))
				{
					WRITE_BYTE(column);
					WriteValue(m_Cells[target][column]);
				}
			}

			MESSAGE_END();
			dirty &= ~cells;
		}
	}

	return true;
}

bool CScoreboardExt::SendElements(edict_t *pEdict, client_state_t &state)
{
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		unsigned int bit = 1u << id;

		if (!(state.elementsPending & bit))
			continue;

		if (m_Elements[id].key[0])
		{
			// Both or neither (a removal is 3 bytes): a split would leave the new element with the old templates
			if ((state.textsDropped & bit) && (state.elementsSent & bit))
			{
				if (3 + GetElementSize(m_Elements[id].def) > m_iBytesLeft || !SendElementRemoval(pEdict, id))
					return false;

				state.templatesSent[id] = 0;
			}

			if (!SendElement(pEdict, state, id))
				return false;

			state.elementsSent |= bit;
		}
		else if (state.elementsSent & bit)
		{
			if (!SendElementRemoval(pEdict, id))
				return false;

			state.elementsSent &= ~bit;
			state.templatesSent[id] = 0;
		}

		state.textsDropped &= ~bit;
		state.elementsPending &= ~bit;
	}

	return true;
}

int CScoreboardExt::GetElementSize(const element_def_t &def)
{
	// 18 bytes from the opcode to font: an Element that ends before font is ignored
	return 18 + (int)strlen(def.select) + 1 + (int)strlen(def.value) + 1 + (int)strlen(def.list) + 1
	    + 1 + 3 * (int)std::bitset<bhl::sb::MAX_PARAM_ID + 1>(def.paramsSet).count()
	    + 1 + 5 * (int)std::bitset<bhl::sb::MAX_COLOR_SLOTS>(def.slotsSet).count();
}

bool CScoreboardExt::SendElement(edict_t *pEdict, client_state_t &state, int id)
{
	const element_def_t &def = m_Elements[id].def;
	int parent = FindElement(def.parent);

	if (!BeginMessage(pEdict, GetElementSize(def), bhl::sb::E_Op::Element))
		return false;

	state.sentParents[id] = parent == -1 ? bhl::sb::NO_ID : parent;

	WRITE_BYTE(id);
	WRITE_BYTE(def.kind);
	WRITE_SHORT(def.isHudColor ? def.flags | (unsigned int)bhl::sb::E_ElementFlags::HudColor : def.flags);
	WRITE_SHORT(def.x);
	WRITE_SHORT(def.y);
	WRITE_BYTE(def.align);
	WRITE_BYTE(def.place);
	WRITE_BYTE(state.sentParents[id]);
	WRITE_BYTE(def.gap);

	for (int component : def.color)
		WRITE_BYTE(component);

	WRITE_BYTE(def.font);
	WRITE_STRING(def.select);
	WRITE_STRING(def.value);
	WRITE_STRING(def.list);
	WRITE_BYTE((int)std::bitset<bhl::sb::MAX_PARAM_ID + 1>(def.paramsSet).count());

	for (int i = 0; i <= bhl::sb::MAX_PARAM_ID; i++)
	{
		if (!(def.paramsSet & (1ULL << i)))
			continue;

		WRITE_BYTE(i);
		WRITE_SHORT(def.params[i]);
	}

	WRITE_BYTE((int)std::bitset<bhl::sb::MAX_COLOR_SLOTS>(def.slotsSet).count());

	for (int i = 0; i < bhl::sb::MAX_COLOR_SLOTS; i++)
	{
		if (!(def.slotsSet & (1u << i)))
			continue;

		WRITE_BYTE(i);

		for (int component : def.slotColors[i])
			WRITE_BYTE(component);
	}

	MESSAGE_END();
	return true;
}

bool CScoreboardExt::SendElementRemoval(edict_t *pEdict, int id)
{
	if (!BeginMessage(pEdict, 3, bhl::sb::E_Op::Element))
		return false;

	WRITE_BYTE(id);
	WRITE_BYTE((int)bhl::sb::E_ElementKind::None);
	MESSAGE_END();
	return true;
}

bool CScoreboardExt::SendTemplates(edict_t *pEdict, int idx, client_state_t &state)
{
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		for (int variant = 0; variant < bhl::sb::MAX_VARIANTS; variant++)
		{
			uint8_t bit = 1 << variant;

			if (!(state.templatesPending[id] & bit))
				continue;

			const std::string &text = GetText(idx, id, variant);

			// "" removes the variant, so it only goes to a client that has one
			if (!text.empty() || (state.templatesSent[id] & bit))
			{
				if (!BeginMessage(pEdict, 3 + (int)text.size() + 1, bhl::sb::E_Op::Template))
					return false;

				WRITE_BYTE(id);
				WRITE_BYTE(variant);
				WRITE_STRING(text.c_str());
				MESSAGE_END();

				if (text.empty())
					state.templatesSent[id] &= ~bit;
				else
					state.templatesSent[id] |= bit;
			}

			state.templatesPending[id] &= ~bit;
		}
	}

	return true;
}

bool CScoreboardExt::SendHudState(edict_t *pEdict, int idx, client_state_t &state)
{
	unsigned int visible = m_iVisible ^ m_PlayerHud[idx].visibleToggled;

	if (visible == state.sentVisible && m_iHiddenStd == state.sentHidden)
		return true;

	if (!BeginMessage(pEdict, 7, bhl::sb::E_Op::HudState))
		return false;

	WRITE_LONG((int)visible);
	WRITE_SHORT(m_iHiddenStd);
	MESSAGE_END();
	state.sentVisible = visible;
	state.sentHidden = m_iHiddenStd;
	return true;
}

bool CScoreboardExt::SendEvents(edict_t *pEdict, int idx, client_state_t &state)
{
	std::deque<event_t> &queue = m_Events[idx];

	while (!queue.empty())
	{
		const event_t &event = queue.front();
		int id = FindElement(event.key);
		constexpr int param = (int)bhl::sb::E_Param::LifeMs;
		bool isParamSet = id != -1 && (m_Elements[id].def.paramsSet & (1ULL << param));
		int life = event.lifeMs ? event.lifeMs : (isParamSet ? m_Elements[id].def.params[param] : bhl::sb::FindParamInfo(bhl::sb::E_ElementKind::Feed, param)->iDefault);

		// Dropped unsent once its line would be gone
		if (gpGlobals->time <= event.time + life * 0.001f)
		{
			// Waits for the element, keeping the order
			if (id == -1 || m_Elements[id].def.kind != (int)bhl::sb::E_ElementKind::Feed || !(state.elementsSent & (1u << id)))
				return true;

			if (!BeginMessage(pEdict, GetEventSize(event), bhl::sb::E_Op::Event))
				return false;

			WRITE_BYTE(id);
			WRITE_BYTE(event.variant);
			WRITE_SHORT(event.lifeMs);
			WRITE_BYTE(event.argCount);

			for (int i = 0; i < event.argCount; i++)
				WriteValue(event.args[i]);

			MESSAGE_END();
		}

		queue.pop_front();
	}

	return true;
}

CScoreboardExt::value_t CScoreboardExt::MakeValue(bhl::sb::E_ValueType type, const char *sprite, const char *text)
{
	value_t value;
	value.type = type;
	CopyString(value.sprite, sprite);
	CopyString(value.text, text);
	return value;
}

bool CScoreboardExt::IsEqual(const value_t &a, const value_t &b)
{
	return a.type == b.type && a.value == b.value && a.max == b.max && a.flags == b.flags && a.time == b.time && !strcmp(a.text, b.text) && !strcmp(a.sprite, b.sprite);
}

bool CScoreboardExt::IsEqual(const column_t &a, const column_t &b)
{
	return !strcmp(a.key, b.key) && !strcmp(a.title, b.title) && a.display == b.display && a.flags == b.flags
	    && a.width == b.width && a.sort == b.sort && !strcmp(a.host, b.host);
}

bool CScoreboardExt::IsEqual(const element_def_t &a, const element_def_t &b)
{
	// Unset params and slot colors are compared too: def is always a copy of the stored one
	return a.kind == b.kind && a.flags == b.flags && a.x == b.x && a.y == b.y && a.align == b.align
	    && a.place == b.place && !strcmp(a.parent, b.parent) && a.gap == b.gap && a.isHudColor == b.isHudColor
	    && a.font == b.font && !strcmp(a.select, b.select) && !strcmp(a.value, b.value) && !strcmp(a.list, b.list)
	    && a.paramsSet == b.paramsSet && a.slotsSet == b.slotsSet && !memcmp(a.color, b.color, sizeof(a.color))
	    && !memcmp(a.params, b.params, sizeof(a.params)) && !memcmp(a.slotColors, b.slotColors, sizeof(a.slotColors));
}

int CScoreboardExt::GetEventSize(const event_t &event)
{
	int size = 6; // Opcode, id, variant, life and count

	for (int i = 0; i < event.argCount; i++)
		size += GetValueSize(event.args[i]);

	return size;
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
		len = value.value >= 0 && value.value <= 255 && value.max >= 0 && value.max <= 255 ? 2 : 4;
		break;
	case bhl::sb::E_ValueType::Clock:
		len = bhl::sb::CLOCK_VALUE_LEN;
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
			WRITE_SHORT(value.value);
			WRITE_SHORT(value.max);
		}
		break;
	case bhl::sb::E_ValueType::Clock:
	{
		// The client runs a clock from its arrival, so a running one goes with the time it has run since it was set
		int64_t ms = value.value;

		if (value.flags & (unsigned int)bhl::sb::E_ClockFlags::Run)
		{
			int64_t elapsed = (int64_t)(std::max(gpGlobals->time - value.time, 0.0f) * 1000.0 + 0.5);
			ms = (value.flags & (unsigned int)bhl::sb::E_ClockFlags::Down) ? std::max<int64_t>(ms - elapsed, 0) : std::min<int64_t>(ms + elapsed, INT_MAX);
		}

		WRITE_LONG((int)ms);
		WRITE_BYTE(value.flags);
		break;
	}
	}
}
