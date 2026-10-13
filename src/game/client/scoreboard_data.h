#ifndef SCOREBOARD_DATA_H
#define SCOREBOARD_DATA_H
#include <ClientSupportsFlags.h>
#include <ScoreboardProtocol.h>
#include "cdll_dll.h"
#include "scoreboard_template.h"

/**
 * Client store of the scoreboard and HUD extension (docs/scoreboard-protocol.md, 6.1).
 * Receives "BhlScore" messages and keeps the schema, cells, lists, elements, templates and the HUD state.
 * Computes the order, places, team mode, subjects and built-in values, and evaluates templates.
 * Read by the scoreboard, hud_scores, HudServerPanel and the standard HUD parts.
 *
 * Text buffers of this class are sized in bytes (bufSizeBytes), like Q_UTF8ToWString.
 */
class CScoreboardData
{
public:
	struct Value
	{
		bhl::sb::E_ValueType type = bhl::sb::E_ValueType::None;
		int iValue = 0; //!< Int value, Progress cur, Clock ms when it arrived
		int iMax = 0; //!< Progress max
		bhl::sb::E_ClockFlags clockFlags = bhl::sb::E_ClockFlags::None;
		float flClockTime = 0; //!< GetTime() when a Clock arrived
		char szText[bhl::sb::MAX_TEXT_LEN + 1] = {}; //!< Text value or Icon text
		char szSprite[bhl::sb::MAX_TEXT_LEN + 1] = {}; //!< Icon sprite name from hud.txt

		bool IsEmpty() const;

		bool IsRunningClock() const;

		//! Current value of a Clock in ms (4.3): runs from flClockTime, a countdown stops at 0
		int GetClockMs() const;

		//! Number of the value (4.3): Int, Progress cur or Clock seconds. False for other types.
		bool GetNumber(int &out) const;

		/**
		 * Text of the value (8.10, "Values are written as"): the number, the localized text,
		 * the icon name (GetIconName), "cur/max" or "cur" if max is 0, a Clock as m:ss. "" for None.
		 */
		void ToString(wchar_t *buf, int bufSizeBytes) const;
	};

	struct Column
	{
		bool bValid = false;
		bhl::sb::E_Display display = bhl::sb::E_Display::Text; //!< Unknown displays are Text
		bhl::sb::E_ColumnFlags flags = bhl::sb::E_ColumnFlags::None;
		int iWidth = 0; //!< Proportional units, 0 for the default
		int iSort = 0; //!< Raw sort byte
		int iHost = -1; //!< Badge: id of the column the badge follows, -1 if none
		char szKey[bhl::sb::MAX_KEY_LEN + 1] = {};
		char szTitle[bhl::sb::MAX_COLUMN_TITLE_LEN + 1] = {};

		int GetSortPriority() const { return iSort & bhl::sb::SORT_PRIORITY_MASK; }
		bool IsSortAscending() const { return (iSort & bhl::sb::SORT_ASC) != 0; }
		bool IsHidden() const { return IsEnumFlagSet(flags, bhl::sb::E_ColumnFlags::Hidden); }
		bool IsPrimary() const { return IsEnumFlagSet(flags, bhl::sb::E_ColumnFlags::Primary); }
		bool IsMarkLast() const { return IsEnumFlagSet(flags, bhl::sb::E_ColumnFlags::MarkLast); }
		bool IsBuiltin() const { return display == bhl::sb::E_Display::Builtin; }
		bool IsBadge() const { return display == bhl::sb::E_Display::Badge; }

		//! Returns 0 (left), 1 (center) or 2 (right); 3 is left
		int GetAlign() const;
	};

	struct List
	{
		bool bValid = false;
		char szKey[bhl::sb::MAX_KEY_LEN + 1] = {};
		int iCount = 0;
		Value items[bhl::sb::MAX_LIST_ITEMS];
	};

