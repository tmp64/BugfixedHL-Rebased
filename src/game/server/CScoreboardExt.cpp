#include <algorithm>
#include <bitset>
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

static bool IsByteProgress(int cur, int max)
{
	return cur >= 0 && cur <= 255 && max >= 0 && max <= 255;
}

static bool IsParentPlace(int place)
{
	return bhl::sb::IsParentPlace((bhl::sb::E_Place)place);
}

//----------------------------------------------------------------------------------------------------
// CScoreboardExt
//----------------------------------------------------------------------------------------------------
void CScoreboardExt::ClearAll()
{
	ClearState();

	for (char *name : m_szTeamNames)
		name[0] = '\0';

	m_iTeamCount = 0;

	for (client_state_t &state : m_Clients)
		state = client_state_t();

	for (pacing_t &pacing : m_Pacing)
		pacing = pacing_t();
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

	// Overrides of the slot only change what this client gets, and its sync state starts anew
	m_PlayerHud[idx] = player_hud_t();
	m_Clients[idx] = client_state_t();
	m_Pacing[idx] = pacing_t();
	m_Events[idx] = event_queue_t();
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
	{
		for (int column = 0; column < m_iColumnCount; column++)
		{
			if (m_Cells[target][column].type != bhl::sb::E_ValueType::None)
				state.cellsDirty[target] |= 1u << column;
		}
	}

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		if (!m_Elements[id].key[0])
			continue;

		state.elementsPending |= 1u << id;

		for (int variant = 0; variant < bhl::sb::MAX_VARIANTS; variant++)
		{
			if (!GetText(idx, id, variant).empty())
				state.templatesPending[id] |= 1 << variant;
		}
	}

	state.isHudStatePending = true;
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
	cell.value = ClampWord(cur);
	cell.max = ClampWord(max);
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

void CScoreboardExt::SetClock(int target, const char *key, int ms, unsigned int flags)
{
	value_t cell;
	cell.type = bhl::sb::E_ValueType::Clock;
	cell.value = ms;
	cell.flags = flags & (unsigned int)bhl::sb::E_ClockFlags::All;
	SetCell(target, key, cell);
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

	if (!isNew)
	{
		SetElementDef(id, def);
		return true;
	}

	CopyString(element.key, key);
	element.def = def;
	MarkElement(id);
	MarkHudState();
	MarkChildren(element.key);
	return true;
}

bool CScoreboardExt::HudSetPlace(const char *key, int place, const char *parent, int gap)
{
	int id = FindElement(key);

	if (!parent)
		parent = "";

	if (id == -1 || place < (int)bhl::sb::E_Place::Screen || place > (int)bhl::sb::E_Place::ParentRight || (parent[0] && !bhl::sb::IsValidKey(parent)))
		return false;

	element_def_t def = m_Elements[id].def;
	def.place = place;
	CopyString(def.parent, IsParentPlace(place) ? parent : "");
	def.gap = ClampByte(gap);
	SetElementDef(id, def);
	return true;
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
	SetElementDef(id, def);
	return true;
}

bool CScoreboardExt::HudBind(const char *key, const char *value, const char *list, const char *select)
{
	int id = FindElement(key);

	if (!value)
		value = "";

	if (!list)
		list = "";

	if (!select)
		select = "";

	if (id == -1
	    || (value[0] && !bhl::sb::IsValidRef(value))
	    || (list[0] && !bhl::sb::IsValidKey(list))
	    || (select[0] && !bhl::sb::IsValidRef(select)))
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

	def.paramsSet |= bhl::sb::ParamBit(param);
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
			player_hud_t &hud = m_PlayerHud[i];

			if (GetText(i, id, variant) != text)
				m_Clients[i].templatesPending[id] |= bit;

			hud.textsSet[id] &= ~bit;
			hud.texts[id][variant].clear();
		}

		element.texts[variant] = text;
		return true;
	}

	player_hud_t &hud = m_PlayerHud[player];

	if (GetText(player, id, variant) != text)
		m_Clients[player].templatesPending[id] |= bit;

	if (element.texts[variant] == text)
	{
		hud.textsSet[id] &= ~bit;
		hud.texts[id][variant].clear();
	}
	else
	{
		hud.textsSet[id] |= bit;
		hud.texts[id][variant] = text;
	}

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
		m_Elements[id].isVisible = show;

		for (player_hud_t &hud : m_PlayerHud)
		{
			hud.visibleSet &= ~bit;
			hud.visible &= ~bit;
		}

		// HudState goes only to clients whose visibility differs from what they have
		MarkHudState();
		return true;
	}

	player_hud_t &hud = m_PlayerHud[player];

	if (show == m_Elements[id].isVisible)
	{
		hud.visibleSet &= ~bit;
		hud.visible &= ~bit;
	}
	else
	{
		hud.visibleSet |= bit;

		if (show)
			hud.visible |= bit;
		else
			hud.visible &= ~bit;
	}

	m_Clients[player].isHudStatePending = true;
	return true;
}

