#ifndef SCOREBOARDPROTOCOL_H
#define SCOREBOARDPROTOCOL_H
#include <enum_utils.h>

/**
 * Constants of the "BhlScore" user message.
 * See docs/scoreboard-protocol.md for the format.
 */
namespace bhl::sb
{

constexpr int PROTOCOL_VERSION = 1;
constexpr const char *MSG_NAME = "BhlScore";
constexpr const char *CAPS_KEY = "_bhl";

constexpr int MAX_MSG_SIZE = 192;
constexpr int MAX_COLUMNS = 16;
constexpr int MAX_LISTS = 8;
constexpr int MAX_LIST_ITEMS = 64;
constexpr int MAX_DOTS = 10;

//! String limits in bytes, without the terminating NUL
constexpr int MAX_KEY_LEN = 15;
constexpr int MAX_COLUMN_TITLE_LEN = 31;
constexpr int MAX_PRESET_LEN = 15;
constexpr int MAX_TITLE_LEN = 63;
constexpr int MAX_TAG_LEN = 15;
constexpr int MAX_TEXT_LEN = 31;

//! Column id that means "every column of the target" (only with E_ValueType::None)
constexpr int ALL_COLUMNS = 0xFF;

constexpr int SORT_ASC = 0x80;
constexpr int SORT_PRIORITY_MASK = 0x7F;

constexpr int TARGET_GLOBAL = 0x00;
constexpr int TARGET_PLAYER_FIRST = 0x01;
constexpr int TARGET_PLAYER_LAST = 0x40;
constexpr int TARGET_TEAM_BASE = 0x80;
constexpr int TARGET_TEAM_FIRST = 0x81;
constexpr int TARGET_TEAM_LAST = 0xC0;

enum class E_Op : unsigned int
{
	Reset = 0,
	Config = 1,
	Column = 2,
	Values = 3,
	List = 4,
};

enum class E_ValueType : unsigned int
{
	None = 0,
	Int = 1,
	Text = 2,
	Icon = 3,
	Progress = 4,
};

enum class E_Display : unsigned int
{
	Text = 0,
	Icon = 1,
	Meter = 2,
	Builtin = 3,
	Steps = 4,
};

enum class E_ConfigFlags : unsigned int
{
	None = 0,
	HideFrags = (1 << 0),
	HideDeaths = (1 << 1),
	HideEff = (1 << 2),
	HideSteamID = (1 << 3),
};

enum class E_ColumnFlags : unsigned int
{
	None = 0,
	Hidden = (1 << 0),
	AlignCenter = (1 << 1),
	AlignRight = (2 << 1),
	Primary = (1 << 3),
};

}

#endif
