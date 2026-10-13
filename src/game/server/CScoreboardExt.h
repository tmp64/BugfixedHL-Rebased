#ifndef CSCOREBOARDEXT_H
#define CSCOREBOARDEXT_H

#include <cstdint>
#include <string>
#include <IScoreboardExt.h>
#include <ScoreboardProtocol.h>
#include "eiface.h"
#include "cdll_dll.h"

/**
 * Implementation of IScoreboardExt: keeps the scoreboard and HUD state and sends every client
 * its own view of it in "BhlScore" messages.
 * @see docs/scoreboard-protocol.md
 */
class CScoreboardExt : public bhl::IScoreboardExt
{
public:
	//! Whether a Config, an element or a hidden standard HUD part was set since the last reset (5.1)
	bool IsActive() { return m_bActive; }

	//! Clears everything without sending anything. Called on map change.
	void ClearAll();

	//! Clears cells, per-player texts and visibility, and the sync state of a player slot. Called when the slot is taken or freed.
	void ClearPlayer(int idx);

	/**
	 * Schedules Reset for the client, and everything else if the extension is active.
	 * @param	delay	Send nothing for FULL_SYNC_DELAY (InitHUD has just sent a lot); otherwise a running wait is kept
	 */
	void StartFullSync(int idx, bool delay);

	//! Sends pending data to the clients
	void StartFrame();

	//----------------------------------------------------------------------------------------------------
	// IScoreboardExt methods
	//----------------------------------------------------------------------------------------------------
	virtual void Reset();
	virtual void SetConfig(unsigned int hideFlags, const char *title, const char *tag);
	virtual bool AddColumn(const char *key, const char *title, int display, int width, int sort, unsigned int flags);
	virtual int GetTeamTarget(const char *teamName);
	virtual void SetInt(int target, const char *key, int value);
	virtual void SetText(int target, const char *key, const char *text);
	virtual void SetIcon(int target, const char *key, const char *sprite, const char *text);
	virtual void SetProgress(int target, const char *key, int cur, int max);
	virtual void Clear(int target, const char *key);
	virtual bool ListResize(const char *key, int count);
	virtual void ListSetInt(const char *key, int index, int value);
	virtual void ListSetText(const char *key, int index, const char *text);
	virtual void ListSetIcon(const char *key, int index, const char *sprite, const char *text);
	virtual void SetClock(int target, const char *key, int ms, unsigned int flags);
	virtual bool AddBadge(const char *key, const char *host);
	virtual bool HudAdd(const char *key, int kind, float x, float y, int font, int align, unsigned int flags);
	virtual bool HudSetPlace(const char *key, int place, const char *parent, int gap);
	virtual bool HudSetColor(const char *key, int r, int g, int b, int a);
	virtual bool HudBind(const char *key, const char *value, const char *list, const char *select);
	virtual bool HudSetParam(const char *key, int param, int value);
	virtual bool HudSetText(int player, const char *key, int variant, const char *text);
	virtual bool HudShow(int player, const char *key, bool show);
	virtual void HudRemove(const char *key);
	virtual void HudHideStd(unsigned int parts, bool hide);
	virtual bool HudSetSlotColor(const char *key, int slot, int r, int g, int b, int a);
	virtual bool HudEvent(int player, const char *key, int variant, int lifeMs, const int *args, int argCount, const char *iconSprite, const char *iconText);

private:
	static constexpr int TEAM_TARGET_BASE = 0x100; //!< GetTeamTarget returns this plus the team slot
	static constexpr int FIRST_TEAM = MAX_PLAYERS + 1; //!< Index of the first team in m_Cells
	static constexpr int MAX_TARGETS = FIRST_TEAM + MAX_TEAMS;
	static constexpr int BUDGET = 512; //!< Bytes per client per BUDGET_PERIOD
	static constexpr float BUDGET_PERIOD = 0.1f; //!< Seconds; about 5 KB/s per client, which the reliable channel drains
	static constexpr float FULL_SYNC_DELAY = 0.5f; //!< Seconds after InitHUD before the full sync starts
	static constexpr int MAX_MSG_CELLS = bhl::sb::MAX_MSG_SIZE / 3;
	static constexpr int MAX_QUEUED_EVENTS = 8; //!< Events per client; the oldest is dropped

	struct value_t
	{
		bhl::sb::E_ValueType type = bhl::sb::E_ValueType::None;
		int value = 0; //!< Int value, Progress cur or Clock ms
		int max = 0; //!< Progress max
		unsigned int flags = 0; //!< Clock flags
		char text[bhl::sb::MAX_TEXT_LEN + 1] = {}; //!< Text value or Icon text
		char sprite[bhl::sb::MAX_TEXT_LEN + 1] = {};
	};