void CScoreboardExt::HudRemove(const char *key)
{
	if (key && key[0])
	{
		int id = FindElement(key);

		if (id != -1)
			RemoveElement(id);

		return;
	}

	bool hasElements = false;

	for (element_t &element : m_Elements)
	{
		if (element.key[0])
		{
			hasElements = true;
			element = element_t();
		}
	}

	if (!hasElements)
		return;

	for (player_hud_t &hud : m_PlayerHud)
		hud = player_hud_t();

	// The client drops every definition and template on "remove all"
	for (client_state_t &state : m_Clients)
	{
		state.isRemoveAllPending = true;
		state.elementsPending = 0;
		state.textsDropped = 0;
		memset(state.templatesPending, 0, sizeof(state.templatesPending));
		state.isHudStatePending = true;
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
	MarkHudState();
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
	{
		event.args[i].type = bhl::sb::E_ValueType::Int;
		event.args[i].value = args[i];
	}

	event.argCount = argCount;

	if ((iconSprite && iconSprite[0]) || (iconText && iconText[0]))
	{
		value_t &icon = event.args[event.argCount++];
		icon.type = bhl::sb::E_ValueType::Icon;
		CopyString(icon.sprite, iconSprite);
		CopyString(icon.text, iconText);
	}

	if (GetEventSize(event) > bhl::sb::MAX_MSG_SIZE)
		return false;

	// Events are not state: clients that are not ready now never get this one
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if ((player == 0 || player == i) && IsEventClient(i))
			QueueEvent(i, event);
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
	m_iHideFlags = 0;
	m_szTitle[0] = '\0';
	m_szTag[0] = '\0';

	for (column_t &column : m_Columns)
		column = column_t();

	m_iColumnCount = 0;

	for (list_t &list : m_Lists)
		list = list_t();

	m_iListCount = 0;

	for (auto &cells : m_Cells)
	{
		for (value_t &cell : cells)
			cell = value_t();
	}

	for (element_t &element : m_Elements)
		element = element_t();

	m_iHiddenStd = 0;

	for (player_hud_t &hud : m_PlayerHud)
		hud = player_hud_t();

	for (event_queue_t &queue : m_Events)
		queue = event_queue_t();
}

void CScoreboardExt::Activate()
{
	if (m_bActive)
		return;

	m_bActive = true;

	// Clients still in the wait after InitHUD keep it
	for (int i = 1; i <= gpGlobals->maxClients; i++)
		StartFullSync(i, false);

	serverapi()->QueryUnknownClientCaps();
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

bool CScoreboardExt::IsValidHudPlayer(int player)
{
	return player == 0 || (player >= 1 && player <= gpGlobals->maxClients && serverapi()->IsClientConnected(player));
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

int CScoreboardExt::FindElement(const char *key)
{
	if (!key || !key[0])
		return -1;

	for (int i = 0; i < bhl::sb::MAX_ELEMENTS; i++)
	{
		if (!strcmp(m_Elements[i].key, key))
			return i;
	}

	return -1;
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
			ClearCell(target, id);
	}

	MarkColumn(id);

	if (isNew)
		MarkBadges(definition.key);

	return true;
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
	char key[sizeof(m_Elements[id].key)];
	unsigned int bit = 1u << id;

	CopyString(key, m_Elements[id].key);
	DropTexts(id);
	m_Elements[id] = element_t();

	for (player_hud_t &hud : m_PlayerHud)
	{
		hud.visibleSet &= ~bit;
		hud.visible &= ~bit;
	}

	MarkElement(id);
	MarkHudState();
	MarkChildren(key);
}

void CScoreboardExt::DropTexts(int id)
{
	for (std::string &text : m_Elements[id].texts)
		text.clear();

	for (player_hud_t &hud : m_PlayerHud)
	{
		hud.textsSet[id] = 0;

		for (std::string &text : hud.texts[id])
			text.clear();
	}

	// A new definition keeps the client's templates: they are dropped by a removal sent before it
	for (client_state_t &state : m_Clients)
		state.textsDropped |= 1u << id;
}

const std::string &CScoreboardExt::GetText(int player, int id, int variant)
{
	const player_hud_t &hud = m_PlayerHud[player];
	return (hud.textsSet[id] & (1 << variant)) ? hud.texts[id][variant] : m_Elements[id].texts[variant];
}

unsigned int CScoreboardExt::GetVisibleElements(int player)
{
	const player_hud_t &hud = m_PlayerHud[player];
	unsigned int visible = 0;

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		unsigned int bit = 1u << id;

		if (!m_Elements[id].key[0])
			continue;

		if ((hud.visibleSet & bit) ? (hud.visible & bit) != 0 : m_Elements[id].isVisible)
			visible |= bit;
	}

	return visible;
}

bool CScoreboardExt::IsEventClient(int idx)
{
	return serverapi()->IsScoreboardClient(idx) && IsEnumFlagSet(serverapi()->GetClientCaps(idx), bhl::E_ClientCaps::HudV1);
}

void CScoreboardExt::QueueEvent(int idx, const event_t &event)
{
	event_queue_t &queue = m_Events[idx];

	if (queue.count == MAX_QUEUED_EVENTS)
	{
		queue.first = (queue.first + 1) % MAX_QUEUED_EVENTS;
		queue.count--;
	}

	queue.events[(queue.first + queue.count++) % MAX_QUEUED_EVENTS] = event;
}

int CScoreboardExt::GetEventLife(const event_t &event, int id)
{
	if (event.lifeMs != 0)
		return event.lifeMs;

	constexpr int param = (int)bhl::sb::E_Param::LifeMs;

	if (id != -1 && (m_Elements[id].def.paramsSet & bhl::sb::ParamBit(param)))
		return m_Elements[id].def.params[param];

	return bhl::sb::FindParamInfo(param)->iDefault;
}

void CScoreboardExt::MarkColumn(int id)
{
	if (!m_bActive)
		return;

	for (client_state_t &state : m_Clients)
		state.columnsPending |= 1u << id;
}

void CScoreboardExt::MarkBadges(const char *host)
{
	for (int i = 0; i < m_iColumnCount; i++)
	{
		if (m_Columns[i].display == (int)bhl::sb::E_Display::Badge && !strcmp(m_Columns[i].host, host))
			MarkColumn(i);
	}
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

void CScoreboardExt::MarkHudState()
{
	if (!m_bActive)
		return;

	for (client_state_t &state : m_Clients)
		state.isHudStatePending = true;
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
		if (!BeginMessage(pEdict, bhl::sb::RESET_SIZE))
			return false;

		WRITE_BYTE((int)bhl::sb::E_Op::Reset);
		WRITE_BYTE(bhl::sb::PROTOCOL_VERSION);
		MESSAGE_END();
		state.isResetPending = false;
	}

	if (!m_bActive)
		return true;

	UpdateClientTeams(state);

	// Elements before their templates, templates before HudState shows them, events last
	return SendHeader(pEdict, state)
	    && SendLists(pEdict, state)
	    && SendValues(pEdict, state)
	    && SendElements(pEdict, state)
	    && SendTemplates(pEdict, idx, state)
	    && SendHudState(pEdict, idx, state)
	    && SendEvents(pEdict, idx, state);
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

bool CScoreboardExt::SendHeader(edict_t *pEdict, client_state_t &state)
{
	if (state.isConfigPending && m_bHasConfig)
	{
		int size = 3 + (int)strlen(m_szTitle) + 1 + (int)strlen(m_szTag) + 1;

		if (!BeginMessage(pEdict, size))
			return false;

		WRITE_BYTE((int)bhl::sb::E_Op::Config);
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

		if (!SendColumn(pEdict, i))
			return false;

		state.columnsPending &= ~(1u << i);
	}

	return true;
}

bool CScoreboardExt::SendLists(edict_t *pEdict, client_state_t &state)
{
	for (int i = 0; i < m_iListCount; i++)
	{
		if (!SendList(pEdict, state, i))
			return false;
	}

	return true;
}

bool CScoreboardExt::SendColumn(edict_t *pEdict, int id)
{
	const column_t &column = m_Columns[id];
	int size = 7 + (int)strlen(column.key) + 1 + (int)strlen(column.title) + 1 + 1;
	int host = column.display == (int)bhl::sb::E_Display::Badge ? FindColumn(column.host) : -1;

	if (!BeginMessage(pEdict, size))
		return false;

	// AddBadge leaves flags, width and sort of a Badge at 0
	WRITE_BYTE((int)bhl::sb::E_Op::Column);
	WRITE_BYTE(id);
	WRITE_BYTE(column.display);
	WRITE_SHORT(column.flags);
	WRITE_BYTE(column.width);
	WRITE_BYTE(column.sort);
	WRITE_STRING(column.key);
	WRITE_STRING(column.title);
	WRITE_BYTE(host == -1 ? bhl::sb::NO_ID : host);
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

bool CScoreboardExt::SendElements(edict_t *pEdict, client_state_t &state)
{
	if (state.isRemoveAllPending)
	{
		if (state.elementsSent)
		{
			if (!SendElementRemoval(pEdict, bhl::sb::ALL_ELEMENTS))
				return false;

			state.elementsSent = 0;
			memset(state.templatesSent, 0, sizeof(state.templatesSent));
		}

		state.isRemoveAllPending = false;
	}

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		unsigned int bit = 1u << id;

		if (!(state.elementsPending & bit))
			continue;

		if (m_Elements[id].key[0])
		{
			// Both or neither: a split would leave the new element with the old templates
			if ((state.textsDropped & bit) && (state.elementsSent & bit))
			{
				if (bhl::sb::ELEMENT_REMOVAL_SIZE + GetElementSize(m_Elements[id].def) > m_iBytesLeft || !SendElementRemoval(pEdict, id))
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
	int size = bhl::sb::ELEMENT_FIXED_SIZE + (int)strlen(def.select) + 1 + (int)strlen(def.value) + 1 + (int)strlen(def.list) + 1 + 1 + 3 * GetParamCount(def);
	return def.slotsSet ? size + 1 + 5 * (int)std::bitset<bhl::sb::MAX_COLOR_SLOTS>(def.slotsSet).count() : size;
}

bool CScoreboardExt::SendElement(edict_t *pEdict, client_state_t &state, int id)
{
	const element_def_t &def = m_Elements[id].def;
	int parent = def.parent[0] && IsParentPlace(def.place) ? FindElement(def.parent) : -1;

	if (!BeginMessage(pEdict, GetElementSize(def)))
		return false;

	state.sentParents[id] = parent == -1 ? bhl::sb::NO_ID : parent;

	WRITE_BYTE((int)bhl::sb::E_Op::Element);
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
	WRITE_BYTE(GetParamCount(def));

	for (int i = 0; i <= bhl::sb::MAX_PARAM_ID; i++)
	{
		if (!(def.paramsSet & bhl::sb::ParamBit(i)))
			continue;

		WRITE_BYTE(i);
		WRITE_SHORT(def.params[i]);
	}

	// Only when used, so that an Element without slot colors stays as before
	if (def.slotsSet)
	{
		WRITE_BYTE((int)std::bitset<bhl::sb::MAX_COLOR_SLOTS>(def.slotsSet).count());

		for (int i = 0; i < bhl::sb::MAX_COLOR_SLOTS; i++)
		{
			if (!(def.slotsSet & (1u << i)))
				continue;

			WRITE_BYTE(i);

			for (int component : def.slotColors[i])
				WRITE_BYTE(component);
		}
	}

	MESSAGE_END();
	return true;
}

bool CScoreboardExt::SendElementRemoval(edict_t *pEdict, int id)
{
	if (!BeginMessage(pEdict, bhl::sb::ELEMENT_REMOVAL_SIZE))
		return false;

	WRITE_BYTE((int)bhl::sb::E_Op::Element);
	WRITE_BYTE(id);
	WRITE_BYTE((int)bhl::sb::E_ElementKind::None);
	MESSAGE_END();
	return true;
}

bool CScoreboardExt::SendTemplates(edict_t *pEdict, int idx, client_state_t &state)
{
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		if (!state.templatesPending[id])
			continue;

		for (int variant = 0; variant < bhl::sb::MAX_VARIANTS; variant++)
		{
			uint8_t bit = 1 << variant;

			if (!(state.templatesPending[id] & bit))
				continue;

			const std::string &text = GetText(idx, id, variant);

			// "" removes the variant, so it only goes to a client that has one
			if (!text.empty() || (state.templatesSent[id] & bit))
			{
				if (!BeginMessage(pEdict, bhl::sb::TEMPLATE_HEADER_SIZE + (int)text.size() + 1))
					return false;

				WRITE_BYTE((int)bhl::sb::E_Op::Template);
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
	if (!state.isHudStatePending)
		return true;

	unsigned int visible = GetVisibleElements(idx);

	if (visible != state.sentVisible || m_iHiddenStd != state.sentHidden)
	{
		if (!BeginMessage(pEdict, bhl::sb::HUDSTATE_SIZE))
			return false;

		WRITE_BYTE((int)bhl::sb::E_Op::HudState);
		WRITE_LONG((int)visible);
		WRITE_SHORT(m_iHiddenStd);
		MESSAGE_END();
		state.sentVisible = visible;
		state.sentHidden = m_iHiddenStd;
	}

	state.isHudStatePending = false;
	return true;
}

bool CScoreboardExt::SendEvents(edict_t *pEdict, int idx, client_state_t &state)
{
	event_queue_t &queue = m_Events[idx];

	while (queue.count > 0)
	{
		const event_t &event = queue.events[queue.first];
		int id = FindElement(event.key);

		// Dropped unsent once its line would be gone
		if (gpGlobals->time <= event.time + GetEventLife(event, id) * 0.001f)
		{
			// Waits for the element, keeping the order
			if (id == -1 || m_Elements[id].def.kind != (int)bhl::sb::E_ElementKind::Feed || !(state.elementsSent & (1u << id)))
				return true;

			if (!BeginMessage(pEdict, GetEventSize(event)))
				return false;

			WRITE_BYTE((int)bhl::sb::E_Op::Event);
			WRITE_BYTE(id);
			WRITE_BYTE(event.variant);
			WRITE_SHORT(event.lifeMs);
			WRITE_BYTE(event.argCount);

			for (int i = 0; i < event.argCount; i++)
				WriteValue(event.args[i]);

			MESSAGE_END();
		}

		queue.first = (queue.first + 1) % MAX_QUEUED_EVENTS;
		queue.count--;
	}

	return true;
}

bool CScoreboardExt::IsEqual(const value_t &a, const value_t &b)
{
	return a.type == b.type && a.value == b.value && a.max == b.max && a.flags == b.flags && !strcmp(a.text, b.text) && !strcmp(a.sprite, b.sprite);
}

bool CScoreboardExt::IsEqual(const column_t &a, const column_t &b)
{
	return !strcmp(a.key, b.key) && !strcmp(a.title, b.title) && a.display == b.display && a.flags == b.flags
	    && a.width == b.width && a.sort == b.sort && !strcmp(a.host, b.host);
}

bool CScoreboardExt::IsEqual(const element_def_t &a, const element_def_t &b)
{
	if (a.kind != b.kind || a.flags != b.flags || a.x != b.x || a.y != b.y || a.align != b.align
	    || a.place != b.place || strcmp(a.parent, b.parent) || a.gap != b.gap || a.isHudColor != b.isHudColor
	    || a.font != b.font || strcmp(a.select, b.select) || strcmp(a.value, b.value) || strcmp(a.list, b.list)
	    || a.paramsSet != b.paramsSet || a.slotsSet != b.slotsSet)
		return false;

	for (int i = 0; i < 4; i++)
	{
		if (a.color[i] != b.color[i])
			return false;
	}

	for (int i = 0; i <= bhl::sb::MAX_PARAM_ID; i++)
	{
		if ((a.paramsSet & bhl::sb::ParamBit(i)) && a.params[i] != b.params[i])
			return false;
	}

	for (int i = 0; i < bhl::sb::MAX_COLOR_SLOTS; i++)
	{
		if ((a.slotsSet & (1u << i)) && memcmp(a.slotColors[i], b.slotColors[i], sizeof(a.slotColors[i])))
			return false;
	}

	return true;
}

int CScoreboardExt::GetParamCount(const element_def_t &def)
{
	int count = 0;

	for (int i = 0; i <= bhl::sb::MAX_PARAM_ID; i++)
	{
		if (def.paramsSet & bhl::sb::ParamBit(i))
			count++;
	}

	return count;
}

int CScoreboardExt::GetEventSize(const event_t &event)
{
	int size = bhl::sb::EVENT_HEADER_SIZE;

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
		len = IsByteProgress(value.value, value.max) ? 2 : 4;
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
		WRITE_LONG(value.value);
		WRITE_BYTE(value.flags);
		break;
	}
}
