#ifndef SCOREBOARDPROTOCOL_H
#define SCOREBOARDPROTOCOL_H
#include <algorithm>
#include <cmath>
#include <cstring>
#include <enum_utils.h>

/**
 * Constants of the "BhlScore" user message, shared by the server and the client.
 * See docs/scoreboard-protocol.md for the format. Section numbers below refer to it.
 */
namespace bhl::sb
{

constexpr const char *MSG_NAME = "BhlScore";
constexpr const char *CAPS_KEY = "_bhl";

//----------------------------------------------------------------------------------------------------
// Limits (4.7). String limits are in bytes, without the terminating NUL.
//----------------------------------------------------------------------------------------------------
constexpr int MAX_MSG_SIZE = 192;
constexpr int MAX_COLUMNS = 32;
constexpr int MAX_LISTS = 8;
constexpr int MAX_LIST_ITEMS = 64;
constexpr int MAX_ELEMENTS = 32;
constexpr int MAX_VARIANTS = 8;
constexpr int MAX_DOTS = 10; //!< A scoreboard Meter with max above this is a bar
constexpr int MAX_EVENT_ARGS = 5; //!< Args of one Event: 4 numbers and an icon from the natives
constexpr int MAX_EVENT_INTS = 4; //!< Number args of bhl_hud_event

constexpr int MAX_KEY_LEN = 15; //!< Column, list and element keys
constexpr int MAX_COLUMN_TITLE_LEN = 31;
constexpr int MAX_TITLE_LEN = 63;
constexpr int MAX_TAG_LEN = 15;
constexpr int MAX_TEXT_LEN = 31; //!< Text value, Icon sprite and Icon text
constexpr int MAX_REF_LEN = 31; //!< Element select and value
constexpr int MAX_TEMPLATE_LEN = 188;
constexpr int MAX_BUILTIN_NAME_LEN = 14; //!< Name of a built-in value after '@'
constexpr int MAX_PLACEHOLDERS = 16; //!< Placeholders in one template
constexpr int MAX_CUT = 255; //!< Largest cut format of a placeholder
constexpr int MAX_EVAL_LEN = 255; //!< Evaluated text, in UTF-16 units

//! Id that means "every column of the target" (Values, only with E_ValueType::None)
constexpr int ALL_COLUMNS = 0xFF;

//! Column host and element parent when there is none
constexpr int NO_ID = 0xFF;

constexpr int SORT_ASC = 0x80;
constexpr int SORT_PRIORITY_MASK = 0x7F;

//----------------------------------------------------------------------------------------------------
// Targets of Values (4.3)
//----------------------------------------------------------------------------------------------------
constexpr int TARGET_GLOBAL = 0x00;
constexpr int TARGET_PLAYER_FIRST = 0x01;
constexpr int TARGET_PLAYER_LAST = 0x40;
constexpr int TARGET_TEAM_BASE = 0x80; //!< Team number N is TARGET_TEAM_BASE + N
constexpr int TARGET_TEAM_FIRST = 0x81;
constexpr int TARGET_TEAM_LAST = 0xC0;

//----------------------------------------------------------------------------------------------------
// Element positions (4.3, 8.4)
//----------------------------------------------------------------------------------------------------
constexpr int POS_SCALE = 8192; //!< x and y are in 1/8192 of the screen size
constexpr int POS_CENTER = -8192; //!< x or y exactly at the center

/**
 * Converts a set_hudmessage coordinate to the wire value:
 * (int)floor(f * 8192 + 0.5), clamped to -32768..32767. 0.025 gives 205, -1.0 gives POS_CENTER.
 */
inline int PositionToWire(float f)
{
	if (std::isnan(f))
		return 0;

	return static_cast<int>(std::clamp(std::floor(static_cast<double>(f) * POS_SCALE + 0.5), -32768.0, 32767.0));
}

//----------------------------------------------------------------------------------------------------
// Opcodes (4.3)
//----------------------------------------------------------------------------------------------------
enum class E_Op : unsigned int
{
	Reset = 0,
	Config = 1,
	Column = 2,
	Values = 3,
	List = 4,
	Element = 5,
	Template = 6,
	HudState = 7,
	Event = 8, //!< A line of a Feed element; not state, never replayed
};

//----------------------------------------------------------------------------------------------------
// Values (4.3)
//----------------------------------------------------------------------------------------------------
enum class E_ValueType : unsigned int
{
	None = 0,
	Int = 1,
	Text = 2,
	Icon = 3,
	Progress = 4,
	Clock = 5,
};

constexpr int CLOCK_VALUE_LEN = 5; //!< long ms, byte flags

//! Flags byte of a Clock value
enum class E_ClockFlags : unsigned int
{
	None = 0,
	Run = (1 << 0),
	Down = (1 << 1), //!< Counts down to 0 and stops there
	All = Run | Down,
};

//----------------------------------------------------------------------------------------------------
// Config (4.3)
//----------------------------------------------------------------------------------------------------
enum class E_ConfigFlags : unsigned int
{
	None = 0,
	HideFrags = (1 << 0),
	HideDeaths = (1 << 1),
	HideEff = (1 << 2),
	HideSteamID = (1 << 3),
	All = HideFrags | HideDeaths | HideEff | HideSteamID,
};

//----------------------------------------------------------------------------------------------------
// Columns (4.3)
//----------------------------------------------------------------------------------------------------
enum class E_Display : unsigned int
{
	Text = 0,
	Icon = 1,
	Meter = 2,
	Builtin = 3,
	Steps = 4,
	Badge = 5, //!< Drawn after the cells of the host column, takes no width
};

enum class E_ColumnFlags : unsigned int
{
	None = 0,
	Hidden = (1 << 0),
	AlignCenter = (1 << 1),
	AlignRight = (2 << 1),
	AlignMask = (3 << 1), //!< 3 is treated as left
	Primary = (1 << 3),
	MarkLast = (1 << 4), //!< Steps: the last step has its own color
	All = Hidden | AlignMask | Primary | MarkLast,
};

//! Default widths of custom columns with width 0, proportional units
constexpr int DEFAULT_WIDTH_TEXT = 60;
constexpr int DEFAULT_WIDTH_ICON = 100;
constexpr int DEFAULT_WIDTH_METER = 90;
constexpr int DEFAULT_WIDTH_STEPS = 60;

//! Width of a column whose width is 0
inline int GetDefaultWidth(E_Display display)
{
	return display == E_Display::Icon ? DEFAULT_WIDTH_ICON : display == E_Display::Meter ? DEFAULT_WIDTH_METER : display == E_Display::Steps ? DEFAULT_WIDTH_STEPS : DEFAULT_WIDTH_TEXT;
}

//----------------------------------------------------------------------------------------------------
// HUD elements (4.3, 8)
//----------------------------------------------------------------------------------------------------
enum class E_ElementKind : unsigned int
{
	None = 0, //!< Removes the element. Unknown kinds remove it too.
	Text = 1,
	Meter = 2,
	Strip = 3,
	Icon = 4,
	Board = 5, //!< Rows of the scoreboard order or of a list
	Feed = 6, //!< Lines of Event messages
	Box = 7, //!< Backdrop of the group of its parent
};

inline bool IsKnownKind(int kind)
{
	return kind >= static_cast<int>(E_ElementKind::Text) && kind <= static_cast<int>(E_ElementKind::Box);
}

//! Color slots of an element: variants 0-7, then these (4.3, 8.5)
constexpr int SLOT_WARN = 8; //!< Color while warn_at holds
constexpr int SLOT_ACCENT = 9; //!< Accent bar of a Box
constexpr int MAX_COLOR_SLOTS = 10;

//! Warn color when the element has none in SLOT_WARN
inline constexpr unsigned char DEFAULT_WARN_COLOR[4] = { 255, 64, 32, 255 };

enum class E_ElementFlags : unsigned int
{
	None = 0,
	HudColor = (1 << 0), //!< rgb follows hud_color, only alpha of the message is used
	Subject = (1 << 1), //!< Hidden when there is no subject
	NoScoreboard = (1 << 2), //!< Hidden while the scoreboard is open
	Leader = (1 << 3), //!< The subject is the first row of the scoreboard order
	All = HudColor | Subject | NoScoreboard | Leader,
};

//! What the position of an element is relative to. Unknown values are Screen.
enum class E_Place : unsigned int
{
	Screen = 0,
	Parent = 1,
	NumbersRow = 2,
	BannerTop = 3,
	BannerBottom = 4,
	ParentLeft = 5,
	ParentRight = 6,
};

inline bool IsParentPlace(E_Place place)
{
	return place == E_Place::Parent || place == E_Place::ParentLeft || place == E_Place::ParentRight;
}

inline bool IsBannerPlace(E_Place place)
{
	return place == E_Place::BannerTop || place == E_Place::BannerBottom;
}

//! Anchors in the align byte: bits 0-1 horizontal, bits 2-3 vertical
constexpr int ANCHOR_AUTO = 0;
constexpr int ANCHOR_START = 1; //!< Left or top edge
constexpr int ANCHOR_CENTER = 2;
constexpr int ANCHOR_END = 3; //!< Right or bottom edge

constexpr int ALIGN_MASK = 0x0F;

//! Font of an element (8.5). Unknown ids are Text.
enum class E_Font : unsigned int
{
	Text = 0,
	Large = 1,
	Small = 2,
	Huge = 3,
};

constexpr int FONT_COUNT = 4;

//! Names of the fonts in ClientSchemeBase.res, indexed by E_Font
inline constexpr const char *FONT_NAMES[FONT_COUNT] = { "HudServerText", "HudServerLarge", "HudServerSmall", "HudServerHuge" };

//! Kind params of elements (8.7-8.9)
enum class E_Param : unsigned int
{
	// Meter
	Dots = 1,
	DotSize = 2,
	DotGap = 3,
	BarWide = 4,
	Counter = 5,
	CounterGap = 6,
	AlphaEmpty = 7,