	//! Definition of a HUD element (4.3). Templates are kept apart: an Element never changes them.
	struct Element
	{
		bool bValid = false;
		bhl::sb::E_ElementKind kind = bhl::sb::E_ElementKind::None;
		bhl::sb::E_ElementFlags flags = bhl::sb::E_ElementFlags::None;
		int x = 0; //!< 1/8192 of the screen width, bhl::sb::POS_CENTER is the center
		int y = 0; //!< 1/8192 of the screen height
		bhl::sb::E_HAlign hAlign = bhl::sb::E_HAlign::Auto;
		bhl::sb::E_VAlign vAlign = bhl::sb::E_VAlign::Auto;
		bhl::sb::E_Place place = bhl::sb::E_Place::Screen; //!< Unknown places are Screen
		int iParent = -1; //!< Element id for the parent places, -1 if none
		int iGap = 0; //!< Proportional units
		int r = 255, g = 255, b = 255, a = 255;
		bhl::sb::E_Font font = bhl::sb::E_Font::Text; //!< Unknown fonts are Text
		char szSelect[bhl::sb::MAX_REF_LEN + 1] = {};
		char szValue[bhl::sb::MAX_REF_LEN + 1] = {};
		char szList[bhl::sb::MAX_KEY_LEN + 1] = {};
		bhl::sb::TemplateRef selectRef; //!< Parsed szSelect, not set if it is ""
		bhl::sb::TemplateRef valueRef; //!< Parsed szValue, not set if it is ""
		bhl::sb::ParamMask iParamMask = 0; //!< Bit per param id that the server sent
		int params[bhl::sb::MAX_PARAM_ID + 1] = {}; //!< Clamped values by param id
		unsigned int iColorMask = 0; //!< Bit per color slot that the server sent
		unsigned char colors[bhl::sb::MAX_COLOR_SLOTS][4] = {}; //!< rgba by slot

		bool HasFlag(bhl::sb::E_ElementFlags flag) const { return IsEnumFlagSet(flags, flag); }

		/**
		 * Returns true and the value if the server sent the param.
		 * Otherwise HudServerPanel uses its default from HudLayoutBase.res.
		 */
		bool GetParam(bhl::sb::E_Param param, int &value) const;

		//! Returns true and the rgba if the server sent the color slot (4.3, 8.5)
		bool GetSlotColor(int slot, unsigned char out[4]) const;
	};

	/**
	 * Whose data an element shows (8.2): a player, a team (the first row in team mode) or nobody.
	 * Also used for the ranked entities of 6.4: a participant or a team.
	 */
	struct Subject
	{
		int iPlayer = 0; //!< Player index, 0 for a team or nobody
		int iTeam = 0; //!< Team number of a team subject, 0 otherwise

		bool IsValid() const { return iPlayer != 0 || iTeam != 0; }
		bool IsTeam() const { return iTeam != 0; }
		bool operator==(const Subject &other) const { return iPlayer == other.iPlayer && iTeam == other.iTeam; }
		bool operator!=(const Subject &other) const { return !(*this == other); }
	};

	/**
	 * Built-in values of a subject (8.11), computed once per frame.
	 * A value is missing when its flag is false.
	 */
	struct Builtins
	{
		bool bHasSubject = false;
		bool bRanked = false; //!< The ranked entity of the subject has a number: @place, @state, @behind
		int iPlace = 0;
		int iTotal = 0; //!< Never missing
		int iState = 0; //!< 0 no rivals, 1 a rival ahead, 2 tied for the first place, 3 leading
		Subject rival; //!< Nearest rival ahead when iState is 1: @rival, @gap, @pass
		int iGap = 0; //!< @pass is iGap + 1
		int iLead = 0; //!< Valid when iState is 2 or 3
		int iBehind = 0;
		bool bHasScore = false;
		int iScore = 0;
		int iFrags = 0; //!< @frags and @deaths: valid with bHasSubject
		int iDeaths = 0;
	};

	//! Values of the built-ins that only some templates have (8.10): Board rows and events
	struct TemplateExtras
	{
		const Value *pItem = nullptr; //!< @item
		int iIndex = 0; //!< @index when > 0
		const Value *pValue = nullptr; //!< Replaces the value that @value reads, with the format of the element's value ref applied (FormatValue)
		const Value *pArgs = nullptr; //!< @a-@d by position, @icon is the first Icon
		int iArgCount = 0;
	};

	//! Feed lines kept over all elements
	static constexpr int MAX_HUD_LINES = 32;

	//! A line of a Feed element from an Event message (4.3)
	struct HudLine
	{
		int iElement = -1; //!< -1 for an unused slot
		int iSeq = 0; //!< Grows with every line over all elements and is never reset: the announcer schedule of HudServerPanel
		int iVariant = 0;
		float flTime = 0; //!< GetTime() when it arrived
		float flLife = 0; //!< Seconds, 0 for the element's life_ms
		Value icon; //!< The first Icon arg, empty without one
		wchar_t wszText[bhl::sb::TEMPLATE_BUF_LEN] = {}; //!< The template evaluated on arrival
	};

	static CScoreboardData &Get();

