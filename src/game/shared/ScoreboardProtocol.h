#ifndef SCOREBOARDPROTOCOL_H
#define SCOREBOARDPROTOCOL_H
#include <cmath>
#include <cstring>
#include <enum_utils.h>

/**
 * Constants of the "BhlScore" user message, shared by the server and the client.
 * See docs/scoreboard-protocol.md for the format. Section numbers below refer to it.
 */
namespace bhl::sb
{

constexpr int PROTOCOL_VERSION = 1;
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
constexpr int MAX_PARAMS = 16; //!< Params in one Element message
constexpr int MAX_DOTS = 10; //!< A scoreboard Meter with max above this is a bar

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

//! Message sizes, including the opcode byte
constexpr int RESET_SIZE = 2;
constexpr int ELEMENT_FIXED_SIZE = 18; //!< Opcode to font: an Element that ends before font is ignored
constexpr int ELEMENT_REMOVAL_SIZE = 3; //!< Element with kind None
constexpr int TEMPLATE_HEADER_SIZE = 3; //!< Opcode, id and variant
constexpr int HUDSTATE_SIZE = 7;

//! Id that means "every column of the target" (Values, only with E_ValueType::None)
constexpr int ALL_COLUMNS = 0xFF;

//! Id that means "every element" (Element, only with E_ElementKind::None)
constexpr int ALL_ELEMENTS = 0xFF;

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

	double value = std::floor(static_cast<double>(f) * POS_SCALE + 0.5);

	if (value < -32768.0)
		return -32768;

	if (value > 32767.0)
		return 32767;

	return static_cast<int>(value);
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
};

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

//! Horizontal anchor: bits 0-1 of the align byte
enum class E_HAlign : unsigned int
{
	Auto = 0,
	Left = 1,
	Center = 2,
	Right = 3,
};

//! Vertical anchor: bits 2-3 of the align byte
enum class E_VAlign : unsigned int
{
	Auto = 0,
	Top = 1,
	Middle = 2,
	Bottom = 3,
};

constexpr int ALIGN_MASK = 0x0F;
constexpr int ALIGN_V_SHIFT = 2;

inline E_HAlign GetHAlign(int align)
{
	return static_cast<E_HAlign>(align & 3);
}

inline E_VAlign GetVAlign(int align)
{
	return static_cast<E_VAlign>((align >> ALIGN_V_SHIFT) & 3);
}

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
};

constexpr int MAX_PARAM_ID = 31; //!< Param ids fit a 32-bit mask

struct ParamInfo
{
	E_Param id;
	E_ElementKind kind; //!< Params of other kinds are ignored
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
};

//! Returns the info of a param id or nullptr for an unknown id
inline const ParamInfo *FindParamInfo(int id)
{
	for (const ParamInfo &info : PARAM_INFO)
	{
		if (static_cast<int>(info.id) == id)
			return &info;
	}

	return nullptr;
}

//! Returns the info of a param of the kind, or nullptr if the kind has no such param
inline const ParamInfo *FindParamInfo(E_ElementKind kind, int id)
{
	const ParamInfo *info = FindParamInfo(id);
	return info && info->kind == kind ? info : nullptr;
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
 * "[^](key|@name)[:cur|:max|:left]", up to MAX_REF_LEN bytes; name is 1-14 characters of [a-z].
 * Built-in names are not checked against the known ones. "" is not a ref.
 */
inline bool IsValidRef(const char *ref)
{
	if (!ref)
		return false;

	size_t len = strlen(ref);

	if (len == 0 || len > MAX_REF_LEN)
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

	if (!format)
		return true;

	format++;
	return !strcmp(format, "cur") || !strcmp(format, "max") || !strcmp(format, "left");
}

}

#endif