	// Strip
	CellWide = 10,
	CellMinWide = 11,
	CellTall = 12,
	CellGap = 13,
	IconPadding = 14,
	MaxWide = 15,
	AlphaPassed = 16,
	AlphaCurrent = 17,
	AlphaFuture = 18,
	AlphaIconFuture = 19,
	Frame = 20,

	// Icon
	IconTall = 30,
	IconGap = 31,

	// Every kind (8.13). Ids from 32 on are sent after all older ids.
	FlashMs = 32,
	FadeMs = 33,
	HoldMs = 34,
	WarnAt = 35,
	WarnPulseMs = 36,

	// Meter
	Form = 40,

	// Strip
	Track = 41,

	// Board
	Rows = 44,
	BoardMode = 45,
	RowGap = 46,
	ColumnGap = 47,
	FillAlpha = 48,
	HighlightAlpha = 49,

	// Feed
	Lines = 52,
	LifeMs = 53,
	LineFadeMs = 54,
	Queue = 55,
	LineGap = 56,

	// Box
	AccentWide = 60,
	Radius = 61,
};

constexpr int MAX_PARAM_ID = 63; //!< Param ids fit a 64-bit mask

//! Meter forms (param form)
enum class E_MeterForm : unsigned int
{
	Line = 0, //!< Dots or a bar
	Ring = 1, //!< Dots on a circle
};

//! Rows of a Board (param board_mode)
enum class E_BoardMode : unsigned int
{
	Top = 0, //!< The first rows
	TopAndOwn = 1, //!< The first rows and the own or highlighted row under them
	Window = 2, //!< Rows around the own or highlighted row
};

struct ParamInfo
{
	E_Param id;
	E_ElementKind kind; //!< Params of other kinds are ignored; None means every kind
	const char *pszName; //!< Name in the spec and in HudServerPanel of HudLayoutBase.res
	int iMin;
	int iMax;
	int iDefault;
};

/**
 * Ranges and defaults of the params. Values are clamped to the range on both sides.
 * cell_min_wide is limited to 1-255 here; the layout also keeps it at most cell_wide (8.8).
 */
inline constexpr ParamInfo PARAM_INFO[] = {
	{ E_Param::Dots, E_ElementKind::Meter, "dots", 0, 32, 10 },
	{ E_Param::DotSize, E_ElementKind::Meter, "dot_size", 2, 64, 7 },
	{ E_Param::DotGap, E_ElementKind::Meter, "dot_gap", 0, 64, 3 },
	{ E_Param::BarWide, E_ElementKind::Meter, "bar_wide", 0, 640, 0 },
	{ E_Param::Counter, E_ElementKind::Meter, "counter", 0, 2, 1 },
	{ E_Param::CounterGap, E_ElementKind::Meter, "counter_gap", 0, 64, 6 },
	{ E_Param::AlphaEmpty, E_ElementKind::Meter, "alpha_empty", 0, 255, 80 },

	{ E_Param::CellWide, E_ElementKind::Strip, "cell_wide", 1, 255, 30 },
	{ E_Param::CellMinWide, E_ElementKind::Strip, "cell_min_wide", 1, 255, 14 },
	{ E_Param::CellTall, E_ElementKind::Strip, "cell_tall", 1, 255, 15 },
	{ E_Param::CellGap, E_ElementKind::Strip, "cell_gap", 0, 64, 3 },
	{ E_Param::IconPadding, E_ElementKind::Strip, "icon_padding", 0, 64, 2 },
	{ E_Param::MaxWide, E_ElementKind::Strip, "max_wide", 0, 4096, 300 },
	{ E_Param::AlphaPassed, E_ElementKind::Strip, "alpha_passed", 0, 255, 112 },
	{ E_Param::AlphaCurrent, E_ElementKind::Strip, "alpha_current", 0, 255, 48 },
	{ E_Param::AlphaFuture, E_ElementKind::Strip, "alpha_future", 0, 255, 24 },
	{ E_Param::AlphaIconFuture, E_ElementKind::Strip, "alpha_icon_future", 0, 255, 80 },
	{ E_Param::Frame, E_ElementKind::Strip, "frame", 0, 1, 1 },

	{ E_Param::IconTall, E_ElementKind::Icon, "icon_tall", 0, 255, 0 },
	{ E_Param::IconGap, E_ElementKind::Icon, "icon_gap", 0, 64, 2 },

	{ E_Param::FlashMs, E_ElementKind::None, "flash_ms", 0, 5000, 0 },
	{ E_Param::FadeMs, E_ElementKind::None, "fade_ms", 0, 5000, 0 },
	{ E_Param::HoldMs, E_ElementKind::None, "hold_ms", 0, 30000, 0 },
	{ E_Param::WarnAt, E_ElementKind::None, "warn_at", 0, 32767, 0 },
	{ E_Param::WarnPulseMs, E_ElementKind::None, "warn_pulse_ms", 0, 5000, 0 },

	{ E_Param::Form, E_ElementKind::Meter, "form", 0, 1, 0 },

	{ E_Param::Track, E_ElementKind::Strip, "track", 0, 1, 0 },

	{ E_Param::Rows, E_ElementKind::Board, "rows", 1, 16, 3 },
	{ E_Param::BoardMode, E_ElementKind::Board, "board_mode", 0, 2, 0 },
	{ E_Param::RowGap, E_ElementKind::Board, "row_gap", 0, 64, 2 },
	{ E_Param::ColumnGap, E_ElementKind::Board, "column_gap", 0, 64, 8 },
	{ E_Param::FillAlpha, E_ElementKind::Board, "fill_alpha", 0, 255, 0 },
	{ E_Param::HighlightAlpha, E_ElementKind::Board, "highlight_alpha", 0, 255, 48 },

	{ E_Param::Lines, E_ElementKind::Feed, "lines", 1, 8, 4 },
	{ E_Param::LifeMs, E_ElementKind::Feed, "life_ms", 100, 30000, 5000 },
	{ E_Param::LineFadeMs, E_ElementKind::Feed, "line_fade_ms", 0, 5000, 400 },
	{ E_Param::Queue, E_ElementKind::Feed, "queue", 0, 1, 0 },
	{ E_Param::LineGap, E_ElementKind::Feed, "line_gap", 0, 64, 2 },

	{ E_Param::AccentWide, E_ElementKind::Box, "accent_wide", 0, 64, 0 },
	{ E_Param::Radius, E_ElementKind::Box, "radius", 0, 32, 0 },
};

//! Returns the info of a param of the kind, or nullptr if the kind has no such param
inline const ParamInfo *FindParamInfo(E_ElementKind kind, int id)
{
	for (const ParamInfo &info : PARAM_INFO)
	{
		if (static_cast<int>(info.id) == id)
			return info.kind == kind || info.kind == E_ElementKind::None ? &info : nullptr;
	}

	return nullptr;
}

//! Standard HUD parts that HudState.hide hides (8.12)
enum class E_HudStd : unsigned int
{
	None = 0,
	Timer = (1 << 0), //!< hud_timer modes 1-3
	NextMap = (1 << 1), //!< The hud_nextmap line
	CustomTimers = (1 << 2), //!< Display of customtimer timers, the end sound still plays
	Speedometer = (1 << 3), //!< hud_speedometer in its bottom position
	JumpSpeed = (1 << 4), //!< hud_jumpspeed in its bottom position
	All = Timer | NextMap | CustomTimers | Speedometer | JumpSpeed,
};

//----------------------------------------------------------------------------------------------------
// Keys and refs
//----------------------------------------------------------------------------------------------------

//! Checks a column, list or element key: 1-15 characters of [a-z0-9_]
inline bool IsValidKey(const char *key)
{
	if (!key)
		return false;

	size_t len = strlen(key);
	return len >= 1 && len <= MAX_KEY_LEN && strspn(key, "abcdefghijklmnopqrstuvwxyz0123456789_") == len;
}

/**
 * Checks a ref of Element.select and Element.value (4.3):
 * "[^](key|@name)[:cur|:max|:left]", which is at most 21 bytes, below MAX_REF_LEN; name is 1-14 characters of [a-z].
 * Built-in names are not checked against the known ones. "" is not a ref.
 */
inline bool IsValidRef(const char *ref)
{
	if (!ref)
		return false;

	const char *name = ref[0] == '^' ? ref + 1 : ref;
	const char *format = strchr(name, ':');
	size_t nameLen = format ? static_cast<size_t>(format - name) : strlen(name);

	if (name[0] == '@')
	{
		if (nameLen < 2 || nameLen - 1 > MAX_BUILTIN_NAME_LEN || strspn(name + 1, "abcdefghijklmnopqrstuvwxyz") != nameLen - 1)
			return false;
	}
	else
	{
		if (nameLen < 1 || nameLen > MAX_KEY_LEN || strspn(name, "abcdefghijklmnopqrstuvwxyz0123456789_") != nameLen)
			return false;
	}

	return !format || !strcmp(format + 1, "cur") || !strcmp(format + 1, "max") || !strcmp(format + 1, "left");
}

/**
 * Checks whether the sprite of an Icon value is a sprite file rather than a hud.txt name (4.3):
 * "sprites/<path>.spr", up to MAX_TEXT_LEN bytes of [A-Za-z0-9_./-], without "..".
 */
inline bool IsSpriteFilePath(const char *sprite)
{
	constexpr char PREFIX[] = "sprites/";
	constexpr char SUFFIX[] = ".spr";
	constexpr size_t PREFIX_LEN = sizeof(PREFIX) - 1;
	constexpr size_t SUFFIX_LEN = sizeof(SUFFIX) - 1;
	size_t len = sprite ? strlen(sprite) : 0;

	return len > PREFIX_LEN + SUFFIX_LEN && len <= MAX_TEXT_LEN
	    && !strncmp(sprite, PREFIX, PREFIX_LEN) && !strcmp(sprite + len - SUFFIX_LEN, SUFFIX)
	    && !strstr(sprite, "..")
	    && strspn(sprite, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./-") == len;
}

}

#endif
