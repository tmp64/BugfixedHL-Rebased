//------------------------------------
// BugfixedAPI
// Requires BugfixedAPI AMXX module
// https://github.com/tmp64/BugfixedHL
//------------------------------------

#if defined _bugfixedapi_included
	#endinput
#endif
#define _bugfixedapi_included

#pragma reqlib bugfixedapi
#if !defined AMXMODX_NOAUTOLOAD
	#pragma loadlib bugfixedapi
#endif

enum E_ClientSupports
{
	AGHL_SUPPORTS_NONE = 0,
	AGHL_SUPPORTS_UNICODE_MOTD = (1 << 0),
	AGHL_SUPPORTS_HTML_MOTD = (1 << 1)
};

enum E_MotdType
{
	AGHL_MOTD_NONE = 0,
	AGHL_MOTD_PLAIN = 1 << 0,
	AGHL_MOTD_UNICODE = 1 << 1,
	AGHL_MOTD_HTML = 1 << 2,
	AGHL_MOTD_ALL = AGHL_MOTD_PLAIN | AGHL_MOTD_UNICODE | AGHL_MOTD_HTML
};

/**
 * Returns 1 if API is available.
 * @return				0 or 1
 */
native bhl_is_api_ready();

/**
 * Returns server DLL version.
 * @param major			Variable to store version major in
 * @param minor			Variable to store version minor in
 * @return				0 or 1
 */
native bhl_get_server_version(&major, &minor, &patch);

/**
 * Returns bitfield with BugfixedHL features the client's game supports.
 * Use bitwise AND to check:
 * if (bhl_get_client_supports(idx) & AGHL_SUPPORTS_UNICODE_MOTD) { ... }
 * @param idx			Player's index
 * @return				integer
 */
native E_ClientSupports:bhl_get_client_supports(idx);

/**
 * Returns whether or not player has color code parsing (^1, ^2, ...) enabled (BugfixedHL or Rofi's client DLL).
 * @param idx			Player's index
 * @return				0 or 1
 */
native bhl_get_color_support(idx);

/**
 * Copies player's client version string to buf
 * @param idx			Player's index
 * @param major			Variable to store version major in
 * @param minor			Variable to store version minor in
 * @param patch			Variable to store version patch in
 * @return				1 on success, 0 otherwise (player is not connected or aghl_version is invalid)
 */
native bhl_get_client_version(idx, &major, &minor, &patch);

/**
 * Returns whether or not player's client build is dirty (has uncommited changes)
 * @param idx			Player's index
 * @return				0 or 1
 */
native bhl_is_client_dirty(idx);

/**
 * Copies player's client version commit to a buffer
 * @param idx			Player's index
 * @param buf			Buffer to store the string
 * @param size			Buffer size
 * @return				1 on success, 0 otherwise (player is not connected or aghl_version is invalid)
 */
native bhl_get_client_version_commit(idx, buf[], size = sizeof(buf));

/**
 * Returns 1 if server knows client's game DLL version
 * @param idx			Player's index
 * @return				0 or 1
 */
native bhl_is_client_version_valid(idx);

/**
 * @see bhl_set_automatic_motd
 */
native bhl_get_automatic_motd(E_MotdType:type);

/**
 * Enables or disables automatic send of MOTD to connecting clients.
 *
 * The server checks support for HTML MOTD, then Unicode MOTD.
 * If HTML MOTD is supported, the server will read it from file int `motdfile_html`.
 * If not, if Unicode MOTD is supported, the server will read it from file in `motdfile_unicode`.
 * Otherwise, file `motdfile` is sent.
 *
 * If a MOTD file is supported but it is disabled with SetAutomaticMotd, no MOTD is sent.
 *
 * @see CHalfLifeMultiplay::InitHUD for details (dlls/multiplay_gamerules.cpp)
 */
native bhl_set_automatic_motd(E_MotdType:type, _state);

/**
 * Sends a MOTD to the client from a string
 * @param type MOTD type
 * @param idx Client index [1; (maxplayers)]
 * @param str The string
 */
native bhl_show_motd_from_string(E_MotdType:type, idx, const str[]);

/**
 * Sends a MOTD to the client from a file
 * @param type MOTD type
 * @param idx Client index [1; (maxplayers)]
 * @param file Path to the file relative from gamedir
 */
