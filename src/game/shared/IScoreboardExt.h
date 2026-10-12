#ifndef ISCOREBOARDEXT_H
#define ISCOREBOARDEXT_H

namespace bhl
{

/**
 * Server-side scoreboard extension (docs/scoreboard-protocol.md).
 *
 * Columns and lists are addressed by keys, wire ids are assigned by the server.
 * Targets:
 *   0                        - the whole game;
 *   1 .. maxClients          - a player;
 *   a value of GetTeamTarget - a team.
 * Any other target is ignored.
 *
 * New methods must be added to the end.
 */
class IScoreboardExt
{
public:
	/**
	 * Removes the schema, all cells and lists and sends Reset to the clients.
	 */
	virtual void Reset() = 0;

	/**
	 * Sets the config and turns the custom scoreboard on.
	 * @param	hideFlags	bhl::sb::E_ConfigFlags
	 * @param	preset		Client preset name, "" for the generic look
	 * @param	title		Replaces the server name if not empty
	 * @param	tag			Badge next to the title
	 */
	virtual void SetConfig(unsigned int hideFlags, const char *preset, const char *title, const char *tag) = 0;

	/**
	 * Adds a column or replaces the column with the same key.
	 * @param	display	bhl::sb::E_Display
	 * @param	sort	0 or priority starting at 1, plus bhl::sb::SORT_ASC
	 * @param	flags	bhl::sb::E_ColumnFlags
	 * @return	false if the key is invalid or there are no free columns
	 */
	virtual bool AddColumn(const char *key, const char *title, int display, int width, int sort, unsigned int flags) = 0;

	/**
	 * Returns an opaque target of a team that stays valid until the map changes, or -1.
	 */
	virtual int GetTeamTarget(const char *teamName) = 0;

	virtual void SetInt(int target, const char *key, int value) = 0;
	virtual void SetText(int target, const char *key, const char *text) = 0;
	virtual void SetIcon(int target, const char *key, const char *sprite, const char *text) = 0;
	virtual void SetProgress(int target, const char *key, int cur, int max) = 0;

	/**
	 * Clears a cell, or every cell of the target if key is null or empty.
	 */
	virtual void Clear(int target, const char *key) = 0;

	/**
	 * Creates the list on first use and sets its size.
	 * @return	false if the key is invalid or there are no free lists
	 */
	virtual bool ListResize(const char *key, int count) = 0;

	virtual void ListSetInt(const char *key, int index, int value) = 0;
	virtual void ListSetText(const char *key, int index, const char *text) = 0;
	virtual void ListSetIcon(const char *key, int index, const char *sprite, const char *text) = 0;
};

}

#endif
