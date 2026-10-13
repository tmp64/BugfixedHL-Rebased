#ifndef ISCOREBOARDEXT_H
#define ISCOREBOARDEXT_H

namespace bhl
{

/**
 * Server-side scoreboard and HUD extension (docs/scoreboard-protocol.md).
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
 * Players of per-player texts and visibility: 0 is the default for everybody,
 * 1 .. maxClients is that player only (5.4).
 *
 * Strings are truncated to the limits of bhl::sb (ScoreboardProtocol.h) on a UTF-8 character boundary,
 * except templates, which are refused when too long. Unknown bits of flags and parts are dropped.
 * A setter that writes the value already stored marks nothing.
 *
 * New methods must be added to the end.
 */
class IScoreboardExt
{
public:
	/**
	 * Removes everything of every plugin: config, columns, cells, lists, elements, templates,
	 * visibility, per-player overrides and hidden standard HUD parts.
	 * Sends Reset to every ready client and turns the extension off.
	 */
	virtual void Reset() = 0;

	/**
	 * Sets the config and turns the custom scoreboard on. Turns the extension on.
	 * @param	hideFlags	bhl::sb::E_ConfigFlags
	 * @param	title		Replaces the server name if not empty, up to 63 bytes
	 * @param	tag			Badge after the server name, up to 15 bytes
	 */
	virtual void SetConfig(unsigned int hideFlags, const char *title, const char *tag) = 0;

	/**
	 * Adds a column or replaces the definition of the column with the same key; its cells are kept.
	 * A Builtin column takes one of the keys rank, frags, deaths, eff, steamid and ping.
	 * @param	title	Text or #token, up to 31 bytes; "" keeps the title of a built-in column
	 * @param	display	bhl::sb::E_Display; Badge is refused, use AddBadge
	 * @param	width	Proportional units, 0 for the default; clamped to 0-255
	 * @param	sort	0 or priority starting at 1, plus bhl::sb::SORT_ASC
	 * @param	flags	bhl::sb::E_ColumnFlags
	 * @return	false if the key is invalid, display is Badge or unknown, or all 32 columns are used
	 */
	virtual bool AddColumn(const char *key, const char *title, int display, int width, int sort, unsigned int flags) = 0;

	/**
	 * Returns an opaque target of a team that stays valid until the map changes, or -1.
	 * @param	teamName	The name get_user_team() returns
	 */
	virtual int GetTeamTarget(const char *teamName) = 0;

	virtual void SetInt(int target, const char *key, int value) = 0;
	virtual void SetText(int target, const char *key, const char *text) = 0;
	virtual void SetIcon(int target, const char *key, const char *sprite, const char *text) = 0;

	//! cur and max are clamped to 0-65535
	virtual void SetProgress(int target, const char *key, int cur, int max) = 0;

	/**
	 * Clears a cell, or every cell of the target if key is null or empty.
	 */
	virtual void Clear(int target, const char *key) = 0;

	/**
	 * Creates the list on first use and sets its size. A size of 0 removes the list on the clients.
	 * @param	count	Clamped to 0-64
	 * @return	false if the key is invalid or all 8 lists are used
	 */
	virtual bool ListResize(const char *key, int count) = 0;

	//! Items out of range and unknown lists are ignored
	virtual void ListSetInt(const char *key, int index, int value) = 0;
	virtual void ListSetText(const char *key, int index, const char *text) = 0;
	virtual void ListSetIcon(const char *key, int index, const char *sprite, const char *text) = 0;

	/**
	 * Sets a Clock cell, a time that the client runs itself (4.3).
	 * @param	ms		Value now, in milliseconds
	 * @param	flags	bhl::sb::E_ClockFlags: Run runs it, Down makes it count down to 0
	 */
	virtual void SetClock(int target, const char *key, int ms, unsigned int flags) = 0;

	/**
	 * Adds a Badge column, or turns the column with the same key into one; its cells are kept.
	 * Cells are set with the usual setters. The server sends it with sort 0 and without Primary.
	 * host may name a column that is added later.
	 * @return	false if key or host is invalid, or all 32 columns are used
	 */
	virtual bool AddBadge(const char *key, const char *host) = 0;

	/**
	 * Adds an element, or changes kind, position, font, anchors and flags of an existing one.
	 * x and y follow set_hudmessage. A kind change drops templates, refs and params.
	 * Bit 0 of flags is ignored: the color mode comes from HudSetColor.
	 * A new element is visible for everybody, placed on the screen, follows hud_color with alpha 255
	 * and has no refs, params or templates. Turns the extension on.
	 * @param	kind	bhl::sb::E_ElementKind except None
	 * @param	font	bhl::sb::E_Font; unknown fonts are Text
	 * @param	align	Anchors (4.3), bits above 3 are dropped
	 * @param	flags	bhl::sb::E_ElementFlags
	 * @return	false if the key or kind is invalid, or the key is new and 32 elements exist
	 */
	virtual bool HudAdd(const char *key, int kind, float x, float y, int font, int align, unsigned int flags) = 0;

