#ifndef SCOREBOARD_DATA_H
#define SCOREBOARD_DATA_H
#include <ScoreboardProtocol.h>
#include "cdll_dll.h"

/**
 * Client store of the scoreboard extension (docs/scoreboard-protocol.md).
 * Receives "BhlScore" messages. Read by the scoreboard, hud_scores and the GunGame HUD.
 */
class CScoreboardData
{
public:
	struct Value
	{
		bhl::sb::E_ValueType type = bhl::sb::E_ValueType::None;
		int iValue = 0; //!< Int value or Progress cur
		int iMax = 0; //!< Progress max
		char szText[bhl::sb::MAX_TEXT_LEN + 1] = {}; //!< Text value or Icon text
		char szSprite[bhl::sb::MAX_TEXT_LEN + 1] = {}; //!< Icon sprite name from hud.txt

		bool IsEmpty() const;

		//! Sort key: Int value or Progress cur. Returns false for other types.
		bool GetNumber(int &out) const;

		//! Text form: "5", text, icon name (see GetIconName), "cur/max". Empty for None.
		void ToString(wchar_t *buf, int bufSizeBytes) const;
	};

	struct Column
	{
		bool bValid = false;
		bhl::sb::E_Display display = bhl::sb::E_Display::Text;
		bhl::sb::E_ColumnFlags flags = bhl::sb::E_ColumnFlags::None;
		int iWidth = 0; //!< Proportional units, 0 for the default
		int iSort = 0; //!< Raw sort byte
		char szKey[bhl::sb::MAX_KEY_LEN + 1] = {};
		char szTitle[bhl::sb::MAX_COLUMN_TITLE_LEN + 1] = {};

		int GetSortPriority() const { return iSort & bhl::sb::SORT_PRIORITY_MASK; }
		bool IsSortAscending() const { return (iSort & bhl::sb::SORT_ASC) != 0; }
		bool IsHidden() const { return IsEnumFlagSet(flags, bhl::sb::E_ColumnFlags::Hidden); }
		bool IsPrimary() const { return IsEnumFlagSet(flags, bhl::sb::E_ColumnFlags::Primary); }

		//! Returns 0 (left), 1 (center) or 2 (right)
		int GetAlign() const;
	};

	struct List
	{
		bool bValid = false;
		char szKey[bhl::sb::MAX_KEY_LEN + 1] = {};
		int iCount = 0;
		Value items[bhl::sb::MAX_LIST_ITEMS];
	};

	static CScoreboardData &Get();

	CScoreboardData();
	CScoreboardData(const CScoreboardData &) = delete;
	CScoreboardData &operator=(const CScoreboardData &) = delete;

	//! Hooks BhlScore. Called from CHud::Init.
	void Init();

	//! Clears everything. Called from CHud::VidInit.
	void VidInit();

	//! Clears cells of disconnected players. Called from CHud::Frame.
	void Frame();

	//! True when hud_scoreboard_custom allows the custom scoreboard (setting only, ignores the server)
	static bool IsCustomEnabled();

	//! True when hud_gungame allows the GunGame HUD (also requires IsCustomEnabled)
	static bool IsGunGameHudEnabled();

	//! True when the server sent a config after the last reset and IsCustomEnabled()
	bool IsActive() const;

	//! True when IsActive() and the config uses the "gungame" preset
	bool IsGunGame() const;

	bool IsStdColumnHidden(bhl::sb::E_ConfigFlags flag) const;
	const char *GetPreset() const { return m_szPreset; }
	const char *GetTitle() const { return m_szTitle; }
	const char *GetTag() const { return m_szTag; }

	//! Incremented on every change of the store
	int GetSerial() const { return m_iSerial; }

	//! Incremented on every change of the config or columns
	int GetSchemaSerial() const { return m_iSchemaSerial; }

	//! @param	id	Column id [0; MAX_COLUMNS)
	const Column &GetColumn(int id) const;

	//! Returns column id or -1
	int FindColumn(const char *key) const;

	//! Returns id of the Primary column or -1
	int FindPrimaryColumn() const;

	//! Returns id of the column with the lowest non-zero sort priority or -1
	int GetFirstSortColumn() const;

	//! Values of absent or disconnected targets are empty
	const Value &GetGlobalValue(int col) const;
	const Value &GetPlayerValue(int client, int col) const;
	const Value &GetTeamValue(int team, int col) const;

	//! Returns nullptr if there is no such list
	const List *FindList(const char *key) const;

	/**
	 * Compares players by the sort keys of the schema.
	 * @return	< 0 if a goes before b, > 0 if after, 0 if the keys are equal
	 */
	int ComparePlayers(int a, int b) const;
	int CompareTeams(int a, int b) const;

	/**
	 * Place by the first sort key among participants: connected players that are not spectators.
	 * Equal values share a place (1, 1, 3).
	 * @param	pTotal	Receives the number of participants
	 * @return	Place starting at 1, or 0 if the player has no value or there is no sort key
	 */
	int GetPlayerPlace(int client, int *pTotal = nullptr) const;

	/**
	 * Place by the first sort key among teams that have participants.
	 */
	int GetTeamPlace(int team, int *pTotal = nullptr) const;

	/**
	 * Returns "One", "Few" or "Many": the plural form of n for the client language
	 * (token BHL_PluralRule in bugfixedhl_*.txt).
	 */
	static const char *GetPluralSuffix(int n);

	/**
	 * Converts a UTF-8 string of the protocol to wide.
	 * "#token" is localized, a missing token is shown without '#'.
	 */
	static void LocalizeString(const char *str, wchar_t *buf, int bufSizeBytes);

	/**
	 * Display name of an Icon value: BHL_Weapon_<sprite without "d_"> for standard HL sprites,
	 * the text of the value otherwise.
	 */
	static void GetIconName(const Value &value, wchar_t *buf, int bufSizeBytes);

	//! hud_scoreboard_dump: prints the store to the console
	void DumpCommand();

	//! hud_scoreboard_test [preset|off] [phase] [phase_left]: fills the store with a fake GunGame schema
	void TestCommand();

private:
	bool m_bConfigReceived = false;
	bhl::sb::E_ConfigFlags m_ConfigFlags = bhl::sb::E_ConfigFlags::None;
	char m_szPreset[bhl::sb::MAX_PRESET_LEN + 1] = {};
	char m_szTitle[bhl::sb::MAX_TITLE_LEN + 1] = {};
	char m_szTag[bhl::sb::MAX_TAG_LEN + 1] = {};
	int m_iSerial = 0;
	int m_iSchemaSerial = 0;

	Column m_Columns[bhl::sb::MAX_COLUMNS];
	Value m_GlobalValues[bhl::sb::MAX_COLUMNS];
	Value m_PlayerValues[MAX_PLAYERS + 1][bhl::sb::MAX_COLUMNS];
	Value m_TeamValues[MAX_TEAMS + 1][bhl::sb::MAX_COLUMNS];
	List m_Lists[bhl::sb::MAX_LISTS];

	int m_SortColumns[bhl::sb::MAX_COLUMNS] = {}; //!< Ids of the sort key columns in priority order
	int m_iSortColumnCount = 0;
	bool m_bPlayerConnected[MAX_PLAYERS + 1] = {}; //!< Connection state seen by the last Frame

	void Clear();
	void UpdateSortColumns();

	//! Returns MAX_COLUMNS values of the protocol target or nullptr
	Value *GetTargetValues(int target);

	int MsgFunc_BhlScore(const char *pszName, int iSize, void *pbuf);
};

#endif