native bhl_show_motd_from_file(E_MotdType:type, idx, const file[]);

/**
 * Sets player's score and sends score update message to all players.
 * @param	idx		Client index [1; (maxplayers)]
 * @param	frags	Number of kills (score)
 * @param	deaths	Number of deaths
 */
native bhl_set_player_score(idx, frags, deaths);

//------------------------------------
// Client capabilities, the scoreboard and HUD extension
// See docs/scoreboard-protocol.md in the BugfixedHL repository
//------------------------------------

enum BhlClientCaps
{
	BHL_CAPS_NONE = 0,
	BHL_CAPS_SCOREBOARD_V1 = (1 << 0),
	BHL_CAPS_HUD_V1 = (1 << 1)
};

enum BhlSbDisplay
{
	BHL_SB_TEXT = 0,
	BHL_SB_ICON = 1,
	BHL_SB_METER = 2,
	BHL_SB_BUILTIN = 3,
	BHL_SB_STEPS = 4,
	BHL_SB_BADGE = 5	// added by bhl_sb_add_badge only
};

enum BhlSbHide
{
	BHL_SB_HIDE_NONE = 0,
	BHL_SB_HIDE_FRAGS = (1 << 0),
	BHL_SB_HIDE_DEATHS = (1 << 1),
	BHL_SB_HIDE_EFF = (1 << 2),
	BHL_SB_HIDE_STEAMID = (1 << 3)
};

enum BhlSbColumnFlags
{
	BHL_SB_COL_NONE = 0,
	BHL_SB_COL_HIDDEN = (1 << 0),
	BHL_SB_COL_CENTER = (1 << 1),
	BHL_SB_COL_RIGHT = (2 << 1),
	BHL_SB_COL_PRIMARY = (1 << 3),
	BHL_SB_COL_MARK_LAST = (1 << 4)
};

enum BhlSbTarget
{
	BHL_SB_INVALID = -1,
	BHL_SB_GLOBAL = 0
};

/**
 * Add to the sort priority of bhl_sb_add_column for ascending order.
 */
#define BHL_SB_SORT_ASC 0x80

enum BhlSbClock
{
	BHL_SB_CLOCK_STOP = 0,
	BHL_SB_CLOCK_RUN = (1 << 0),
	BHL_SB_CLOCK_DOWN = (1 << 1)
};

enum BhlHudKind
{
	BHL_HUD_TEXT = 1,
	BHL_HUD_METER = 2,
	BHL_HUD_STRIP = 3,
	BHL_HUD_ICON = 4,
	BHL_HUD_BOARD = 5,	// rows of the scoreboard order or of a list
	BHL_HUD_FEED = 6,	// lines of bhl_hud_event
	BHL_HUD_BOX = 7	// backdrop of a group, placed with a parent place at the root of the group
};

enum BhlHudFont
{
	BHL_HUD_FONT_TEXT = 0,
	BHL_HUD_FONT_LARGE = 1,
	BHL_HUD_FONT_SMALL = 2,
	BHL_HUD_FONT_HUGE = 3
};

enum BhlHudAlign
{
	BHL_HUD_AUTO = 0,
	BHL_HUD_LEFT = 1,
	BHL_HUD_HCENTER = 2,
	BHL_HUD_RIGHT = 3,
	BHL_HUD_TOP = (1 << 2),
	BHL_HUD_VCENTER = (2 << 2),
	BHL_HUD_BOTTOM = (3 << 2)
};

enum BhlHudFlags
{
	BHL_HUD_NONE = 0,
	BHL_HUD_SUBJECT = (1 << 1),
	BHL_HUD_NO_SCOREBOARD = (1 << 2),
	BHL_HUD_LEADER = (1 << 3),
	BHL_HUD_DEFAULT = (1 << 1) | (1 << 2)
};

enum BhlHudPlace
{
	BHL_HUD_SCREEN = 0,
	BHL_HUD_PARENT = 1,
	BHL_HUD_NUMBERS_ROW = 2,
	BHL_HUD_BANNER_TOP = 3,
	BHL_HUD_BANNER_BOTTOM = 4,
	BHL_HUD_PARENT_LEFT = 5,
	BHL_HUD_PARENT_RIGHT = 6
};