	CScoreboardData();
	CScoreboardData(const CScoreboardData &) = delete;
	CScoreboardData &operator=(const CScoreboardData &) = delete;

	//! Hooks BhlScore. Called from CHud::Init.
	void Init();

	//! Clears everything and ends a wait for Reset (SetClientCaps). Called from CHud::VidInit.
	void VidInit();

	//! Clears cells of players that left, starts a new frame for the caches. Called from CHud::Frame.
	void Frame();

	//----------------------------------------------------------------------------------------------------
	// Client settings and caps (3.2)
	//----------------------------------------------------------------------------------------------------

	//! hud_scoreboard_custom: show the custom scoreboard when the server sends one
	static bool IsCustomEnabled();

	//! hud_server_elements: draw HUD elements and honour hidden standard parts
	static bool IsHudEnabled();

	/**
	 * Called by CHud::UpdateCapsUserinfo with the caps it writes to _bhl.
	 * When ScoreboardV1 is cleared the store is cleared as on Reset, and messages are dropped
	 * while it stays cleared. Before the first call the store accepts messages.
	 *
	 * After ScoreboardV1 comes back, every message except Reset is dropped until a Reset with
	 * version >= 1 arrives or VidInit runs. A full sync always starts with Reset, and a server
	 * that missed the toggle (a Query source re-reads _bhl only every 10 s) must not fill the empty
	 * store with plain deltas.
	 */
	void SetClientCaps(bhl::E_ClientCaps caps);

	//! Client time in seconds that Clock values run on
	static float GetTime();

	//----------------------------------------------------------------------------------------------------
	// Schema (4.3, 6.2)
	//----------------------------------------------------------------------------------------------------

	//! True when a Config arrived after the last Reset or VidInit
	bool HasConfig() const { return m_bConfigReceived; }

	//! True when HasConfig() and IsCustomEnabled(): the custom scoreboard is shown
	bool IsActive() const;

	//! True when IsActive() and the config hides the standard column
	bool IsStdColumnHidden(bhl::sb::E_ConfigFlags flag) const;

	const char *GetTitle() const { return m_szTitle; }
	const char *GetTag() const { return m_szTag; }

	//! @param	id	Column id [0; MAX_COLUMNS); other ids give an invalid column
	const Column &GetColumn(int id) const;

	//! Returns column id or -1
	int FindColumn(const char *key) const;

	//! Returns id of the first Primary column (lowest id) that is not Hidden, Builtin or Badge, or -1 (6.5)
	int FindPrimaryColumn() const;

	/**
	 * Badge columns drawn after the cells of host, in id order (6.7). Leaves out Hidden badges.
	 * Returns 0 if host is missing, hidden, Builtin or a Badge column.
	 * @return	Number of ids written
	 */
	int GetBadgeColumns(int host, int *ids, int maxIds) const;

	//! Sort key columns in priority order (ties by id): priority > 0, not Builtin or Badge, Hidden included
	int GetSortColumnCount() const { return m_iSortColumnCount; }
	int GetSortColumn(int i) const;

	//! Returns id of the first sort key column or -1
	int GetFirstSortColumn() const;

	//----------------------------------------------------------------------------------------------------
	// Cells and lists
	//----------------------------------------------------------------------------------------------------

	//! Values of absent or disconnected targets are empty
	const Value &GetGlobalValue(int col) const;
	const Value &GetPlayerValue(int client, int col) const;
	const Value &GetTeamValue(int team, int col) const;

	//! Returns nullptr if there is no such list
	const List *FindList(const char *key) const;

	//! True if a cell or list item holds a running Clock
	bool HasRunningClock() const;

	//----------------------------------------------------------------------------------------------------
	// Order and places (6.4)
	//----------------------------------------------------------------------------------------------------

	//! Connected and not a spectator; bots count
	static bool IsParticipant(int client);

	/**
	 * Full order of players: the sort keys, then frags descending, deaths ascending, then the index.
	 * Empty cells go after filled ones.
	 * @return	< 0 if a goes before b, > 0 if after; 0 only for a == b
	 */
	int ComparePlayers(int a, int b) const;

	//! Full order of teams by team cells, then team frags descending, deaths ascending, then the number
	int CompareTeams(int a, int b) const;

	//! Team frags and deaths as the scoreboard shows them: TeamScore if it overrides them, else the sums of the players
	void GetTeamScore(int team, int &frags, int &deaths) const;

	/**
	 * Place in the rank column among participants: by the first sort key only, equal values share
	 * a place (1, 1, 3); without sort keys by frags. 0 if the player is not a participant or his
	 * first-key cell is empty.
	 */
	int GetPlayerPlace(int client) const;

