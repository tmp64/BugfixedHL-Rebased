#ifndef ISCOREBOARDEXT_H
#define ISCOREBOARDEXT_H

namespace bhl
{

/**
 * Server-side scoreboard and HUD extension. The methods back the bhl_sb_* and bhl_hud_* natives;
 * bugfixedapi.inc and docs/scoreboard-protocol.md (5, 8, 9) describe their behaviour.
 *
 * Columns, lists and elements are addressed by keys of 1-15 characters of [a-z0-9_].
 * Wire ids are assigned by the server and never reach plugins.
 * All plugins share one state, so plugins prefix their keys.
 *
 * Targets of cells:
 *   0                        - the whole game;
 *   1 .. maxClients          - a player; values for players that are not connected are ignored;
 *   a value of GetTeamTarget - a team.
 * Any other target is ignored.
 *
 * Players of per-player texts and visibility: 0 is the default for everybody and drops the per-player values
 * of that variant or bit, 1 .. maxClients is that player only (5.4).
 *
 * A null string is treated as "". Strings are truncated to the limits of bhl::sb (ScoreboardProtocol.h)
 * on a UTF-8 character boundary, except templates, which are refused when too long.
 * Unknown bits of flags and parts are dropped. A setter that writes the value already stored marks nothing.
 *
 * New methods must be added to the end.
 */
class IScoreboardExt
{
public:
	//! Removes everything of every plugin, sends Reset to every ready client and turns the extension off
	virtual void Reset() = 0;

	//! Turns the custom scoreboard and the extension on. hideFlags: bhl::sb::E_ConfigFlags
	virtual void SetConfig(unsigned int hideFlags, const char *title, const char *tag) = 0;

	/**
	 * Adds a column or replaces the definition of the column with the same key; its cells are kept.
	 * display: bhl::sb::E_Display except Badge; width is clamped to 0-255; sort: 0 or a priority starting at 1,
	 * plus bhl::sb::SORT_ASC; flags: bhl::sb::E_ColumnFlags.
	 * @return	false if the key is invalid, display is Badge or unknown, or all 32 columns are used
	 */
	virtual bool AddColumn(const char *key, const char *title, int display, int width, int sort, unsigned int flags) = 0;

	//! Returns an opaque target of a team that stays valid until the map changes, or -1
	virtual int GetTeamTarget(const char *teamName) = 0;

	virtual void SetInt(int target, const char *key, int value) = 0;
	virtual void SetText(int target, const char *key, const char *text) = 0;
	virtual void SetIcon(int target, const char *key, const char *sprite, const char *text) = 0;

	//! cur and max are clamped to 0-65535
	virtual void SetProgress(int target, const char *key, int cur, int max) = 0;

	//! Clears a cell, or every cell of the target if key is ""
	virtual void Clear(int target, const char *key) = 0;

	/**
	 * count is clamped to 0-64; 0 removes the list on the clients.
	 * @return	false if the key is invalid or all 8 lists are used
	 */
	virtual bool ListResize(const char *key, int count) = 0;

	//! Items out of range and unknown lists are ignored
	virtual void ListSetInt(const char *key, int index, int value) = 0;
	virtual void ListSetText(const char *key, int index, const char *text) = 0;
	virtual void ListSetIcon(const char *key, int index, const char *sprite, const char *text) = 0;

	//! Sets a Clock cell (4.3). flags: bhl::sb::E_ClockFlags. A running clock runs from the call, so a call with the same values restarts it (5.2).
	virtual void SetClock(int target, const char *key, int ms, unsigned int flags) = 0;

	/**
	 * host may name a column that is added later.
	 * @return	false if key or host is invalid, or all 32 columns are used
	 */
	virtual bool AddBadge(const char *key, const char *host) = 0;

	/**
	 * Turns the extension on. Bit 0 of flags is ignored: the color mode comes from HudSetColor.
	 * kind: bhl::sb::E_ElementKind except None; font: bhl::sb::E_Font, unknown fonts are Text;
	 * align: anchors (4.3), bits above 3 are dropped; flags: bhl::sb::E_ElementFlags.
	 */
	virtual bool HudAdd(const char *key, int kind, float x, float y, int font, int align, unsigned int flags) = 0;

	//! place: bhl::sb::E_Place; parent may name an element that is added later; gap is clamped to 0-255
	virtual bool HudSetPlace(const char *key, int place, const char *parent, int gap) = 0;

	//! r < 0: follow hud_color; g and b are ignored. Components are clamped to 0-255.
	virtual bool HudSetColor(const char *key, int r, int g, int b, int a) = 0;

	/**
	 * A value ref "@value" is accepted; it names the element's own value, so clients treat it as missing.
	 * false, and nothing changes, if the Element message would be larger than bhl::sb::MAX_MSG_SIZE.
	 */
	virtual bool HudBind(const char *key, const char *value, const char *list, const char *select) = 0;

	//! param: bhl::sb::E_Param of the element's kind or of every kind; false as HudBind
	virtual bool HudSetParam(const char *key, int param, int value) = 0;

	virtual bool HudSetText(int player, const char *key, int variant, const char *text) = 0;
	virtual bool HudShow(int player, const char *key, bool show) = 0;

	//! Unknown keys are ignored, "" removes every element
	virtual void HudRemove(const char *key) = 0;

	//! Sets or clears only the given parts (bhl::sb::E_HudStd), for everybody. Hiding a part turns the extension on.
	virtual void HudHideStd(unsigned int parts, bool hide) = 0;

	/**
	 * slot: 0-7, bhl::sb::SLOT_WARN or bhl::sb::SLOT_ACCENT; r < 0 clears the slot. Components are clamped
	 * to 0-255. false as HudBind.
	 * Added in version v1.4
	 */
	virtual bool HudSetSlotColor(const char *key, int slot, int r, int g, int b, int a) = 0;

	/**
	 * Sends a line to a Feed element. Events are not state (5.5, 5.6): only clients that get the scoreboard and have
	 * HudV1 now get them. lifeMs is clamped to 0-65535, 0 for the life_ms param of the element. args: argCount Int
	 * values, 0 to bhl::sb::MAX_EVENT_INTS; an Icon value follows if iconSprite or iconText is not empty.
	 * Added in version v1.4
	 */
	virtual bool HudEvent(int player, const char *key, int variant, int lifeMs, const int *args, int argCount, const char *iconSprite, const char *iconText) = 0;
};

}

#endif