enum BhlHudParam
{
	BHL_HUD_P_DOTS = 1,
	BHL_HUD_P_DOT_SIZE = 2,
	BHL_HUD_P_DOT_GAP = 3,
	BHL_HUD_P_BAR_WIDE = 4,
	BHL_HUD_P_COUNTER = 5,
	BHL_HUD_P_COUNTER_GAP = 6,
	BHL_HUD_P_ALPHA_EMPTY = 7,
	BHL_HUD_P_CELL_WIDE = 10,
	BHL_HUD_P_CELL_MIN_WIDE = 11,
	BHL_HUD_P_CELL_TALL = 12,
	BHL_HUD_P_CELL_GAP = 13,
	BHL_HUD_P_ICON_PADDING = 14,
	BHL_HUD_P_MAX_WIDE = 15,
	BHL_HUD_P_ALPHA_PASSED = 16,
	BHL_HUD_P_ALPHA_CURRENT = 17,
	BHL_HUD_P_ALPHA_FUTURE = 18,
	BHL_HUD_P_ALPHA_ICON_FUTURE = 19,
	BHL_HUD_P_FRAME = 20,
	BHL_HUD_P_ICON_TALL = 30,
	BHL_HUD_P_ICON_GAP = 31,
	BHL_HUD_P_FLASH_MS = 32,	// every kind
	BHL_HUD_P_FADE_MS = 33,	// every kind
	BHL_HUD_P_HOLD_MS = 34,	// every kind
	BHL_HUD_P_WARN_AT = 35,	// every kind
	BHL_HUD_P_WARN_PULSE_MS = 36,	// every kind
	BHL_HUD_P_FORM = 40,	// 0 dots or bar, 1 ring
	BHL_HUD_P_TRACK = 41,	// 0 off, 1 marks of the other players
	BHL_HUD_P_ROWS = 44,
	BHL_HUD_P_BOARD_MODE = 45,	// 0 first rows, 1 first rows and the own row, 2 rows around the own row
	BHL_HUD_P_ROW_GAP = 46,
	BHL_HUD_P_COLUMN_GAP = 47,
	BHL_HUD_P_FILL_ALPHA = 48,
	BHL_HUD_P_HIGHLIGHT_ALPHA = 49,
	BHL_HUD_P_LINES = 52,
	BHL_HUD_P_LIFE_MS = 53,
	BHL_HUD_P_LINE_FADE_MS = 54,
	BHL_HUD_P_QUEUE = 55,	// 0 newest lines on top, 1 one line at a time
	BHL_HUD_P_LINE_GAP = 56,
	BHL_HUD_P_ACCENT_WIDE = 60,
	BHL_HUD_P_RADIUS = 61
};

enum BhlHudStd
{
	BHL_HUD_STD_NONE = 0,
	BHL_HUD_STD_TIMER = (1 << 0),
	BHL_HUD_STD_NEXTMAP = (1 << 1),
	BHL_HUD_STD_CUSTOM_TIMERS = (1 << 2),
	BHL_HUD_STD_SPEEDOMETER = (1 << 3),
	BHL_HUD_STD_JUMPSPEED = (1 << 4)
};

/**
 * r of bhl_hud_color: the element follows hud_color and uses only alpha.
 */
#define BHL_HUD_COLOR -1

/**
 * Longest template in bytes. Longer texts are not set, they are never cut.
 */
#define BHL_HUD_MAX_TEXT 188

/**
 * Color slots of bhl_hud_slot_color after the text variants 0-7.
 * BHL_HUD_SLOT_WARN is used while the warn_at param holds, BHL_HUD_SLOT_ACCENT by the accent bar of a box.
 */
#define BHL_HUD_SLOT_WARN 8
#define BHL_HUD_SLOT_ACCENT 9

/**
 * Returns the scoreboard target of a player.
 * @param idx			Player's index
 */
stock BhlSbTarget:bhl_sb_player(idx)
{
	return BhlSbTarget:idx;
}

/**
 * Returns the scoreboard target of a team.
 * The target stays valid until the map changes.
 * @param teamName		Team name, the same that get_user_team() returns
 * @return				Team target or BHL_SB_INVALID
 */
native BhlSbTarget:bhl_sb_team(const teamName[]);