	/**
	 * parent may name an element that is added later.
	 * @param	place	bhl::sb::E_Place
	 * @param	parent	Key of the parent for the parent places, "" or null for none; ignored by other places
	 * @param	gap		Proportional units, clamped to 0-255
	 * @return	false if the element is unknown, the place is unknown or parent is not a valid key
	 */
	virtual bool HudSetPlace(const char *key, int place, const char *parent, int gap) = 0;

	/**
	 * r < 0: follow hud_color; g and b are ignored. Components are clamped to 0-255.
	 * @return	false if the element is unknown
	 */
	virtual bool HudSetColor(const char *key, int r, int g, int b, int a) = 0;

	/**
	 * Sets the refs of an element. null is the same as "".
	 * A value ref "@value" is valid and accepted; it names the element's own value, so clients
	 * treat it as missing instead of resolving it again.
	 * @param	value	Ref (bhl::sb::IsValidRef) or ""
	 * @param	list	List key or ""
	 * @param	select	Ref or ""
	 * @return	false if the element is unknown, a ref or the list key is invalid, or the Element message
	 *			would be larger than bhl::sb::MAX_MSG_SIZE; nothing changes then
	 */
	virtual bool HudBind(const char *key, const char *value, const char *list, const char *select) = 0;

	/**
	 * Sets a kind param (8.7-8.9) or a param of every kind. The value is clamped to the range of the param.
	 * @param	param	bhl::sb::E_Param
	 * @return	false if the element is unknown, has no such param or the Element message would be larger
	 *			than bhl::sb::MAX_MSG_SIZE
	 */
	virtual bool HudSetParam(const char *key, int param, int value) = 0;

	/**
	 * Sets a template variant. "" removes the variant.
	 * player 0: the default, drops the per-player texts of the variant; otherwise that player only.
	 * false for a text longer than 188 bytes, which is never cut.
	 * @param	variant	0-7
	 * @return	false if the element is unknown, the variant or player is out of range,
	 *			the player is not connected or the text is too long; nothing changes then
	 */
	virtual bool HudSetText(int player, const char *key, int variant, const char *text) = 0;

	/**
	 * player 0: the default, drops the per-player visibility; otherwise that player only.
	 * @return	false if the element is unknown, the player is out of range or not connected
	 */
	virtual bool HudShow(int player, const char *key, bool show) = 0;

	/**
	 * Removes an element with its templates and per-player overrides. Unknown keys are ignored.
	 * "" or null removes every element.
	 */
	virtual void HudRemove(const char *key) = 0;

	/**
	 * Sets or clears only the given standard HUD parts, for everybody. Hiding a part turns the extension on.
	 * @param	parts	bhl::sb::E_HudStd
	 */
	virtual void HudHideStd(unsigned int parts, bool hide) = 0;

	/**
	 * Sets the color of a color slot (4.3): slots 0-7 are used while that template variant is drawn,
	 * then bhl::sb::SLOT_WARN and bhl::sb::SLOT_ACCENT. The color is used as is, also with hud_color.
	 * A kind change drops the slot colors. Components are clamped to 0-255.
	 * Added in version v1.4
	 * @param	r	r < 0 clears the slot; g, b and a are ignored then
	 * @return	false if the element is unknown, the slot is out of range or the Element message would be
	 *			larger than bhl::sb::MAX_MSG_SIZE; nothing changes then
	 */
	virtual bool HudSetSlotColor(const char *key, int slot, int r, int g, int b, int a) = 0;

	/**
	 * Sends a line to a Feed element (4.3, op Event). Events are not state: they go only to clients that
	 * get the scoreboard and have HudV1 when this is called, wait until the client has the element,
	 * are dropped when their life has passed before they could be sent, and are never sent again.
	 * At most 8 events wait per client, the oldest is dropped.
	 * Added in version v1.4
	 * @param	player		0 for everybody, otherwise that player only
	 * @param	variant		Template variant of the line, 0-7
	 * @param	lifeMs		0 for the life_ms param of the element; clamped to 0-65535
	 * @param	args		argCount numbers, sent as Int values @a to @d
	 * @param	argCount	0 to bhl::sb::MAX_EVENT_INTS
	 * @param	iconSprite	With iconText: an Icon value after the numbers if either is not empty; cut like SetIcon
	 * @return	false if the element is unknown or not a Feed, the variant, argCount or player is out of range,
	 *			or the player is not connected
	 */
	virtual bool HudEvent(int player, const char *key, int variant, int lifeMs, const int *args, int argCount, const char *iconSprite, const char *iconText) = 0;
};

}

#endif
