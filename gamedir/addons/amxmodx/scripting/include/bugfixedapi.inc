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
// Client capabilities and the scoreboard extension
// See docs/scoreboard-protocol.md in the BugfixedHL repository
//------------------------------------

enum BhlClientCaps
{
	BHL_CAPS_NONE = 0,
	BHL_CAPS_SCOREBOARD_V1 = (1 << 0),
	BHL_CAPS_GUNGAME_HUD_V1 = (1 << 1)
};

enum BhlSbDisplay
{
	BHL_SB_TEXT = 0,
	BHL_SB_ICON = 1,
	BHL_SB_METER = 2,
	BHL_SB_BUILTIN = 3,
	BHL_SB_STEPS = 4
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
	BHL_SB_COL_PRIMARY = (1 << 3)
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
 * Use bitwise AND to check:
 * if (bhl_get_client_caps(idx) & BHL_CAPS_GUNGAME_HUD_V1) { ... }
 * @param idx			Player's index
 * @return				BhlClientCaps bitfield, BHL_CAPS_NONE on servers without the feature
 */
native BhlClientCaps:bhl_get_client_caps(idx);

/**
 * Called when the value returned by bhl_get_client_caps() changes for a player in game.
 * @param idx			Player's index
 */
forward bhl_client_caps_changed(idx);

/**
 * Returns whether the server supports the scoreboard extension.
 * All other bhl_sb_* natives do nothing when it returns false.
 */
native bool:bhl_sb_available();

/**
 * Removes the schema, all values and lists. Clients return to the standard scoreboard.
 */
native bhl_sb_reset();

/**
 * Turns the custom scoreboard on.
 * @param hide			Standard columns to hide
 * @param preset		Client preset name, "" for the generic look
 * @param title			Replaces the server name if not empty
 * @param tag			Badge next to the title, for example "FFA"
 */
native bhl_sb_config(BhlSbHide:hide, const preset[] = "", const title[] = "", const tag[] = "");

/**
 * Adds a column, or replaces the column with the same key. Columns are shown in the order they are added.
 * @param key			Up to 15 characters of [a-z0-9_]. For BHL_SB_BUILTIN one of:
 *						rank, frags, deaths, eff, steamid, ping
 * @param title			Text or #token shipped with the client, "" for the default title of a built-in column
 * @param display		How the values are drawn
 * @param width			Width in proportional units, 0 for the default
 * @param sort			0 or sort priority starting at 1, plus BHL_SB_SORT_ASC for ascending order
 * @param flags			Column flags
 * @return				true on success
 */
native bool:bhl_sb_add_column(const key[], const title[], BhlSbDisplay:display = BHL_SB_TEXT,
                              width = 0, sort = 0, BhlSbColumnFlags:flags = BHL_SB_COL_NONE);

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