/**
 * Returns features the client reports to the server.
 * BHL_CAPS_HUD_V1 is only set together with BHL_CAPS_SCOREBOARD_V1. Bots and HLTV get BHL_CAPS_NONE.
 * Use bitwise AND to check:
 * if (bhl_get_client_caps(idx) & BHL_CAPS_HUD_V1) { ... }
 * @param idx			Player's index
 * @return				BhlClientCaps bitfield, BHL_CAPS_NONE on servers without the feature
 */
native BhlClientCaps:bhl_get_client_caps(idx);

/**
 * Called when the value returned by bhl_get_client_caps() changes for a player in game,
 * for example when he changes hud_server_elements.
 * Not called for the caps a player has when he joins: check bhl_get_client_caps() when you need it.
 * @param idx			Player's index
 */
forward bhl_client_caps_changed(idx);

/**
 * Returns whether the server supports the scoreboard and HUD extension (server API 1.4 or newer).
 * All other bhl_sb_* and bhl_hud_* natives log one error and do nothing when it returns false.
 */
native bool:bhl_sb_available();

/**
 * Removes everything of every plugin: schema, values, lists, elements, texts, visibility
 * and hidden standard HUD parts. Clients return to the standard scoreboard and HUD.
 */
native bhl_sb_reset();

/**
 * Turns the custom scoreboard on.
 * Not needed for HUD elements: a plugin that only adds elements keeps the standard scoreboard.
 * @param hide			Standard columns to hide
 * @param title			Replaces the server name if not empty
 * @param tag			Badge after the server name, for example "FFA"
 */
native bhl_sb_config(BhlSbHide:hide, const title[] = "", const tag[] = "");

/**
 * Adds a column, or replaces the column with the same key. Its values are kept.
 * Custom and built-in columns are shown in the order their keys were first added; re-adding a key
 * keeps its place. rank is always first. Standard columns that no built-in column moves keep
 * their default places.
 * @param key			Up to 15 characters of [a-z0-9_]. For BHL_SB_BUILTIN one of:
 *						rank, frags, deaths, eff, steamid, ping
 * @param title			Text or #token shipped with the client, "" for the default title of a built-in column
 * @param display		How the values are drawn; BHL_SB_BADGE is refused, use bhl_sb_add_badge
 * @param width			Width in proportional units, 0 for the default
 * @param sort			0 or sort priority starting at 1, plus BHL_SB_SORT_ASC for ascending order
 * @param flags			Column flags
 * @return				true on success
 */
native bool:bhl_sb_add_column(const key[], const title[], BhlSbDisplay:display = BHL_SB_TEXT,
                              width = 0, sort = 0, BhlSbColumnFlags:flags = BHL_SB_COL_NONE);

/**
 * Adds a badge column: its values are drawn as badges after the values of the column host.
 * Values are set with the usual bhl_sb_set_* natives. A #token is localized.
 * @param key			Up to 15 characters of [a-z0-9_]
 * @param host			Key of the column the badges follow; it may be added later
 * @return				true on success
 */
native bool:bhl_sb_add_badge(const key[], const host[]);

/**
 * Sets a value of a column for a target.
 * Values for players that are not connected are ignored.
 * Setting the value a cell already has sends nothing.
 * @param target		BHL_SB_GLOBAL, bhl_sb_player(idx) or bhl_sb_team(name)
 * @param key			Column key
 */
native bhl_sb_set_int(BhlSbTarget:target, const key[], value);
native bhl_sb_set_text(BhlSbTarget:target, const key[], const text[]);
native bhl_sb_set_icon(BhlSbTarget:target, const key[], const sprite[], const text[]);
native bhl_sb_set_progress(BhlSbTarget:target, const key[], cur, max);

/**
 * Sets a clock: a time that the client runs itself, for timers that change every frame.
 * @param ms			Value now, in milliseconds
 * @param flags			BHL_SB_CLOCK_RUN runs it, BHL_SB_CLOCK_DOWN makes it count down to 0
 */
native bhl_sb_set_clock(BhlSbTarget:target, const key[], ms, BhlSbClock:flags = BHL_SB_CLOCK_RUN);

/**
 * Clears a value of a target, or every value of the target if key is empty.
 */
native bhl_sb_clear(BhlSbTarget:target, const key[] = "");