	struct column_t
	{
		char key[bhl::sb::MAX_KEY_LEN + 1] = {};
		char title[bhl::sb::MAX_COLUMN_TITLE_LEN + 1] = {};
		int display = 0;
		unsigned int flags = 0;
		int width = 0;
		int sort = 0;
		char host[bhl::sb::MAX_KEY_LEN + 1] = {}; //!< Badge: key of the column it follows, resolved when sent
	};

	struct list_t
	{
		char key[bhl::sb::MAX_KEY_LEN + 1] = {};
		int count = 0;
		value_t items[bhl::sb::MAX_LIST_ITEMS];
	};

	//! Fields of the Element message, with the parent kept as a key
	struct element_def_t
	{
		int kind = (int)bhl::sb::E_ElementKind::None;
		unsigned int flags = 0; //!< E_ElementFlags without HudColor
		int x = 0; //!< Wire position
		int y = 0;
		int align = 0;
		int place = (int)bhl::sb::E_Place::Screen;
		char parent[bhl::sb::MAX_KEY_LEN + 1] = {}; //!< Only set for the parent places
		int gap = 0;
		bool isHudColor = true;
		int color[4] = { 0, 0, 0, 255 }; //!< r, g, b, a; rgb is 0 with isHudColor
		int font = (int)bhl::sb::E_Font::Text;
		char select[bhl::sb::MAX_REF_LEN + 1] = {};
		char value[bhl::sb::MAX_REF_LEN + 1] = {};
		char list[bhl::sb::MAX_KEY_LEN + 1] = {};
		bhl::sb::ParamMask paramsSet = 0; //!< Bit per param id: set by HudSetParam, sent
		int params[bhl::sb::MAX_PARAM_ID + 1] = {};
		unsigned int slotsSet = 0; //!< Bit per color slot: set by HudSetSlotColor, sent
		int slotColors[bhl::sb::MAX_COLOR_SLOTS][4] = {};
	};

	struct element_t
	{
		char key[bhl::sb::MAX_KEY_LEN + 1] = {}; //!< Empty if the id is free
		element_def_t def;
		bool isVisible = true; //!< Default visibility
		std::string texts[bhl::sb::MAX_VARIANTS]; //!< Default templates, "" if the variant has none
	};

	//! Per-player texts and visibility (5.4). Kept apart from client_state_t, which a full sync resets.
	struct player_hud_t
	{
		unsigned int visibleSet = 0; //!< Bit per element id: the player has his own visibility
		unsigned int visible = 0;
		uint8_t textsSet[bhl::sb::MAX_ELEMENTS] = {}; //!< Bit per variant: the player has his own text
		std::string texts[bhl::sb::MAX_ELEMENTS][bhl::sb::MAX_VARIANTS];
	};

	//! Kept apart from client_state_t, so that a full sync doesn't refill the budget
	struct pacing_t
	{
		float nextSendTime = 0; //!< Nothing is sent to the client before this time
		float periodEnd = 0; //!< End of the budget period
		int bytesLeft = 0; //!< Budget left in the period
	};

	//! Kept apart from client_state_t: a full sync neither clears nor replays events
	struct event_t
	{
		char key[bhl::sb::MAX_KEY_LEN + 1] = {}; //!< Element, looked up when sent
		int variant = 0;
		int lifeMs = 0;
		float time = 0; //!< When HudEvent was called
		int argCount = 0;
		value_t args[bhl::sb::MAX_EVENT_ARGS];
	};

	struct event_queue_t
	{
		event_t events[MAX_QUEUED_EVENTS]; //!< A ring, the oldest at first
		int first = 0;
		int count = 0;
	};

	struct client_state_t
	{
		bool isResetPending = false;
		bool isConfigPending = false;
		unsigned int columnsPending = 0; //!< Bit per column id
		unsigned int listsResized = 0; //!< Bit per list id
		uint64_t listItemsDirty[bhl::sb::MAX_LISTS] = {};
		unsigned int cellsDirty[MAX_TARGETS] = {}; //!< Bit per column id
		uint64_t playersCleared = 0; //!< Bit (slot - 1): all cells of the player must be cleared
		uint64_t teamsCleared = 0; //!< Bit (team number - 1): all cells of the team number must be cleared
		int teamNumbers[MAX_TEAMS] = {}; //!< Team number the cells of a team slot were sent with, 0 if none
		bool isRemoveAllPending = false;
		unsigned int elementsPending = 0; //!< Bit per element id: definition, or removal if the id is free
		unsigned int elementsSent = 0; //!< Bit per element id the client has
		unsigned int textsDropped = 0; //!< Bit per element id: texts dropped, a client that has the element gets a removal before the definition
		uint8_t sentParents[bhl::sb::MAX_ELEMENTS] = {}; //!< Parent id the client got with the element, for ids in elementsSent
		uint8_t templatesPending[bhl::sb::MAX_ELEMENTS] = {}; //!< Bit per variant
		uint8_t templatesSent[bhl::sb::MAX_ELEMENTS] = {}; //!< Bit per variant the client has a text for
		bool isHudStatePending = false;
		unsigned int sentVisible = 0; //!< HudState the client has
		unsigned int sentHidden = 0;
	};