	//! Place in the rank column among teams with participants, the same way
	int GetTeamPlace(int team) const;

	//! True if the ranked entities are the teams with participants (6.4)
	bool IsTeamMode() const;

	//! First row of the order: a team in team mode, otherwise a participant; invalid without participants
	Subject GetFirstRow() const;

	/**
	 * The ranked entities (6.4) in the scoreboard order: teams with participants in team mode, otherwise participants.
	 * @return	Number of entities written, at most max
	 */
	int GetOrderedEntities(Subject *out, int max) const;

	//! The ranked entity of a subject: his team in team mode, otherwise the subject itself
	Subject GetRankedEntity(const Subject &subject) const;

	//! Built-in values of a subject, computed once per frame for each subject (8.11)
	Builtins GetBuiltins(const Subject &subject) const;

	//----------------------------------------------------------------------------------------------------
	// HUD elements (8)
	//----------------------------------------------------------------------------------------------------

	//! @param	id	Element id [0; MAX_ELEMENTS); other ids give an invalid element
	const Element &GetElement(int id) const;

	//! Raw HudState.visible: bit per element id
	unsigned int GetVisibleMask() const { return m_iVisibleMask; }

	//! True if the element is defined and its bit is set in HudState.visible
	bool IsElementVisible(int id) const;

	//! Raw HudState.hide: bhl::sb::E_HudStd
	unsigned int GetHiddenStdParts() const { return m_iHiddenStdParts; }

	//! True if the server hides the standard HUD part and IsHudEnabled() (8.12)
	bool IsStdPartHidden(bhl::sb::E_HudStd part) const;

	//! Subject of the viewer (8.2): the local player, the observed player or nobody
	Subject GetViewerSubject() const;

	/**
	 * Subject of an element: the first row with Leader, otherwise the viewer's subject.
	 * @return	false if the element must not be drawn: Subject without a subject, Leader without
	 *			participants (8.3 rule 4, 6.8)
	 */
	bool GetElementSubject(int id, Subject &out) const;

	/**
	 * Resolves a ref for a subject (8.10) as a template shows it: the ITemplateContext::GetValue of
	 * the store. A cell of the player, then of his team, then global (a team subject starts with its
	 * team cells, nobody reads global cells only), or a built-in value; with bFirstRow for the first row.
	 * cur, max and left make an Int of a Progress. Built-in numbers come as Int.
	 *
	 * "@value" is the value ref of element elementId (-1 for none), resolved at most one level:
	 * its own format applies first, then ref.format. When that value ref is itself "@value", with or
	 * without '^' or a format, the value is missing, so a self-reference can't recurse.
	 *
	 * Text cells come localized by the token rule (4.2). Names (@name, @rival) are never looked up
	 * by that rule: they come as ITemplateContext::GetPlayerName and GetTeamName write them.
	 * @return	false if the value is missing (out.type is then Missing)
	 */
	bool ResolveRef(const bhl::sb::TemplateRef &ref, const Subject &subject, int elementId, bhl::sb::TemplateValue &out) const;

	/**
	 * Value of the element's value ref for drawing a Meter, an Icon or a Strip.
	 * Returns the stored value unchanged; only the ref's own cur/max/left format is applied, which makes an Int.
	 * A Meter needs a Progress, an Icon needs an Icon. A Strip takes its position with Value::GetNumber
	 * and, without a list or with a list that doesn't exist, its count from Progress iMax.
	 * Built-in numbers come as Int. @name, @rival and a value ref that is itself "@value" are missing.
	 * @return	false if the value is missing
	 */
	bool GetElementValue(int id, const Subject &subject, Value &out) const;

	/**
	 * Applies the cur, max or left format of a ref to a value as GetElementValue does: a Progress becomes an Int,
	 * any other value becomes empty. None leaves the value as it is.
	 * @return	false if the format emptied the value
	 */
	static bool FormatValue(bhl::sb::E_RefFormat format, Value &value);

	/**
	 * Variant of an element (8.10, Variants): Text by the number of select as ResolveRef gives it
	 * ("" is 0), Icon and Meter 0, Strip none.
	 * @return	0-7, or -1 if the select value is missing or out of range, or the variant has no template
	 */
	int GetElementVariant(int id, const Subject &subject) const;