/**
 * Creates a list on first use and sets its size (up to 64 items).
 * @param key			Up to 15 characters of [a-z0-9_]
 * @return				true on success
 */
native bool:bhl_sb_list_resize(const key[], count);

/**
 * Sets an item of a list. Items out of range are ignored.
 */
native bhl_sb_list_set_int(const key[], index, value);
native bhl_sb_list_set_text(const key[], index, const text[]);
native bhl_sb_list_set_icon(const key[], index, const sprite[], const text[]);

/**
 * Adds a HUD element, or changes kind, position, font, anchors and flags of an existing one.
 * A new element is shown, is placed on the screen, follows hud_color with alpha 255 and has
 * no refs, params or texts. A kind change drops its texts, refs and params.
 * @param key			Up to 15 characters of [a-z0-9_], in their own namespace
 * @param kind			Kind of the element
 * @param x				Like in set_hudmessage: 0.0-1.0 from the left, negative from the right, -1.0 is the center
 * @param y				The same from the top
 * @param font			Font of the texts
 * @param align			Horizontal and vertical anchor, BHL_HUD_AUTO follows set_hudmessage
 * @param flags			Element flags; the color mode comes from bhl_hud_color
 * @return				false for an invalid key or kind, or when 32 elements exist
 */
native bool:bhl_hud_add(const key[], BhlHudKind:kind, Float:x = -1.0, Float:y = -1.0,
                        BhlHudFont:font = BHL_HUD_FONT_TEXT, BhlHudAlign:align = BHL_HUD_AUTO,
                        BhlHudFlags:flags = BHL_HUD_DEFAULT);

/**
 * Sets what the position of an element is relative to.
 * While no element has the key parent, the element is not drawn.
 * @param place			Place of the element
 * @param parent		Key of the parent for the parent places; it may be added later
 * @param gap			Distance to the parent, or the margin on the numbers row, in proportional units
 * @return				false for an unknown element or place, or an invalid parent key
 */
native bool:bhl_hud_place(const key[], BhlHudPlace:place, const parent[] = "", gap = 0);

/**
 * Sets the color of an element.
 * @param r				0-255, or BHL_HUD_COLOR to follow hud_color (g and b are ignored then)
 * @return				false for an unknown element
 */
native bool:bhl_hud_color(const key[], r = BHL_HUD_COLOR, g = 0, b = 0, a = 255);

/**
 * Sets the data an element shows.
 * In Pawn strings the ^ prefix of a ref is written ^^.
 * @param value			Column key or built-in value with an optional ^ and one of :cur, :max and :left,
 *						for example "progress", "@state", "level:left", "^^weapon"
 * @param list			List key: the icons of a strip, the rows of a board
 * @param select		Ref like value whose number picks the text variant 0-7, "" for variant 0
 * @return				false for an unknown element, an invalid ref or list key, or when the element
 *						would not fit in a message; nothing changes then
 */
native bool:bhl_hud_bind(const key[], const value[] = "", const list[] = "", const select[] = "");

/**
 * Sets a parameter of an element kind, or of every kind. The value is clamped to the range of the parameter.
 * @return				false for an unknown element, a parameter of another kind, or when the element
 *						would not fit in a message
 */
native bool:bhl_hud_param(const key[], BhlHudParam:param, value);

/**
 * Sets a text variant of an element: a template with placeholders such as {@place} and {level:cur}.
 * In Pawn strings the ^ prefix of a ref is written ^^: "{^^weapon}".
 * Formats like format(): %L works and %% writes %. A result longer than BHL_HUD_MAX_TEXT bytes
 * is not set. Never format text typed by players into a template; escape server strings
 * with bhl_hud_escape().
 * @param player		0 sets the default for everybody and drops the per-player texts of the variant,
 *						a player index sets the text for that player only until his slot is cleared
 *						(connect, disconnect), so set it again in client_putinserver
 * @param variant		0-7
 * @param fmt			Template, "" removes the variant
 * @return				false for an unknown element, a variant out of range, a player out of range
 *						or not connected, or a too long text
 */
native bool:bhl_hud_text(player, const key[], variant, const fmt[], any:...);

