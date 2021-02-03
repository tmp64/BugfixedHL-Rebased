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
native bhl_is_client_version_valid(idx)

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