	/**
	 * Text of an element for a subject: Text its chosen variant, Icon variant 0, Meter its counter
	 * (variant 0, or "{@value:cur} / {@value:max}" when variant 0 has no template); Strip none.
	 * @return	false if the text is empty (buf is "")
	 */
	bool GetElementText(int id, const Subject &subject, wchar_t *buf, int bufSizeBytes) const;

	//! Template of a variant, nullptr if the variant has no text
	const bhl::sb::CTemplate *GetTemplate(int id, int variant) const;

	/**
	 * Evaluates any template for a subject in the context of element elementId (-1 for none).
	 * Used by hud_server_eval.
	 * @param	pMissing	See bhl::sb::CTemplate::Evaluate
	 * @param	pExtras		@item, @index, @a-@d, @icon and the value of @value of a Board row or an event; refs with '^' resolve without them
	 */
	bool EvaluateTemplate(const bhl::sb::CTemplate &tpl, const Subject &subject, int elementId, wchar_t *buf, int bufSizeBytes, unsigned int *pMissing = nullptr, const TemplateExtras *pExtras = nullptr) const;

	/**
	 * Lines of a Feed element that are kept, oldest first, up to MAX_HUD_LINES.
	 * Lines don't expire here: HudServerPanel drops them by their life. The next message may replace them.
	 * @return	Number of lines written
	 */
	int GetHudLines(int elementId, const HudLine **out) const;

	/**
	 * Banner row of the scoreboard (6.8): the lowest-id Text element with this place that is
	 * visible in HudState, passes the Subject and Leader rules and has a non-empty text.
	 * Ignores hud_server_elements and the other rules of 8.3.
	 * @param	place	BannerTop or BannerBottom
	 * @return	Element id or -1; buf receives its text
	 */
	int GetBannerRow(bhl::sb::E_Place place, wchar_t *buf, int bufSizeBytes) const;

	//----------------------------------------------------------------------------------------------------
	// Change counters for redraws
	//----------------------------------------------------------------------------------------------------

	//! Incremented on every change of the store
	int GetSerial() const { return m_iSerial; }

	//! Incremented on every change of the config or columns
	int GetSchemaSerial() const { return m_iSchemaSerial; }

	//! Incremented on every change of elements, templates and HudState
	int GetHudSerial() const { return m_iHudSerial; }

	//! Incremented when the store is cleared: Reset (a full sync starts with it), VidInit and the loss of ScoreboardV1
	int GetResetCount() const { return m_iResetCount; }

	//! Incremented on every removal of the element id, also by Clear
	int GetElementRemovals(int id) const { return m_iElementRemovals[id]; }

	//----------------------------------------------------------------------------------------------------
	// Text helpers
	//----------------------------------------------------------------------------------------------------

	/**
	 * Converts a UTF-8 string of the protocol to wide (4.2): a string that is exactly a client
	 * token is localized, a missing token is shown without '#'. Other strings, "#1" too, stay as they are.
	 */
	static void LocalizeString(const char *str, wchar_t *buf, int bufSizeBytes);

	/**
	 * Display name of an Icon value (10.3): BHL_Weapon_<sprite without "d_"> for standard HL sprites,
	 * the localized text of the value otherwise.
	 */
	static void GetIconName(const Value &value, wchar_t *buf, int bufSizeBytes);

	//----------------------------------------------------------------------------------------------------
	// Debug commands (6.9)
	//----------------------------------------------------------------------------------------------------

	//! hud_scoreboard_dump
	void DumpCommand();

	//! hud_scoreboard_test [off]
	void TestCommand();

	//! hud_server_eval "<template>"
	void EvalCommand();

private:
	bool m_bConfigReceived = false;
	bool m_bAcceptMessages = true; //!< False while the client caps have no ScoreboardV1
	bool m_bWaitForReset = false; //!< ScoreboardV1 came back: only Reset is accepted until one arrives
	bhl::sb::E_ConfigFlags m_ConfigFlags = bhl::sb::E_ConfigFlags::None;
	char m_szTitle[bhl::sb::MAX_TITLE_LEN + 1] = {};
	char m_szTag[bhl::sb::MAX_TAG_LEN + 1] = {};
	int m_iSerial = 0;
	int m_iSchemaSerial = 0;
	int m_iHudSerial = 0;
	int m_iResetCount = 0;
	int m_iElementRemovals[bhl::sb::MAX_ELEMENTS] = {};
	int m_iFrame = 0; //!< Incremented by Frame(), invalidates the per-frame caches