/**
 * Shows or hides an element.
 * @param player		0 sets the default for everybody and drops the per-player visibility,
 *						a player index sets it for that player only until his slot is cleared
 *						(connect, disconnect), so set it again in client_putinserver
 * @return				false for an unknown element, or a player out of range or not connected
 */
native bool:bhl_hud_show(player, const key[], bool:show = true);

/**
 * Removes an element, or every element if key is empty.
 */
native bhl_hud_remove(const key[] = "");

/**
 * Hides or shows standard HUD parts on clients that draw the elements.
 * Changes only the given parts, so plugins that hide different parts don't undo each other.
 */
native bhl_hud_hide_std(BhlHudStd:parts, bool:hide = true);

/**
 * Sets the color of a color slot of an element. It replaces the color of the element while that
 * text variant is drawn (slots 0-7), while the warn_at param holds (BHL_HUD_SLOT_WARN), or for
 * the accent bar of a box (BHL_HUD_SLOT_ACCENT). It is used as is, also for elements that follow
 * hud_color. A kind change drops the slot colors.
 * @param slot			0-7, BHL_HUD_SLOT_WARN or BHL_HUD_SLOT_ACCENT
 * @param r				0-255, or -1 to clear the slot (g, b and a are ignored then)
 * @return				false for an unknown element, a slot out of range, or when the element
 *						would not fit in a message
 */
native bool:bhl_hud_slot_color(const key[], slot, r, g, b, a = 255);

/**
 * Adds a line to a feed: the text variant is evaluated once with the numbers and the icon.
 * In the template {@a} to {@d} are the numbers ({@a:name} writes the name of the player with
 * that index) and {@icon} is the icon text; the icon sprite is drawn before the line.
 * Lines are not kept: only players in game who draw elements (BHL_CAPS_HUD_V1) get them now,
 * and a line that can't be sent before its life ends is dropped.
 * @param player		0 for every player, or a player index
 * @param key			Key of a BHL_HUD_FEED element
 * @param variant		Text variant 0-7
 * @param life_ms		How long the line is shown, up to 65535, 0 for the life_ms param of the element
 * @param sprite		Icon of the line: hud.txt name or "sprites/<name>.spr" precached by the plugin, "" for none
 * @param sprite_text	Text of the icon, written by {@icon}
 * @param ...			Up to 4 numbers
 * @return				false for an unknown element or one that is not a feed, a variant out of range,
 *						a player out of range or not connected, or more than 4 numbers
 */
native bool:bhl_hud_event(player, const key[], variant, life_ms = 0, const sprite[] = "",
                          const sprite_text[] = "", any:...);

/**
 * Sets a text variant from the plugin dictionary: for one player in his language,
 * or for player 0 the default in the server language plus every player in game in his language.
 * Players who join later get the default, so call it with the player index in client_putinserver.
 * In the dictionary ^ is written once. AMXX turns ^n, ^t and ^1-^4 there into control characters
 * and breaks every other ^ after them in the same text, so a ^ ref can't name a key that starts
 * with n, t or 1-4 and can't follow such an escape.
 * @param mlKey			Dictionary key; a literal % in the text is written %%
 */
stock bool:bhl_hud_text_lang(player, const key[], variant, const mlKey[])
{
	if (player)
		return bhl_hud_text(player, key, variant, "%L", player, mlKey);

	new players[32], num;
	get_players(players, num, "ch");
	bhl_hud_text(0, key, variant, "%L", LANG_SERVER, mlKey);

	for (new i; i < num; i++)
		bhl_hud_text(players[i], key, variant, "%L", players[i], mlKey);

	return true;
}

//
// Doubles { and } so that a server string formatted into a template stays literal.
// A too long string is cut on a UTF-8 character boundary. src and dst must be different arrays.
// len is charsmax(dst). Returns the number of characters written.
//
stock bhl_hud_escape(const src[], dst[], len)
{
	new i, j;

	for (; src[i] && j < len; i++)
	{
		if (src[i] == '{' || src[i] == '}')
		{
			if (j + 2 > len)
				break;

			dst[j++] = src[i];
		}

		dst[j++] = src[i];
	}

	// The next byte continues a character, so drop its copied part
	while (j > 0 && (src[i] & 0xC0) == 0x80)
	{
		i--;
		j--;
	}

	dst[j] = EOS;
	return j;
}