	struct msg_cell_t
	{
		int wire; //!< Target as sent
		int target; //!< Index in m_Cells, -1 to clear all cells of the wire target
		int column;
	};

	bool m_bActive = false;
	bool m_bHasConfig = false;
	unsigned int m_iHideFlags = 0;
	char m_szTitle[bhl::sb::MAX_TITLE_LEN + 1] = {};
	char m_szTag[bhl::sb::MAX_TAG_LEN + 1] = {};

	column_t m_Columns[bhl::sb::MAX_COLUMNS];
	int m_iColumnCount = 0;
	list_t m_Lists[bhl::sb::MAX_LISTS];
	int m_iListCount = 0;
	char m_szTeamNames[MAX_TEAMS][MAX_TEAM_NAME] = {};
	int m_iTeamCount = 0;
	int m_TeamNumbers[MAX_TEAMS] = {}; //!< Team numbers of the team slots in the current frame
	value_t m_Cells[MAX_TARGETS][bhl::sb::MAX_COLUMNS];

	element_t m_Elements[bhl::sb::MAX_ELEMENTS];
	unsigned int m_iHiddenStd = 0; //!< E_HudStd
	player_hud_t m_PlayerHud[MAX_PLAYERS + 1];

	client_state_t m_Clients[MAX_PLAYERS + 1];
	pacing_t m_Pacing[MAX_PLAYERS + 1];
	event_queue_t m_Events[MAX_PLAYERS + 1];

	int m_iBytesLeft = 0;
	msg_cell_t m_MsgCells[MAX_MSG_CELLS];
	int m_iMsgCellCount = 0;
	int m_iMsgSize = 0;

	//! Clears the state of every plugin and turns the extension off; team targets stay valid
	void ClearState();
	void Activate();
	int GetCellTarget(int target);
	bool IsValidHudPlayer(int player);
	int FindColumn(const char *key);
	int FindList(const char *key);
	int FindElement(const char *key);
	int FindFreeElementId();
	bool SetColumn(const column_t &definition);
	void SetCell(int target, const char *key, const value_t &value);
	void ClearCell(int target, int column);
	void SetListItem(const char *key, int index, const value_t &value);

	//! Returns false and changes nothing if the Element message would be larger than MAX_MSG_SIZE
	bool SetElementDef(int id, const element_def_t &def);
	void RemoveElement(int id);
	void DropTexts(int id);
	const std::string &GetText(int player, int id, int variant);
	unsigned int GetVisibleElements(int player);

	//! Whether the client gets events: a scoreboard client with HudV1
	bool IsEventClient(int idx);
	void QueueEvent(int idx, const event_t &event);

	//! Milliseconds the client shows the line of an event of the element id (-1 if there is none)
	int GetEventLife(const event_t &event, int id);

	void MarkColumn(int id);
	void MarkBadges(const char *host);
	void MarkElement(int id);
	void MarkChildren(const char *parent);
	void MarkHudState();

	//! Returns false if the budget ran out before everything was sent
	bool SendToClient(int idx);
	void UpdateClientTeams(client_state_t &state);
	bool BeginMessage(edict_t *pEdict, int size);
	bool SendHeader(edict_t *pEdict, client_state_t &state);
	bool SendColumn(edict_t *pEdict, int id);
	bool SendLists(edict_t *pEdict, client_state_t &state);
	bool SendList(edict_t *pEdict, client_state_t &state, int id);
	bool SendValues(edict_t *pEdict, client_state_t &state);
	bool AddValuesCell(edict_t *pEdict, client_state_t &state, int wire, int target, int column);
	bool FlushValues(edict_t *pEdict, client_state_t &state);
	bool SendElements(edict_t *pEdict, client_state_t &state);
	static int GetElementSize(const element_def_t &def);
	bool SendElement(edict_t *pEdict, client_state_t &state, int id);
	bool SendElementRemoval(edict_t *pEdict, int id);
	bool SendTemplates(edict_t *pEdict, int idx, client_state_t &state);
	bool SendHudState(edict_t *pEdict, int idx, client_state_t &state);
	bool SendEvents(edict_t *pEdict, int idx, client_state_t &state);

	static bool IsEqual(const value_t &a, const value_t &b);
	static bool IsEqual(const column_t &a, const column_t &b);
	static bool IsEqual(const element_def_t &a, const element_def_t &b);
	static int GetParamCount(const element_def_t &def);
	static int GetEventSize(const event_t &event);
	static int GetValueSize(const value_t &value);
	static void WriteValue(const value_t &value);
};

#endif