	Column m_Columns[bhl::sb::MAX_COLUMNS];
	Value m_GlobalValues[bhl::sb::MAX_COLUMNS];
	Value m_PlayerValues[MAX_PLAYERS + 1][bhl::sb::MAX_COLUMNS];
	Value m_TeamValues[MAX_TEAMS + 1][bhl::sb::MAX_COLUMNS];
	List m_Lists[bhl::sb::MAX_LISTS];

	Element m_Elements[bhl::sb::MAX_ELEMENTS];
	bhl::sb::CTemplate m_Templates[bhl::sb::MAX_ELEMENTS][bhl::sb::MAX_VARIANTS];
	unsigned int m_iVisibleMask = 0;
	unsigned int m_iHiddenStdParts = 0;

	int m_SortColumns[bhl::sb::MAX_COLUMNS] = {}; //!< Ids of the sort key columns in priority order
	int m_iSortColumnCount = 0;
	bool m_bPlayerConnected[MAX_PLAYERS + 1] = {}; //!< Connection state seen by the last Frame

	//! Ranked entities of a frame (6.4, 8.11), computed on first use
	struct Ranking
	{
		int iFrame = -1;
		int iSerial = -1;
		bool bTeamMode = false;
		int iSign = 1; //!< -1 if the first sort key is ascending
		Subject firstRow;
		int iCount = 0;
		Subject entities[MAX_PLAYERS]; //!< Participants, or teams with participants in team mode
		bool bHasNumber[MAX_PLAYERS] = {};
		int numbers[MAX_PLAYERS] = {};
	};

	struct BuiltinsCacheEntry
	{
		int iFrame = -1;
		int iSerial = -1;
		Subject subject;
		Builtins builtins;
	};

	//! Subjects whose built-in values are kept per frame: the viewer's, the leader and the rows of a Board
	static constexpr int BUILTINS_CACHE_SIZE = 16;

	mutable Ranking m_Ranking;
	mutable BuiltinsCacheEntry m_BuiltinsCache[BUILTINS_CACHE_SIZE];
	mutable int m_iNextBuiltinsCache = 0;
	mutable int m_iRunningClockSerial = -1; //!< m_iSerial when m_bHasRunningClock was computed
	mutable bool m_bHasRunningClock = false;

	HudLine m_HudLines[MAX_HUD_LINES]; //!< A ring: events are not state and change no serial
	int m_iNextHudLine = 0; //!< Slot of the next line, the oldest one when the ring is full
	int m_iLastHudLineSeq = 0;

	//! Reads the payload of a BhlScore message
	class CMsgReader;

	//! Clears everything as on Reset
	void Clear();
	void UpdateSortColumns();

	//! Returns MAX_COLUMNS values of the protocol target or nullptr
	Value *GetTargetValues(int target);

	//! Removes an element, its templates and its Feed lines; its visibility bit stays
	void RemoveElement(int id);

	const Ranking &GetRanking() const;
	Builtins ComputeBuiltins(const Subject &subject) const;

	//! Number of a ranked entity (6.4): its first-key number, or its frags without sort keys
	bool GetEntityNumber(const Subject &entity, int sortCol, int &out) const;

	//! ComparePlayers or CompareTeams for two entities of the same kind
	int CompareEntities(const Subject &a, const Subject &b) const;

	//! Cell of a subject (8.10): the player's, his team's, then global; a team subject starts with its team
	const Value &GetSubjectValue(const Subject &subject, int col) const;

	//! Value ref of an element, nullptr without one or when it is "@value" itself
	const bhl::sb::TemplateRef *GetValueRef(int elementId) const;

	int MsgFunc_BhlScore(const char *pszName, int iSize, void *pbuf);
	void ReadReset(CMsgReader &reader);
	void ReadConfig(CMsgReader &reader);
	void ReadColumn(CMsgReader &reader);
	void ReadValues(CMsgReader &reader);
	void ReadList(CMsgReader &reader);
	void ReadElement(CMsgReader &reader);
	void ReadTemplate(CMsgReader &reader);
	void ReadHudState(CMsgReader &reader);

	/**
	 * Adds a line to a Feed element: its variant's template is evaluated once, for the element's subject,
	 * with the args. Ignored for another kind, a variant without a template, a broken message, or when
	 * the text is empty and there is no icon sprite.
	 */
	void ReadEvent(CMsgReader &reader);

	/**
	 * Reads "type len payload" (4.3). A value of an unknown type, with a wrong len or a broken
	 * payload is returned empty.
	 * @return	false if the message ends inside the value
	 */
	static bool ReadValue(CMsgReader &reader, Value &value, int &type);
};

#endif
