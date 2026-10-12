#ifndef CSCOREBOARDEXT_H
#define CSCOREBOARDEXT_H

#include <cstdint>
#include <IScoreboardExt.h>
#include <ScoreboardProtocol.h>
#include "eiface.h"
#include "cdll_dll.h"

/**
 * Implementation of IScoreboardExt: keeps the scoreboard state and sends it
 * to the clients in "BhlScore" messages.
 * @see docs/scoreboard-protocol.md
 */
class CScoreboardExt : public bhl::IScoreboardExt
{
public:
	bool IsActive() { return m_bActive; }

	//! Clears everything without sending anything. Called on map change.
	void ClearAll();

	//! Clears cells and the sync state of a player slot. Called when the slot is taken or freed.
	void ClearPlayer(int idx);

	/**
	 * Schedules Reset for the client, and everything else if the schema is active.
	 * @param	delay	Send nothing for FULL_SYNC_DELAY (InitHUD has just sent a lot)
	 */
	void StartFullSync(int idx, bool delay);

	//! Sends pending data to the clients
	void StartFrame();

	//----------------------------------------------------------------------------------------------------
	// IScoreboardExt methods
	//----------------------------------------------------------------------------------------------------
	virtual void Reset();
	virtual void SetConfig(unsigned int hideFlags, const char *preset, const char *title, const char *tag);
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

private:
	static constexpr int TEAM_TARGET_BASE = 0x100; //!< GetTeamTarget returns this plus the team slot
	static constexpr int FIRST_TEAM = MAX_PLAYERS + 1; //!< Index of the first team in m_Cells
	static constexpr int MAX_TARGETS = FIRST_TEAM + MAX_TEAMS;
	static constexpr int FRAME_BUDGET = 512; //!< Bytes per client per frame
	static constexpr float FULL_SYNC_DELAY = 0.5f; //!< Seconds after InitHUD before the full sync starts
	static constexpr float BUDGET_DELAY = 0.1f; //!< Seconds to wait after the budget ran out, lets the reliable channel drain
	static constexpr int MAX_MSG_CELLS = bhl::sb::MAX_MSG_SIZE / 3;

	struct value_t
	{
		bhl::sb::E_ValueType type = bhl::sb::E_ValueType::None;
		int value = 0; //!< Int value or Progress cur
		int max = 0; //!< Progress max
		char text[bhl::sb::MAX_TEXT_LEN + 1] = {}; //!< Text value or Icon text
		char sprite[bhl::sb::MAX_TEXT_LEN + 1] = {};
	};

	struct column_t
	{
		char key[bhl::sb::MAX_KEY_LEN + 1] = {};
		char title[bhl::sb::MAX_COLUMN_TITLE_LEN + 1] = {};
		int display = 0;
		int flags = 0;
		int width = 0;
		int sort = 0;
	};

	struct list_t
	{
		char key[bhl::sb::MAX_KEY_LEN + 1] = {};
		int count = 0;
		value_t items[bhl::sb::MAX_LIST_ITEMS];
	};

	struct client_state_t
	{
		float nextSendTime = 0; //!< Nothing is sent to the client before this time
		bool isResetPending = false;
		bool isConfigPending = false;
		unsigned int columnsPending = 0; //!< Bit per column id
		unsigned int listsResized = 0; //!< Bit per list id
		uint64_t listItemsDirty[bhl::sb::MAX_LISTS] = {};
		unsigned int cellsDirty[MAX_TARGETS] = {}; //!< Bit per column id
		uint64_t playersCleared = 0; //!< Bit (slot - 1): all cells of the player must be cleared
		uint64_t teamsCleared = 0; //!< Bit (team number - 1): all cells of the team number must be cleared
		int teamNumbers[MAX_TEAMS] = {}; //!< Team number the cells of a team slot were sent with, 0 if none
	};

	struct msg_cell_t
	{
		int wire; //!< Target as sent
		int target; //!< Index in m_Cells, -1 to clear all cells of the wire target
		int column;
	};

	bool m_bActive = false;
	int m_iHideFlags = 0;
	char m_szPreset[bhl::sb::MAX_PRESET_LEN + 1] = {};
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

	client_state_t m_Clients[MAX_PLAYERS + 1];

	int m_iBytesLeft = 0;
	msg_cell_t m_MsgCells[MAX_MSG_CELLS];
	int m_iMsgCellCount = 0;
	int m_iMsgSize = 0;

	void ClearSchema();
	int GetCellTarget(int target);
	int FindColumn(const char *key);
	int FindList(const char *key);
	void SetCell(int target, const char *key, const value_t &value);
	void ClearCell(int target, int column);
	void SetListItem(const char *key, int index, const value_t &value);

	//! Returns false if the budget ran out before everything was sent
	bool SendToClient(int idx);
	void UpdateClientTeams(client_state_t &state);
	bool BeginMessage(edict_t *pEdict, int size);
	bool SendColumn(edict_t *pEdict, int id);
	bool SendList(edict_t *pEdict, client_state_t &state, int id);
	bool SendValues(edict_t *pEdict, client_state_t &state);
	bool AddValuesCell(edict_t *pEdict, client_state_t &state, int wire, int target, int column);
	bool FlushValues(edict_t *pEdict, client_state_t &state);

	static bool IsEqual(const value_t &a, const value_t &b);
	static int GetValueSize(const value_t &value);
	static void WriteValue(const value_t &value);
};

#endif
