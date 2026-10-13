#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <vector>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include "hud.h"
#include "hud_renderer.h"
#include "triangleapi.h"
#include "hud/server_panel.h"
#include "vgui/client_viewport.h"
#include "vgui/hud_ammo.h"
#include "vgui/hud_battery.h"
#include "vgui/hud_health.h"
#include "vgui/score_panel_cells.h"

//----------------------------------------------------------------------------------------------------
// Layout and effect math (docs/scoreboard-protocol.md, 8.4, 8.7, 8.8, 8.13-8.16). Pixels; sizes are already
// scaled with P(). Times are seconds, durations are the ms of the params.
//----------------------------------------------------------------------------------------------------
namespace
{

//! Anchors, the values of bhl::sb::E_HAlign and bhl::sb::E_VAlign
constexpr int ANCHOR_AUTO = 0;
constexpr int ANCHOR_START = 1; //!< Left or top edge
constexpr int ANCHOR_CENTER = 2;
constexpr int ANCHOR_END = 3; //!< Right or bottom edge

//! Side zones of the numbers row in widths of a digit: offset 10, suit icon 2 and number 3 of the armor
constexpr int NUMBERS_ROW_DIGITS = 15;

//! Length of a Meter bar in dots when the dots param is 0
constexpr int METER_BAR_DOTS = 10;

constexpr double PI = 3.14159265358979323846;

//! FNV-1a of the content signature
constexpr unsigned int HASH_BASIS = 2166136261u;
constexpr unsigned int HASH_PRIME = 16777619u;

struct Box
{
	int x = 0, y = 0;
	int wide = 0, tall = 0;
};

//! Row of the HL health, armor and ammo numbers; the side zones add the gap of an element
struct NumbersRow
{
	int iTop = 0;
	int iTall = 0;
	int iLeft = 0; //!< Right edge of the health and armor numbers
	int iRight = 0; //!< Left edge of the ammo numbers

	//! Width between the side zones
	int GetFree(int gap) const { return (iRight - gap) - (iLeft + gap); }
};

//! Where the position of an element comes from
struct PlaceInput
{
	bhl::sb::E_Place place = bhl::sb::E_Place::Screen;
	int x = 0, y = 0; //!< Wire positions, 1/8192 of the screen
	int hAnchor = ANCHOR_AUTO;
	int vAnchor = ANCHOR_AUTO;
	int gap = 0;
};

struct LayoutNode
{
	bool bValid = false;
	bhl::sb::E_Place place = bhl::sb::E_Place::Screen;
	int iParent = -1;
};

enum class E_LayoutVisit
{
	New,
	Active,
	Placed,
	Unplaced,
};

struct MeterLayout
{
	bool bDots = false;
	int iWide = 0;
	int iFillWide = 0; //!< 0 when cur is 0
};

struct StripLayout
{
	int iCellWide = 0;
	int iShown = 0; //!< 0 without cells
	int iFirst = 0;
	int iWide = 0;
};

struct RingLayout
{
	int iDots = 0;
	int iFilled = 0;
	int iSize = 0; //!< Width and height of the ring
};

//! Resolves the auto anchor by the position, as set_hudmessage does
int GetAnchor(int pos, int anchor)
{
	if (anchor != ANCHOR_AUTO)
		return anchor;

	if (pos == bhl::sb::POS_CENTER)
		return ANCHOR_CENTER;

	return pos < 0 ? ANCHOR_END : ANCHOR_START;
}

//! Horizontal anchor of a placed box: the side of the box that stays when its width changes
int GetBoxAnchor(const PlaceInput &in)
{
	if (in.place == bhl::sb::E_Place::ParentLeft)
		return ANCHOR_END;

	if (in.place == bhl::sb::E_Place::ParentRight)
		return ANCHOR_START;

	return GetAnchor(in.x, in.hAnchor);
}

//! Start of a box on an axis: the reference point of the position with the box's anchor on it
int PlaceOnAxis(int pos, int anchor, int screenSize, int size)
{
	int ref;

	if (pos == bhl::sb::POS_CENTER)
		ref = screenSize / 2;
	else if (pos < 0)
		ref = static_cast<int>((1.0 + static_cast<double>(pos) / bhl::sb::POS_SCALE) * screenSize);
	else
		ref = static_cast<int>(static_cast<double>(pos) / bhl::sb::POS_SCALE * screenSize);

	switch (GetAnchor(pos, anchor))
	{
	case ANCHOR_CENTER:
		return ref - size / 2;
	case ANCHOR_END:
		return ref - size;
	default:
		return ref;
	}
}

//! Moves a box inside the screen like set_hudmessage: the start wins if it doesn't fit
int MoveInside(int pos, int size, int screenSize)
{
	if (pos + size > screenSize)
		pos = screenSize - size;

	return std::max(pos, 0);
}

/**
 * Places a box of box.wide x box.tall and moves it inside the screen.
 * @param	parent	Placed box of the parent, used by the parent places
 */
void PlaceBox(const PlaceInput &in, const Box &parent, const NumbersRow &row, int screenWide, int screenTall, Box &box)
{
	switch (in.place)
	{
	case bhl::sb::E_Place::ParentLeft:
	case bhl::sb::E_Place::ParentRight:
		box.x = in.place == bhl::sb::E_Place::ParentLeft ? parent.x - in.gap - box.wide : parent.x + parent.wide + in.gap;
		box.y = parent.y + (parent.tall - box.tall) / 2;
		break;
	case bhl::sb::E_Place::Parent:
		box.x = PlaceOnAxis(in.x, in.hAnchor, screenWide, box.wide);
		box.y = in.vAnchor == ANCHOR_END ? parent.y - in.gap - box.tall : parent.y + parent.tall + in.gap;
		break;
	case bhl::sb::E_Place::NumbersRow:
	{
		// The zones are tested where the box will be drawn
		box.x = MoveInside(PlaceOnAxis(in.x, in.hAnchor, screenWide, box.wide), box.wide, screenWide);
		bool lifted = box.x < row.iLeft + in.gap || box.x + box.wide > row.iRight - in.gap;
		box.y = lifted ? row.iTop - in.gap - box.tall : row.iTop + (row.iTall - box.tall) / 2;
		break;
	}
	default:
		box.x = PlaceOnAxis(in.x, in.hAnchor, screenWide, box.wide);
		box.y = PlaceOnAxis(in.y, in.vAnchor, screenTall, box.tall);
		break;
	}

	box.x = MoveInside(box.x, box.wide, screenWide);
	box.y = MoveInside(box.y, box.tall, screenTall);
}

bool VisitLayoutNode(const LayoutNode *nodes, E_LayoutVisit *visits, int id, int *order, int &count)
{
	if (visits[id] != E_LayoutVisit::New)
		return visits[id] == E_LayoutVisit::Placed;

	const LayoutNode &node = nodes[id];
	bool placed = node.bValid && !bhl::sb::IsBannerPlace(node.place);
	visits[id] = E_LayoutVisit::Active;

	// A parent that is still active is the element itself or closes a cycle
	if (placed && bhl::sb::IsParentPlace(node.place))
		placed = node.iParent >= 0 && node.iParent < bhl::sb::MAX_ELEMENTS && VisitLayoutNode(nodes, visits, node.iParent, order, count);

	visits[id] = placed ? E_LayoutVisit::Placed : E_LayoutVisit::Unplaced;

	if (placed)
		order[count++] = id;

	return placed;
}

/**
 * Orders the elements parents first (8.4). Leaves out the unplaced ones: undefined, banner places,
 * and parent places whose parent is undefined, has a banner place, is the element itself, is in a
 * cycle or is unplaced.
 * @param	nodes	MAX_ELEMENTS nodes by element id
 * @param	order	Receives the ids of the placed elements
 * @return	Number of placed elements
 */
int GetLayoutOrder(const LayoutNode *nodes, int *order)
{
	E_LayoutVisit visits[bhl::sb::MAX_ELEMENTS] = {};
	int count = 0;

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
		VisitLayoutNode(nodes, visits, id, order, count);

	return count;
}

/**
 * Layout of a Meter with a Progress value (8.7).
 * @param	cur			Clamped to [0, maxValue]
 * @param	maxValue	Progress max, above 0
 * @param	dot			max(2, P(dot_size))
 * @param	barWide		P(bar_wide), 0 for the length of the dots
 */
MeterLayout LayoutMeter(int cur, int maxValue, int dots, int dot, int dotGap, int barWide)
{
	MeterLayout out;
	out.bDots = dots > 0 && maxValue <= dots;

	if (out.bDots)
	{
		out.iWide = maxValue * dot + (maxValue - 1) * dotGap;
		return out;
	}

	int barDots = dots > 0 ? dots : METER_BAR_DOTS;
	out.iWide = barWide > 0 ? barWide : barDots * dot + (barDots - 1) * dotGap;

	if (cur > 0)
		out.iFillWide = std::max(static_cast<int>(static_cast<long long>(out.iWide) * cur / maxValue), dot);

	return out;
}

/**
 * Layout of a Strip (8.8). Without cells the strip has no width.
 * @param	cur			Current cell [0, count)
 * @param	cellWide	P(cell_wide)
 * @param	cellMinWide	P(cell_min_wide) of cell_min_wide limited to cell_wide
 * @param	maxWide		P(max_wide), or the screen width for 0
 * @param	free		Width between the side zones of the numbers row, used only on it
 * @param	markerWide	Width of the window markers in the element font
 */
StripLayout LayoutStrip(int count, int cur, int cellWide, int cellMinWide, int gap, int maxWide, bool numbersRow, int free, int markerWide)
{
	StripLayout out;

	if (count <= 0)
		return out;

	int cellMin = std::max(1, cellMinWide);
	int cellMax = std::max(cellMin, cellWide);
	int minWide = count * cellMin + (count - 1) * gap;
	bool lifted = numbersRow && minWide > free && free < maxWide;
	int limit = numbersRow && !lifted ? std::min(maxWide, free) : maxWide;

	out.iCellWide = std::clamp((limit - (count - 1) * gap) / count, cellMin, cellMax);
	out.iShown = count;

	if (minWide > limit)
	{
		int marker = markerWide + gap;
		out.iShown = std::clamp((limit - 2 * marker + gap) / std::max(cellMin + gap, 1), 1, count);
		out.iFirst = std::clamp(cur - out.iShown / 2, 0, count - out.iShown);
	}

	out.iWide = out.iShown * out.iCellWide + (out.iShown - 1) * gap;
	return out;
}

/**
 * Layout of a Meter with form 1 (8.7): dots on a circle, a dot per unit while max fits the dots,
 * else the dots param (10 for 0) and filled = round(cur * dots / max).
 * @param	dot	max(2, P(dot_size))
 */
RingLayout LayoutRing(int cur, int maxValue, int dots, int dot, int dotGap)
{
	RingLayout out;
	out.iDots = maxValue <= dots ? maxValue : (dots > 0 ? dots : METER_BAR_DOTS);
	out.iFilled = out.iDots == maxValue ? cur : static_cast<int>((2LL * cur * out.iDots + maxValue) / (2LL * maxValue));
	out.iSize = std::max(3 * dot, static_cast<int>(std::ceil(out.iDots * (dot + dotGap) / PI)) + dot);
	return out;
}

//! Position of dot i of a ring of size x size in the box of the ring: dot 0 on top, clockwise
void GetRingDot(int size, int dot, int i, int count, int &x, int &y)
{
	double angle = -PI / 2 + 2 * PI * i / count;
	double center = (size - dot) / 2.0;
	x = static_cast<int>(std::floor(center + center * std::cos(angle) + 0.5));
	y = static_cast<int>(std::floor(center + center * std::sin(angle) + 0.5));
}

/**
 * Box of a track marker above a cell of a Strip (8.8): n discs for n = 1 or 2, else a text of textWide.
 * @param	gap	P(2): between the discs, and between the marker and the cell
 */
Box LayoutTrackMarker(int cellX, int cellWide, int cellY, int n, int disc, int gap, int textWide, int textTall)
{
	Box box;
	box.wide = n <= 2 ? n * disc + (n - 1) * gap : textWide;
	box.tall = n <= 2 ? disc : textTall;
	box.x = cellX + (cellWide - box.wide) / 2;
	box.y = cellY - gap - box.tall;
	return box;
}

/**
 * Rows of a Board (8.14) out of count in the order: board_mode 0 the first rows; 1 also the own row after
 * a separator (-1) when it isn't among them; 2 a window around the own row, clamped to the ends.
 * @param	own		Own row or -1
 * @param	out		Receives up to rows + 2 row numbers, -1 for the separator
 * @return	Number of rows written
 */
int SelectBoardRows(int count, int own, int rows, int mode, int *out)
{
	int shown = std::min(rows, count);
	int first = 0;
	int n = 0;

	if (mode == static_cast<int>(bhl::sb::E_BoardMode::Window) && own >= 0)
		first = std::clamp(own - shown / 2, 0, count - shown);

	for (int i = first; i < first + shown; i++)
		out[n++] = i;

	if (mode == static_cast<int>(bhl::sb::E_BoardMode::TopAndOwn) && own >= shown)
	{
		out[n++] = -1;
		out[n++] = own;
	}

	return n;
}

/**
 * Columns of a Board: separated by gap, with the icon column of iconWide (0 for none) and a gap before the last one.
 * @param	x	Receives the offsets of the columns
 * @return	Width of a row
 */
int LayoutBoardColumns(int count, const int *wide, int iconWide, int gap, int *x, int &iconX)
{
	int pos = 0;
	iconX = 0;

	for (int i = 0; i < count; i++)
	{
		if (i == count - 1 && iconWide > 0)
		{
			iconX = pos;
			pos += iconWide + gap;
		}

		x[i] = pos;
		pos += wide[i] + (i < count - 1 ? gap : 0);
	}

	return pos;
}

/**
 * Splits text at TAB characters into up to maxColumns NUL-terminated columns; later TABs become spaces.
 * @return	Number of columns, at least 1
 */
int SplitColumns(wchar_t *text, int *starts, int maxColumns)
{
	int count = 1;
	starts[0] = 0;

	for (int i = 0; text[i]; i++)
	{
		if (text[i] != L'\t')
			continue;

		if (count < maxColumns)
		{
			text[i] = L'\0';
			starts[count++] = i + 1;
		}
		else
		{
			text[i] = L' ';
		}
	}

	return count;
}

//! True if text has only digits, spaces and + - . , / : % (a right-aligned Board column)
bool IsNumericText(const wchar_t *text)
{
	return wcsspn(text, L"0123456789 +-.,/:%") == wcslen(text);
}

//! Part of a Board row that its fill bar covers: value / maxValue, 0 for value <= 0
float GetFillFraction(int value, int maxValue)
{
	return value > 0 && maxValue > 0 ? std::min(static_cast<float>(value) / maxValue, 1.0f) : 0.0f;
}

//! Height of count rows of tall pixels with gap between them
int GetStackTall(int count, int tall, int gap)
{
	return count > 0 ? count * tall + (count - 1) * gap : 0;
}

//! Offset of a line of wide pixels in a box of boxWide by the horizontal anchor of the box
int AlignInBox(int anchor, int boxWide, int wide)
{
	if (anchor == ANCHOR_END)
		return boxWide - wide;

	if (anchor == ANCHOR_CENTER)
		return (boxWide - wide) / 2;

	return 0;
}

//! True if the parent chain of element id reaches root, id included
bool IsInGroup(const LayoutNode *nodes, int id, int root)
{
	for (int i = 0; i < bhl::sb::MAX_ELEMENTS && id >= 0 && id < bhl::sb::MAX_ELEMENTS; i++)
	{
		if (id == root)
			return true;

		if (!nodes[id].bValid || !bhl::sb::IsParentPlace(nodes[id].place))
			return false;

		id = nodes[id].iParent;
	}

	return false;
}

//! Adds a box to bounds (x0, y0, x1, y1); an empty box adds nothing
void AddToBounds(int *bounds, int x, int y, int wide, int tall)
{
	if (wide <= 0 || tall <= 0)
		return;

	bounds[0] = std::min(bounds[0], x);
	bounds[1] = std::min(bounds[1], y);
	bounds[2] = std::max(bounds[2], x + wide);
	bounds[3] = std::max(bounds[3], y + tall);
}

/**
 * fade_ms factor (8.13): from `from` toward 1 (fadeIn) or 0 at 1 / fadeMs per ms; without fade_ms 1 or 0 at once.
 * @param	elapsed	Time since the direction was set
 */
float GetFadeFactor(float from, bool fadeIn, float elapsed, int fadeMs)
{
	if (fadeMs <= 0)
		return fadeIn ? 1.0f : 0.0f;

	float step = std::max(elapsed, 0.0f) * 1000.0f / fadeMs;
	return fadeIn ? std::min(from + step, 1.0f) : std::max(from - step, 0.0f);
}

//! flash_ms factor: 1 (white) at the change, down to 0 over flashMs
float GetFlashFactor(float elapsed, int flashMs)
{
	if (flashMs <= 0 || elapsed < 0)
		return 0.0f;

	return std::max(1.0f - elapsed * 1000.0f / flashMs, 0.0f);
}

//! A color component flashed toward white by k
int FlashComponent(int value, float k)
{
	return k > 0 ? value + static_cast<int>((255 - value) * k + 0.5f) : value;
}

//! warn_pulse_ms factor of the alpha while warning: 1 at the start of the warning
float GetPulseFactor(float elapsed, int periodMs)
{
	if (periodMs <= 0)
		return 1.0f;

	return 0.55f + 0.45f * static_cast<float>(std::cos(2 * PI * elapsed * 1000.0 / periodMs));
}

//! True while hold_ms keeps the element drawn after its appearance or change; always without hold_ms
bool IsHoldRunning(float elapsed, int holdMs)
{
	return holdMs <= 0 || elapsed * 1000.0f < holdMs;
}

//! Alpha of a Feed line (8.15): fades in over fadeMs from its start and out over the last fadeMs of its life
float GetLineAlpha(float age, float life, int fadeMs)
{
	if (age < 0 || age >= life)
		return 0.0f;

	if (fadeMs <= 0)
		return 1.0f;

	float fade = fadeMs / 1000.0f;
	return std::min({ 1.0f, age / fade, (life - age) / fade });
}

/**
 * The line an announcer Feed (queue 1) shows: lines in arrival order, each from max(arrival, end of the
 * previous one) for its life. anchorSeq, anchorStart and anchorEnd keep the last started line (all 0 for none),
 * so evicting older lines from the ring doesn't move the later ones: lines before it are skipped.
 * @param	seq	Sequence numbers of the lines, growing
 * @return	Line index or -1; anchorStart is then its start
 */
int FindAnnouncerLine(const int *seq, const float *arrival, const float *life, int count, float now, int &anchorSeq, float &anchorStart, float &anchorEnd)
{
	float end = anchorEnd;

	for (int i = 0; i < count; i++)
	{
		if (seq[i] < anchorSeq)
			continue;

		float lineStart = seq[i] == anchorSeq ? anchorStart : std::max(arrival[i], end);
		end = lineStart + life[i];

		if (lineStart > now)
			break;

		anchorSeq = seq[i];
		anchorStart = lineStart;
		anchorEnd = end;

		if (now < end)
			return i;
	}

	return -1;
}

unsigned int HashInt(unsigned int hash, int value)
{
	for (int i = 0; i < 4; i++)
		hash = (hash ^ ((static_cast<unsigned int>(value) >> (8 * i)) & 0xFF)) * HASH_PRIME;

	return hash;
}

unsigned int HashText(unsigned int hash, const wchar_t *text)
{
	for (; *text; text++)
		hash = HashInt(hash, static_cast<int>(*text));

	return HashInt(hash, 0);
}

}

//----------------------------------------------------------------------------------------------------
// Drawing helpers
//----------------------------------------------------------------------------------------------------
namespace
{

constexpr int CIRCLE_TEXTURE_SIZE = 64;

constexpr wchar_t MARKER_LEFT[] = L"\u2039";
constexpr wchar_t MARKER_RIGHT[] = L"\u203A";
constexpr wchar_t BOARD_SEPARATOR = 0x2026;
constexpr wchar_t TRACK_FORMAT[] = L"\u00D7%d";

//! Color slots of the template variants
constexpr unsigned int VARIANT_SLOTS = (1u << bhl::sb::MAX_VARIANTS) - 1;

//! P(v): proportional units of a 640x480 screen to pixels
int Scaled(int value)
{
	return vgui2::scheme()->GetProportionalScaledValue(value);
}

int GetTextWide(vgui2::HFont font, const wchar_t *text)
{
	int wide = 0, tall = 0;
	vgui2::surface()->GetTextSize(font, text, wide, tall);
	return wide;
}

/**
 * The numbers row in coordinates of a panel at originX, originY (8.4): the classic numbers,
 * or the health, battery and ammo panels with hud_custom 1.
 */
NumbersRow GetNumbersRow(int originX, int originY, int screenWide, int screenTall)
{
	NumbersRow row;

	if (hud_custom.GetBool())
	{
		int hx, hy, hw, ht, bx, by, bw, bt, ax, ay, aw, at;
		g_pViewport->GetHealthPanel()->GetBounds(hx, hy, hw, ht);
		g_pViewport->GetBatteryPanel()->GetBounds(bx, by, bw, bt);
		g_pViewport->GetAmmoPanel()->GetBounds(ax, ay, aw, at);

		int top = std::min({ hy, by, ay });
		row.iTop = top - originY;
		row.iTall = std::max({ hy + ht, by + bt, ay + at }) - top;
		row.iLeft = std::max(hx + hw, bx + bw) - originX;
		row.iRight = ax - originX;
		return row;
	}

	int digitWide = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).GetWidth();
	row.iTall = gHUD.m_iFontHeight;
	row.iTop = screenTall - row.iTall - row.iTall / 2;
	row.iLeft = NUMBERS_ROW_DIGITS * digitWide;
	row.iRight = screenWide - NUMBERS_ROW_DIGITS * digitWide;
	return row;
}

//! Creates a white anti-aliased disc or ring texture from columns [x0; x1) of the circle
int CreateCircleTexture(bool ring, int x0, int x1)
{
	constexpr float radius = CIRCLE_TEXTURE_SIZE / 2.0f;
	constexpr float ringWidth = CIRCLE_TEXTURE_SIZE / 6.0f;
	int wide = x1 - x0;
	std::vector<unsigned char> rgba(wide * CIRCLE_TEXTURE_SIZE * 4, 255);

	for (int y = 0; y < CIRCLE_TEXTURE_SIZE; y++)
	{
		for (int x = x0; x < x1; x++)
		{
			float dist = std::hypot(x + 0.5f - radius, y + 0.5f - radius);
			float alpha = clamp(radius - dist, 0.0f, 1.0f);

			if (ring)
				alpha *= clamp(dist - (radius - ringWidth), 0.0f, 1.0f);

			rgba[(y * wide + x - x0) * 4 + 3] = static_cast<unsigned char>(alpha * 255);
		}
	}

	int id = vgui2::surface()->CreateNewTextureID(true);
	vgui2::surface()->DrawSetTextureRGBA(id, rgba.data(), wide, CIRCLE_TEXTURE_SIZE, true, false);
	return id;
}

void DrawTexture(int id, int x, int y, int wide, int tall, Color color)
{
	vgui2::surface()->DrawSetTexture(id);
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawTexturedRect(x, y, x + wide, y + tall);
}

/**
 * DrawScoreSprite for the panel. With OpenGL, CHudRenderer leaves blending off after a sprite,
 * so it is turned back on as VGUI2 sets it for the shapes drawn after the sprite.
 */
void DrawPanelSprite(int sprite, int x, int y, int wide, int tall, Color color)
{
	DrawScoreSprite(sprite, x, y, wide, tall, color);

	if (CHudRenderer::Get().IsAvailable())
		gEngfuncs.pTriAPI->RenderMode(kRenderTransTexture);
}

Color ToColor(const unsigned char *rgba)
{
	return Color(rgba[0], rgba[1], rgba[2], rgba[3]);
}

Color FlashColor(Color color, float k)
{
	return Color(FlashComponent(color.r(), k), FlashComponent(color.g(), k), FlashComponent(color.b(), k), color.a());
}

int ToPartAlpha(float alpha)
{
	return static_cast<int>(alpha * 255 + 0.5f);
}

}

DEFINE_HUD_ELEM(CHudServerPanel);

CHudServerPanel::CHudServerPanel()
    : vgui2::Panel(NULL, "HudServerPanel")
{
	SetParent(g_pViewport);
	SetPaintBackgroundEnabled(true);
	SetMouseInputEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetVisible(false);

	for (const bhl::sb::ParamInfo &info : bhl::sb::PARAM_INFO)
		m_iParamDefaults[static_cast<int>(info.id)] = info.iDefault;
}

void CHudServerPanel::Init()
{
	m_iFlags |= HUD_ACTIVE;
}

void CHudServerPanel::VidInit()
{
	m_iStripSpriteSerial = -1;
	ResetScoreSpriteFiles();

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		m_szIconSprites[id][0] = '\0';
		m_iIconSprites[id] = -1;
		m_Effects[id] = EffectState();
		m_Anchors[id] = AnnouncerAnchor();

		// The rows keep sprite indexes
		m_States[id].bDrawn = false;
		m_pRows[id].reset();
		m_pGhostRows[id].reset();
	}
}

void CHudServerPanel::Think()
{
	bool visible = UpdateLayout();

	if (IsVisible() != visible)
	{
		SetVisible(visible);

		if (visible)
			m_iFlags &= ~HUD_ACTIVE; // Think is called from VGUI while the panel is visible
		else
			m_iFlags |= HUD_ACTIVE; // Think from HUD to update visibility
	}
}

void CHudServerPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	for (int i = 0; i < bhl::sb::FONT_COUNT; i++)
		m_Fonts[i] = pScheme->GetFont(bhl::sb::FONT_NAMES[i], IsProportional());
}

void CHudServerPanel::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);

	for (const bhl::sb::ParamInfo &info : bhl::sb::PARAM_INFO)
		m_iParamDefaults[static_cast<int>(info.id)] = clamp(inResourceData->GetInt(info.pszName, info.iDefault), info.iMin, info.iMax);
}

bool CHudServerPanel::UpdateLayout()
{
	for (ElementState &state : m_States)
		state.bDrawn = false;

	const CScoreboardData &data = CScoreboardData::Get();
	float time = CScoreboardData::GetTime();

	// No flash for what a Reset or full sync changes, and the announcers start over
	if (m_iResetCount != data.GetResetCount())
	{
		m_iResetCount = data.GetResetCount();

		for (EffectState &fx : m_Effects)
			fx.bHasSignature = false;

		for (AnnouncerAnchor &anchor : m_Anchors)
			anchor = AnnouncerAnchor();
	}

	if (!CScoreboardData::IsHudEnabled() || hud_draw.GetFloat() <= 0 || gHUD.m_iIntermission || (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL))
	{
		StopEffects(true);
		return false;
	}

	if (data.GetVisibleMask() == 0 && !IsFading())
	{
		StopEffects(false);
		return false;
	}

	UpdateStripSprites();
	SaveGhosts();

	LayoutNode nodes[bhl::sb::MAX_ELEMENTS];
	bool isParent[bhl::sb::MAX_ELEMENTS] = {};
	bool isPresent[bhl::sb::MAX_ELEMENTS] = {};

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = data.GetElement(id);
		nodes[id].bValid = el.bValid && el.kind != bhl::sb::E_ElementKind::Box; // A Box is never a parent, it covers its group after the layout
		nodes[id].place = el.place;
		nodes[id].iParent = el.iParent;

		if (el.bValid && bhl::sb::IsParentPlace(el.place) && el.iParent >= 0 && el.iParent < bhl::sb::MAX_ELEMENTS)
			isParent[el.iParent] = true;
	}

	int order[bhl::sb::MAX_ELEMENTS];
	int count = GetLayoutOrder(nodes, order);
	int originX, originY, screenWide, screenTall;
	GetBounds(originX, originY, screenWide, screenTall);

	NumbersRow row = GetNumbersRow(originX, originY, screenWide, screenTall);
	bool isScoreboardOpen = g_pViewport->IsScoreBoardVisible();

	for (int i = 0; i < count; i++)
	{
		int id = order[i];
		const CScoreboardData::Element &el = data.GetElement(id);
		ElementState &state = m_States[id];
		CScoreboardData::Subject subject;

		// The subject is needed even when the element is hidden: a parent's box counts anyway
		bool hasSubject = data.GetElementSubject(id, subject);
		bool isShown = hasSubject && data.IsElementVisible(id) && !(el.HasFlag(bhl::sb::E_ElementFlags::NoScoreboard) && isScoreboardOpen);

		if (!isShown && !isParent[id])
			continue;

		PlaceInput in;
		in.place = el.place;
		in.x = el.x;
		in.y = el.y;
		in.hAnchor = static_cast<int>(el.hAlign);
		in.vAnchor = static_cast<int>(el.vAlign);
		in.gap = Scaled(el.iGap);

		int freeWide = el.place == bhl::sb::E_Place::NumbersRow ? row.GetFree(in.gap) : INT_MAX;
		bool hasContent = MeasureElement(id, el, subject, freeWide, state);

		Box parent;

		if (bhl::sb::IsParentPlace(el.place))
		{
			const ElementState &parentState = m_States[el.iParent];
			parent.x = parentState.x;
			parent.y = parentState.y;
			parent.wide = parentState.wide;
			parent.tall = parentState.tall;
		}

		Box box;
		box.wide = state.wide;
		box.tall = state.tall;
		PlaceBox(in, parent, row, screenWide, screenTall, box);

		state.x = box.x;
		state.y = box.y;
		isPresent[id] = isShown && hasContent;
		PlaceContent(el, GetBoxAnchor(in), state);
	}

	bool isAnyDrawn = false;

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = data.GetElement(id);

		if (el.kind != bhl::sb::E_ElementKind::Box && UpdateEffects(id, el, isPresent[id], time))
			isAnyDrawn = true;
	}

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = data.GetElement(id);

		if (el.kind != bhl::sb::E_ElementKind::Box)
			continue;

		// The group: what the elements under the root draw, ghosts that fade out included
		int bounds[4] = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };
		float groupFade = 0;

		for (int member = 0; member < bhl::sb::MAX_ELEMENTS; member++)
		{
			const ElementState &memberState = m_States[member];

			if (!memberState.bDrawn || memberState.kind == bhl::sb::E_ElementKind::Box || !IsInGroup(nodes, member, el.iParent))
				continue;

			AddDrawnBounds(member, memberState, bounds);
			groupFade = std::max(groupFade, m_Effects[member].flFade);
		}

		// Placed when its root is
		ElementState &state = m_States[id];
		CScoreboardData::Subject subject;
		bool isPlaced = bhl::sb::IsParentPlace(el.place) && std::find(order, order + count, el.iParent) != order + count;
		bool isPresentBox = isPlaced && data.GetElementSubject(id, subject) && data.IsElementVisible(id)
		    && !(el.HasFlag(bhl::sb::E_ElementFlags::NoScoreboard) && isScoreboardOpen) && bounds[0] < bounds[2];

		if (isPresentBox)
		{
			int pad = Scaled(el.iGap);
			MeasureElement(id, el, subject, INT_MAX, state);
			state.x = bounds[0] - pad;
			state.y = bounds[1] - pad;
			state.wide = bounds[2] - bounds[0] + 2 * pad;
			state.tall = bounds[3] - bounds[1] + 2 * pad;
		}

		if (UpdateEffects(id, el, isPresentBox, time))
		{
			state.flAlpha *= groupFade;
			isAnyDrawn = true;
		}
	}

	return isAnyDrawn;
}

bool CHudServerPanel::MeasureElement(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, int freeWide, ElementState &state)
{
	int font = static_cast<int>(el.font);
	Color rgb = el.HasFlag(bhl::sb::E_ElementFlags::HudColor) ? gHUD.GetHudColor(HudPart::Common, 0) : Color(el.r, el.g, el.b, 255);

	state.kind = el.kind;
	state.font = m_Fonts[font < bhl::sb::FONT_COUNT ? font : 0];
	state.iFontTall = vgui2::surface()->GetFontTall(state.font);
	state.color = Color(rgb.r(), rgb.g(), rgb.b(), el.a);
	state.bGhost = false;
	state.flAlpha = 1;
	state.flFlash = 0;
	state.bWarn = false;
	state.wszText[0] = L'\0';
	state.bHasText = false;
	state.iTextWide = 0;
	state.iCur = 0;
	state.iMax = 0;
	state.bRing = false;
	state.bTrack = false;
	state.iSprite = -1;
	state.wide = 0;
	state.tall = state.iFontTall;

	switch (el.kind)
	{
	case bhl::sb::E_ElementKind::Text:
		state.bHasText = CScoreboardData::Get().GetElementText(id, subject, state.wszText, sizeof(state.wszText));
		state.iTextWide = state.bHasText ? GetTextWide(state.font, state.wszText) : 0;
		state.wide = state.iTextWide;
		return state.bHasText;
	case bhl::sb::E_ElementKind::Meter:
		return MeasureMeter(id, el, subject, state);
	case bhl::sb::E_ElementKind::Strip:
		return MeasureStrip(id, el, subject, freeWide, state);
	case bhl::sb::E_ElementKind::Icon:
		return MeasureIcon(id, el, subject, state);
	case bhl::sb::E_ElementKind::Board:
		return MeasureBoard(id, el, subject, state);
	case bhl::sb::E_ElementKind::Feed:
		return MeasureFeed(id, el, state);
	default:
		return false;
	}
}

bool CHudServerPanel::MeasureMeter(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state)
{
	const CScoreboardData &data = CScoreboardData::Get();
	CScoreboardData::Value value;
	int dot = std::max(2, Scaled(GetParam(el, bhl::sb::E_Param::DotSize)));
	state.tall = dot;

	if (!data.GetElementValue(id, subject, value) || value.type != bhl::sb::E_ValueType::Progress || value.iMax <= 0)
		return false;

	state.iMax = value.iMax;
	state.iCur = clamp(value.iValue, 0, value.iMax);
	state.iDotGap = Scaled(GetParam(el, bhl::sb::E_Param::DotGap));

	MeterLayout meter = LayoutMeter(state.iCur, state.iMax, GetParam(el, bhl::sb::E_Param::Dots), dot, state.iDotGap, Scaled(GetParam(el, bhl::sb::E_Param::BarWide)));
	state.bDots = meter.bDots;
	state.wide = meter.iWide;
	state.iFillWide = meter.iFillWide;

	if (GetParam(el, bhl::sb::E_Param::Form) == static_cast<int>(bhl::sb::E_MeterForm::Ring))
	{
		RingLayout ring = LayoutRing(state.iCur, state.iMax, GetParam(el, bhl::sb::E_Param::Dots), dot, state.iDotGap);
		state.bRing = true;
		state.bDots = ring.iDots == state.iMax;
		state.iDotSize = dot;
		state.iRingDots = ring.iDots;
		state.iRingFilled = ring.iFilled;
		state.wide = ring.iSize;
		state.tall = ring.iSize;
	}

	int counter = GetParam(el, bhl::sb::E_Param::Counter);

	if (counter == 2 || (counter == 1 && !state.bDots))
	{
		state.bHasText = data.GetElementText(id, subject, state.wszText, sizeof(state.wszText));
		state.iTextWide = state.bHasText ? GetTextWide(state.font, state.wszText) : 0;
	}

	return true;
}

bool CHudServerPanel::MeasureStrip(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, int freeWide, ElementState &state)
{
	const CScoreboardData &data = CScoreboardData::Get();
	const CScoreboardData::List *list = el.szList[0] ? data.FindList(el.szList) : nullptr;
	CScoreboardData::Value value;
	bool hasValue = data.GetElementValue(id, subject, value);
	int count = 0;

	// A list key without such a list counts as no list (8.8)
	if (list)
		count = list->iCount;
	else if (hasValue && value.type == bhl::sb::E_ValueType::Progress)
		count = value.iMax;

	count = clamp(count, 0, bhl::sb::MAX_LIST_ITEMS);

	int position = 0;
	bool hasPosition = hasValue && value.GetNumber(position);

	state.iMax = count;
	state.iCur = count > 0 ? clamp(position, 1, count) - 1 : 0;
	state.iCellGap = Scaled(GetParam(el, bhl::sb::E_Param::CellGap));
	state.tall = Scaled(GetParam(el, bhl::sb::E_Param::CellTall));

	int cellWide = GetParam(el, bhl::sb::E_Param::CellWide);
	int cellMinWide = std::min(GetParam(el, bhl::sb::E_Param::CellMinWide), cellWide);
	int maxWide = GetParam(el, bhl::sb::E_Param::MaxWide);
	int markerWide = vgui2::surface()->GetCharacterWidth(state.font, MARKER_LEFT[0]);

	int maxWidePx = maxWide > 0 ? Scaled(maxWide) : GetWide();
	StripLayout strip = LayoutStrip(count, state.iCur, Scaled(cellWide), Scaled(cellMinWide), state.iCellGap, maxWidePx, el.place == bhl::sb::E_Place::NumbersRow, freeWide, markerWide);

	state.iCellWide = strip.iCellWide;
	state.iShown = strip.iShown;
	state.iFirst = strip.iFirst;
	state.wide = strip.iWide;
	state.bTrack = count > 0 && GetParam(el, bhl::sb::E_Param::Track) == 1;

	if (state.bTrack)
		UpdateTrack(id, subject, count);

	return count > 0 && hasPosition;
}

bool CHudServerPanel::MeasureIcon(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state)
{
	const CScoreboardData &data = CScoreboardData::Get();
	CScoreboardData::Value value;

	// Without an Icon value the element has no content, not even its text (8.3, 8.9)
	if (!data.GetElementValue(id, subject, value) || value.type != bhl::sb::E_ValueType::Icon)
		return false;

	int iconTall = GetParam(el, bhl::sb::E_Param::IconTall);
	int sprite = GetIconSprite(id, value.szSprite);
	const wrect_t &rc = GetScoreSpriteRect(sprite);

	state.iIconTall = iconTall > 0 ? Scaled(iconTall) : state.iFontTall;

	if (sprite != -1 && rc.GetWidth() > 0 && rc.GetHeight() > 0 && state.iIconTall > 0)
	{
		state.iSprite = sprite;
		state.iIconWide = rc.GetWidth() * state.iIconTall / rc.GetHeight();
	}

	state.bHasText = data.GetElementText(id, subject, state.wszText, sizeof(state.wszText));
	state.iTextWide = state.bHasText ? GetTextWide(state.font, state.wszText) : 0;

	bool hasIcon = state.iSprite != -1;
	int iconWide = hasIcon ? state.iIconWide : 0;
	int gap = hasIcon && state.bHasText ? Scaled(GetParam(el, bhl::sb::E_Param::IconGap)) : 0;

	state.wide = iconWide + gap + state.iTextWide;
	state.tall = std::max(hasIcon ? state.iIconTall : 0, state.iFontTall);
	return hasIcon || state.bHasText;
}

bool CHudServerPanel::MeasureBoard(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state)
{
	const CScoreboardData &data = CScoreboardData::Get();
	bool isList = el.szList[0] != '\0';
	const CScoreboardData::List *list = isList ? data.FindList(el.szList) : nullptr;

	// In list mode @value is the item of the list named by the value ref
	bool hasValueList = isList && el.valueRef.builtin == bhl::sb::E_Builtin::None && el.valueRef.szKey[0];
	const CScoreboardData::List *values = hasValueList ? data.FindList(el.valueRef.szKey) : nullptr;

	CScoreboardData::Subject entities[MAX_PLAYERS];
	int count = 0;
	int own = -1;

	if (isList)
	{
		bhl::sb::TemplateValue select;
		int number = 0;
		count = list ? list->iCount : 0;

		if (subject.IsValid() && data.ResolveRef(el.selectRef, subject, id, select) && select.GetNumber(number) && number >= 1 && number <= count)
			own = number - 1;
	}
	else
	{
		count = data.GetOrderedEntities(entities, MAX_PLAYERS);
		CScoreboardData::Subject ranked = data.GetRankedEntity(subject);

		for (int i = 0; i < count && subject.IsValid(); i++)
		{
			if (entities[i] == ranked)
				own = i;
		}
	}

	RowsState &rows = GetRows(id);
	int picked[MAX_ROWS];
	int maxRows = std::min(GetParam(el, bhl::sb::E_Param::Rows), MAX_ROWS - 2);
	rows.iCount = SelectBoardRows(count, own, maxRows, GetParam(el, bhl::sb::E_Param::BoardMode), picked);
	rows.iOwn = -1;
	rows.iColumns = 1;
	rows.iStep = state.iFontTall + Scaled(GetParam(el, bhl::sb::E_Param::RowGap));

	if (rows.iCount == 0)
		return false;

	const bhl::sb::CTemplate *templates[2] = { data.GetTemplate(id, 0), data.GetTemplate(id, 1) };
	CScoreboardData::Value fills[MAX_ROWS];
	int columnWide[MAX_ROW_COLUMNS] = {};
	bool isTextColumn[MAX_ROW_COLUMNS] = {};
	int iconWide = 0;
	int maxInt = 0;

	for (int i = 0; i < rows.iCount; i++)
	{
		RowState &row = rows.rows[i];
		int index = picked[i];
		row.iVariant = 0;
		row.iSprite = -1;
		row.iIconWide = 0;

		if (index < 0)
		{
			row.wszText[0] = BOARD_SEPARATOR;
			row.wszText[1] = L'\0';
			row.iColumns = 1;
			row.iColumnStart[0] = 0;
			row.iColumnWide[0] = GetTextWide(state.font, row.wszText);
			columnWide[0] = std::max(columnWide[0], row.iColumnWide[0]);
			continue;
		}

		if (index == own)
		{
			rows.iOwn = i;
			row.iVariant = templates[1] ? 1 : 0;
		}

		CScoreboardData::Subject rowSubject = isList ? subject : entities[index];
		CScoreboardData::TemplateExtras extras;
		extras.iIndex = index + 1;

		if (isList)
			extras.pItem = &list->items[index];

		if (hasValueList)
		{
			if (values && index < values->iCount)
				fills[i] = values->items[index];

			CScoreboardData::FormatValue(el.valueRef.format, fills[i]);
			extras.pValue = &fills[i];
		}
		else
		{
			data.GetElementValue(id, rowSubject, fills[i]);
		}

		row.wszText[0] = L'\0';

		if (templates[row.iVariant])
			data.EvaluateTemplate(*templates[row.iVariant], rowSubject, id, row.wszText, sizeof(row.wszText), nullptr, &extras);

		row.iColumns = SplitColumns(row.wszText, row.iColumnStart, MAX_ROW_COLUMNS);
		rows.iColumns = std::max(rows.iColumns, row.iColumns);

		for (int c = 0; c < row.iColumns; c++)
		{
			const wchar_t *text = row.wszText + row.iColumnStart[c];
			row.iColumnWide[c] = GetTextWide(state.font, text);
			columnWide[c] = std::max(columnWide[c], row.iColumnWide[c]);
			isTextColumn[c] = isTextColumn[c] || !IsNumericText(text);
		}

		const CScoreboardData::Value &icon = isList ? list->items[index] : fills[i];

		if (icon.type == bhl::sb::E_ValueType::Icon && icon.szSprite[0])
		{
			SetRowIcon(row, icon.szSprite, state.iFontTall);
			iconWide = std::max(iconWide, row.iIconWide);
		}

		if (fills[i].type == bhl::sb::E_ValueType::Int)
			maxInt = std::max(maxInt, fills[i].iValue);
	}

	for (int i = 0; i < rows.iCount; i++)
	{
		const CScoreboardData::Value &value = fills[i];
		bool isProgress = value.type == bhl::sb::E_ValueType::Progress;
		bool isInt = value.type == bhl::sb::E_ValueType::Int;
		rows.rows[i].flFill = isProgress ? GetFillFraction(value.iValue, value.iMax) : (isInt ? GetFillFraction(value.iValue, maxInt) : 0.0f);
	}

	for (int c = 0; c < MAX_ROW_COLUMNS; c++)
	{
		rows.iColumnWide[c] = columnWide[c];
		rows.bColumnRight[c] = !isTextColumn[c];
	}

	rows.iIconWide = iconWide;
	state.wide = LayoutBoardColumns(rows.iColumns, columnWide, iconWide, Scaled(GetParam(el, bhl::sb::E_Param::ColumnGap)), rows.iColumnX, rows.iIconX);
	state.tall = GetStackTall(rows.iCount, state.iFontTall, rows.iStep - state.iFontTall);
	return true;
}

bool CHudServerPanel::MeasureFeed(int id, const CScoreboardData::Element &el, ElementState &state)
{
	const CScoreboardData &data = CScoreboardData::Get();
	const CScoreboardData::HudLine *lines[CScoreboardData::MAX_HUD_LINES];
	int seqs[CScoreboardData::MAX_HUD_LINES];
	float arrivals[CScoreboardData::MAX_HUD_LINES];
	float lives[CScoreboardData::MAX_HUD_LINES];
	int count = data.GetHudLines(id, lines);
	float time = CScoreboardData::GetTime();
	float life = GetParam(el, bhl::sb::E_Param::LifeMs) / 1000.0f;
	int fadeMs = GetParam(el, bhl::sb::E_Param::LineFadeMs);

	for (int i = 0; i < count; i++)
	{
		seqs[i] = lines[i]->iSeq;
		arrivals[i] = lines[i]->flTime;
		lives[i] = lines[i]->flLife > 0 ? lines[i]->flLife : life;
	}

	int picked[MAX_ROWS];
	float alphas[MAX_ROWS];
	int shown = 0;

	if (GetParam(el, bhl::sb::E_Param::Queue) == 1)
	{
		AnnouncerAnchor &anchor = m_Anchors[id];
		int line = FindAnnouncerLine(seqs, arrivals, lives, count, time, anchor.iSeq, anchor.flStart, anchor.flEnd);

		if (line >= 0)
		{
			picked[shown] = line;
			alphas[shown++] = GetLineAlpha(time - anchor.flStart, lives[line], fadeMs);
		}
	}
	else
	{
		int maxLines = std::min(GetParam(el, bhl::sb::E_Param::Lines), MAX_ROWS);

		for (int i = count - 1; i >= 0 && shown < maxLines; i--)
		{
			float age = time - arrivals[i];

			if (age < lives[i])
			{
				picked[shown] = i;
				alphas[shown++] = GetLineAlpha(age, lives[i], fadeMs);
			}
		}
	}

	if (shown == 0)
		return false;

	RowsState &rows = GetRows(id);
	int iconGap = Scaled(4);
	rows.iCount = shown;
	rows.iStep = state.iFontTall + Scaled(GetParam(el, bhl::sb::E_Param::LineGap));

	for (int i = 0; i < shown; i++)
	{
		const CScoreboardData::HudLine &line = *lines[picked[i]];
		RowState &row = rows.rows[i];
		memcpy(row.wszText, line.wszText, sizeof(row.wszText));
		row.iVariant = line.iVariant;
		row.flAlpha = alphas[i];
		SetRowIcon(row, line.icon.szSprite, state.iFontTall);

		int textWide = GetTextWide(state.font, row.wszText);
		row.iTextX = row.iIconWide > 0 && textWide > 0 ? row.iIconWide + iconGap : row.iIconWide;
		row.iWide = row.iTextX + textWide;
		state.wide = std::max(state.wide, row.iWide);
	}

	state.tall = GetStackTall(shown, state.iFontTall, rows.iStep - state.iFontTall);
	return true;
}

void CHudServerPanel::PlaceContent(const CScoreboardData::Element &el, int anchor, ElementState &state)
{
	state.iTextX = state.x;
	state.iTextY = state.y + (state.tall - state.iFontTall) / 2;
	state.iAnchor = anchor;

	if (state.bRing)
	{
		state.iTextX = state.x + (state.wide - state.iTextWide) / 2;
	}
	else if (el.kind == bhl::sb::E_ElementKind::Meter)
	{
		// The counter stands outside the box, so the anchored edge of the box stays in place
		int gap = Scaled(GetParam(el, bhl::sb::E_Param::CounterGap));
		state.iTextX = anchor == ANCHOR_END ? state.x - gap - state.iTextWide : state.x + state.wide + gap;
	}
	else if (el.kind == bhl::sb::E_ElementKind::Icon && state.iSprite != -1)
	{
		state.iIconX = state.x;
		state.iIconY = state.y + (state.tall - state.iIconTall) / 2;
		state.iTextX = state.x + state.iIconWide + Scaled(GetParam(el, bhl::sb::E_Param::IconGap));
	}
}

void CHudServerPanel::UpdateStripSprites()
{
	const CScoreboardData &data = CScoreboardData::Get();

	if (m_iStripSpriteSerial == data.GetSerial())
		return;

	m_iStripSpriteSerial = data.GetSerial();

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = data.GetElement(id);
		const CScoreboardData::List *list = nullptr;

		if (el.bValid && el.kind == bhl::sb::E_ElementKind::Strip && el.szList[0])
			list = data.FindList(el.szList);

		for (int i = 0; i < bhl::sb::MAX_LIST_ITEMS; i++)
		{
			const CScoreboardData::Value *item = list && i < list->iCount ? &list->items[i] : nullptr;
			bool hasIcon = item && item->type == bhl::sb::E_ValueType::Icon && item->szSprite[0];
			m_iStripSprites[id][i] = hasIcon ? GetScoreSpriteIndex(item->szSprite) : -1;
		}
	}
}

int CHudServerPanel::GetIconSprite(int id, const char *sprite)
{
	if (!sprite[0])
		return -1;

	if (strcmp(m_szIconSprites[id], sprite))
	{
		Q_strncpy(m_szIconSprites[id], sprite, sizeof(m_szIconSprites[id]));
		m_iIconSprites[id] = GetScoreSpriteIndex(sprite);
	}

	return m_iIconSprites[id];
}

void CHudServerPanel::SetRowIcon(RowState &row, const char *sprite, int tall)
{
	if (strcmp(row.szSpriteName, sprite))
	{
		Q_strncpy(row.szSpriteName, sprite, sizeof(row.szSpriteName));
		row.iSpriteOfName = GetScoreSpriteIndex(sprite);
	}

	const wrect_t &rc = GetScoreSpriteRect(row.iSpriteOfName);
	bool isValid = rc.GetWidth() > 0 && rc.GetHeight() > 0;
	row.iSprite = isValid ? row.iSpriteOfName : -1;
	row.iIconWide = isValid ? rc.GetWidth() * tall / rc.GetHeight() : 0;
}

void CHudServerPanel::UpdateTrack(int id, const CScoreboardData::Subject &subject, int count)
{
	const CScoreboardData &data = CScoreboardData::Get();
	unsigned char *counts = m_iTrackCounts[id];
	std::fill(counts, counts + bhl::sb::MAX_LIST_ITEMS, 0);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CScoreboardData::Subject player;
		CScoreboardData::Value value;
		int position = 0;
		player.iPlayer = i;

		if (i == subject.iPlayer || !CScoreboardData::IsParticipant(i) || !data.GetElementValue(id, player, value) || !value.GetNumber(position))
			continue;

		counts[clamp(position, 1, count) - 1]++;
	}
}

CHudServerPanel::RowsState &CHudServerPanel::GetRows(int id)
{
	if (!m_pRows[id])
		m_pRows[id] = std::make_unique<RowsState>();

	return *m_pRows[id];
}

const CHudServerPanel::RowsState &CHudServerPanel::GetPaintRows(int id, const ElementState &state) const
{
	return state.bGhost ? *m_pGhostRows[id] : *m_pRows[id];
}

int CHudServerPanel::GetParam(const CScoreboardData::Element &el, bhl::sb::E_Param param) const
{
	int value;

	if (el.GetParam(param, value))
		return value;

	return m_iParamDefaults[static_cast<int>(param)];
}

void CHudServerPanel::SaveGhosts()
{
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		EffectState &fx = m_Effects[id];

		if (!fx.bDrawn || fx.iFadeMs <= 0)
			continue;

		if (!m_pGhosts[id])
			m_pGhosts[id] = std::make_unique<ElementState>();

		*m_pGhosts[id] = m_States[id];
		fx.bHasGhost = true;

		if (fx.kind == bhl::sb::E_ElementKind::Board || fx.kind == bhl::sb::E_ElementKind::Feed)
		{
			if (!m_pGhostRows[id])
				m_pGhostRows[id] = std::make_unique<RowsState>();

			std::swap(m_pRows[id], m_pGhostRows[id]);
		}
	}
}

void CHudServerPanel::StopEffects(bool keepPresence)
{
	for (EffectState &fx : m_Effects)
	{
		fx.bDrawn = false;
		fx.flFadeFrom = 0;
		fx.flFade = 0;

		if (!keepPresence)
			fx.bPresent = false;
	}
}

bool CHudServerPanel::IsFading() const
{
	for (const EffectState &fx : m_Effects)
	{
		if (fx.iFadeMs > 0 && (fx.bDrawn || fx.flFade > 0))
			return true;
	}

	return false;
}

bool CHudServerPanel::UpdateEffects(int id, const CScoreboardData::Element &el, bool present, float time)
{
	const CScoreboardData &data = CScoreboardData::Get();
	EffectState &fx = m_Effects[id];
	ElementState &state = m_States[id];
	int removals = data.GetElementRemovals(id);

	// Removal and redefinition with another kind are instant: the old element doesn't fade out
	if (fx.kind != el.kind || fx.iRemovals != removals)
	{
		fx = EffectState();
		fx.kind = el.kind;
		fx.iRemovals = removals;
		m_Anchors[id] = AnnouncerAnchor();
	}

	if (!el.bValid)
		return false;

	CScoreboardData::Subject subject;
	data.GetElementSubject(id, subject);

	if (fx.subject != subject)
	{
		fx.subject = subject;
		fx.bPresent = false;
		fx.bHasSignature = false;
		fx.flFlashTime = NEVER;
	}

	int flashMs = GetParam(el, bhl::sb::E_Param::FlashMs);
	int holdMs = GetParam(el, bhl::sb::E_Param::HoldMs);
	bool appeared = present && !fx.bPresent;
	fx.bPresent = present;

	if (appeared)
	{
		fx.flHoldTime = time;
		fx.flFlashTime = NEVER;
		fx.bWarn = false;
	}

	if (present && (flashMs > 0 || holdMs > 0))
	{
		unsigned int signature = GetSignature(id, subject, state);

		// A change while hidden is no flash: the element appears with it
		if (fx.bHasSignature && signature != fx.iSignature && !appeared)
		{
			fx.flHoldTime = time;
			fx.flFlashTime = fx.bDrawn ? time : NEVER;
		}

		fx.iSignature = signature;
		fx.bHasSignature = true;
	}

	if (present)
	{
		int warnAt = GetParam(el, bhl::sb::E_Param::WarnAt);
		CScoreboardData::Value value;
		int number = 0;
		bool warn = warnAt > 0 && data.GetElementValue(id, subject, value) && value.GetNumber(number) && number <= warnAt;

		if (warn && !fx.bWarn)
			fx.flWarnTime = time;

		fx.bWarn = warn;
	}

	bool drawn = present && IsHoldRunning(time - fx.flHoldTime, holdMs);

	if (drawn != fx.bDrawn)
	{
		fx.flFadeFrom = GetFadeFactor(fx.flFadeFrom, fx.bDrawn, time - fx.flFadeTime, fx.iFadeMs);
		fx.flFadeTime = time;
	}

	fx.iFadeMs = GetParam(el, bhl::sb::E_Param::FadeMs);
	fx.flFade = GetFadeFactor(fx.flFadeFrom, drawn, time - fx.flFadeTime, fx.iFadeMs);
	fx.bDrawn = drawn;

	if (drawn)
	{
		// Color order (8.13): element color, variant slot, warn, flash
		unsigned char rgba[4];
		bool hasVariants = state.kind == bhl::sb::E_ElementKind::Text || state.kind == bhl::sb::E_ElementKind::Icon || state.kind == bhl::sb::E_ElementKind::Meter;
		int variant = hasVariants && (el.iColorMask & VARIANT_SLOTS) ? data.GetElementVariant(id, subject) : -1;

		if (variant >= 0 && el.GetSlotColor(variant, rgba))
			state.color = ToColor(rgba);

		if (fx.bWarn)
			state.color = el.GetSlotColor(bhl::sb::SLOT_WARN, rgba) ? ToColor(rgba) : ToColor(bhl::sb::DEFAULT_WARN_COLOR);

		state.bWarn = fx.bWarn;
		state.flFlash = GetFlashFactor(time - fx.flFlashTime, flashMs);

		if (state.flFlash > 0)
			state.color = FlashColor(state.color, state.flFlash);
	}
	else if (fx.flFade > 0 && fx.bHasGhost)
	{
		state = *m_pGhosts[id];
		state.bGhost = true;
	}
	else
	{
		return false;
	}

	float pulse = fx.bWarn ? GetPulseFactor(time - fx.flWarnTime, GetParam(el, bhl::sb::E_Param::WarnPulseMs)) : 1.0f;
	state.flAlpha = fx.flFade * pulse;
	state.bDrawn = true;
	return true;
}

unsigned int CHudServerPanel::GetSignature(int id, const CScoreboardData::Subject &subject, const ElementState &state) const
{
	unsigned int hash = HASH_BASIS;

	switch (state.kind)
	{
	case bhl::sb::E_ElementKind::Text:
		hash = HashInt(hash, CScoreboardData::Get().GetElementVariant(id, subject));
		break;
	case bhl::sb::E_ElementKind::Icon:
		hash = HashInt(hash, state.iSprite);
		break;
	case bhl::sb::E_ElementKind::Meter:
	case bhl::sb::E_ElementKind::Strip:
		hash = HashInt(HashInt(hash, state.iCur), state.iMax);
		break;
	case bhl::sb::E_ElementKind::Board:
	{
		const RowsState &rows = *m_pRows[id];

		for (int i = 0; i < rows.iCount; i++)
		{
			const RowState &row = rows.rows[i];

			for (int c = 0; c < row.iColumns; c++)
				hash = HashText(HashInt(hash, c), row.wszText + row.iColumnStart[c]);
		}

		break;
	}
	default:
		break;
	}

	return state.bHasText ? HashText(hash, state.wszText) : hash;
}

void CHudServerPanel::AddDrawnBounds(int id, const ElementState &state, int *bounds) const
{
	AddToBounds(bounds, state.x, state.y, state.wide, state.tall);

	if (state.kind == bhl::sb::E_ElementKind::Meter && state.bHasText)
		AddToBounds(bounds, state.iTextX, state.iTextY, state.iTextWide, state.iFontTall);

	if (state.kind != bhl::sb::E_ElementKind::Strip)
		return;

	if (state.iFirst > 0)
	{
		int markerWide = vgui2::surface()->GetCharacterWidth(state.font, MARKER_LEFT[0]);
		AddToBounds(bounds, state.x - state.iCellGap - markerWide, state.iTextY, markerWide, state.iFontTall);
	}

	if (state.iFirst + state.iShown < state.iMax)
	{
		int markerWide = vgui2::surface()->GetCharacterWidth(state.font, MARKER_RIGHT[0]);
		AddToBounds(bounds, state.x + state.wide + state.iCellGap, state.iTextY, markerWide, state.iFontTall);
	}

	for (int i = 0; state.bTrack && i < state.iShown; i++)
	{
		int n = m_iTrackCounts[id][state.iFirst + i];
		wchar_t text[16];

		if (n <= 0)
			continue;

		V_snwprintf(text, std::size(text), TRACK_FORMAT, n);
		Box box = LayoutTrackMarker(state.x + i * (state.iCellWide + state.iCellGap), state.iCellWide, state.y, n, std::max(2, Scaled(4)), Scaled(2), GetTextWide(state.font, text), state.iFontTall);
		AddToBounds(bounds, box.x, box.y, box.wide, box.tall);
	}
}

Color CHudServerPanel::GetColor(const ElementState &state, int partAlpha) const
{
	return GetColor(state, state.color, partAlpha);
}

Color CHudServerPanel::GetColor(const ElementState &state, Color color, int partAlpha) const
{
	float alpha = clamp(partAlpha, 0, 255) * color.a() / 255.0f * gHUD.GetHudTransparency() * state.flAlpha;
	return Color(color.r(), color.g(), color.b(), static_cast<int>(alpha));
}

Color CHudServerPanel::GetRowColor(const ElementState &state, const CScoreboardData::Element &el, int variant, int partAlpha) const
{
	unsigned char rgba[4];

	if (state.bWarn || !el.GetSlotColor(variant, rgba))
		return GetColor(state, partAlpha);

	return GetColor(state, FlashColor(ToColor(rgba), state.flFlash), partAlpha);
}

void CHudServerPanel::PaintBackground()
{
	// HACK ALERT: Text is drawn in Paint(), see CHudDeathNoticePanel::Paint()
	if (m_iDotTexture == -1)
	{
		m_iDotTexture = CreateCircleTexture(false, 0, CIRCLE_TEXTURE_SIZE);
		m_iRingTexture = CreateCircleTexture(true, 0, CIRCLE_TEXTURE_SIZE);
		m_iCapTextures[0] = CreateCircleTexture(false, 0, CIRCLE_TEXTURE_SIZE / 2);
		m_iCapTextures[1] = CreateCircleTexture(false, CIRCLE_TEXTURE_SIZE / 2, CIRCLE_TEXTURE_SIZE);
	}

	const CScoreboardData &data = CScoreboardData::Get();

	// Boxes go under every other shape, and before any sprite turns blending off
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		if (m_States[id].bDrawn && m_States[id].kind == bhl::sb::E_ElementKind::Box)
			PaintBox(m_States[id], data.GetElement(id));
	}

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const ElementState &state = m_States[id];

		if (!state.bDrawn)
			continue;

		const CScoreboardData::Element &el = data.GetElement(id);

		switch (state.kind)
		{
		case bhl::sb::E_ElementKind::Meter:
			PaintMeter(state, el);
			break;
		case bhl::sb::E_ElementKind::Strip:
			PaintStripCells(id, state, el);
			break;
		case bhl::sb::E_ElementKind::Icon:
			DrawPanelSprite(state.iSprite, state.iIconX, state.iIconY, state.iIconWide, state.iIconTall, GetColor(state, 255));
			break;
		case bhl::sb::E_ElementKind::Board:
			PaintBoardShapes(id, state, el);
			break;
		case bhl::sb::E_ElementKind::Feed:
			PaintFeed(id, state, el, false);
			break;
		default:
			break;
		}
	}
}

void CHudServerPanel::Paint()
{
	const CScoreboardData &data = CScoreboardData::Get();

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const ElementState &state = m_States[id];

		if (!state.bDrawn)
			continue;

		if (state.kind == bhl::sb::E_ElementKind::Strip)
			PaintStripTexts(id, state, data.GetElement(id));
		else if (state.kind == bhl::sb::E_ElementKind::Board)
			PaintBoardTexts(id, state, data.GetElement(id));
		else if (state.kind == bhl::sb::E_ElementKind::Feed)
			PaintFeed(id, state, data.GetElement(id), true);
		else if (state.bHasText)
			DrawCellText(state.font, state.iTextX, state.iTextY, GetColor(state, 255), state.wszText);
	}
}

void CHudServerPanel::PaintMeter(const ElementState &state, const CScoreboardData::Element &el)
{
	int dot = state.tall;

	if (state.bRing)
	{
		Color color = GetColor(state, 255);

		for (int i = 0; i < state.iRingDots; i++)
		{
			int x, y;
			GetRingDot(state.wide, state.iDotSize, i, state.iRingDots, x, y);
			DrawTexture(i < state.iRingFilled ? m_iDotTexture : m_iRingTexture, state.x + x, state.y + y, state.iDotSize, state.iDotSize, color);
		}

		return;
	}

	if (state.bDots)
	{
		Color color = GetColor(state, 255);

		for (int i = 0; i < state.iMax; i++)
			DrawTexture(i < state.iCur ? m_iDotTexture : m_iRingTexture, state.x + i * (dot + state.iDotGap), state.y, dot, dot, color);

		return;
	}

	DrawRoundedBar(state.x, state.y, state.wide, dot, GetColor(state, GetParam(el, bhl::sb::E_Param::AlphaEmpty)));

	if (state.iFillWide > 0)
		DrawRoundedBar(state.x, state.y, state.iFillWide, dot, GetColor(state, 255));
}

void CHudServerPanel::PaintStripCells(int id, const ElementState &state, const CScoreboardData::Element &el)
{
	int alphaPassed = GetParam(el, bhl::sb::E_Param::AlphaPassed);
	int alphaCurrent = GetParam(el, bhl::sb::E_Param::AlphaCurrent);
	int alphaFuture = GetParam(el, bhl::sb::E_Param::AlphaFuture);
	int alphaIconFuture = GetParam(el, bhl::sb::E_Param::AlphaIconFuture);
	bool hasFrame = GetParam(el, bhl::sb::E_Param::Frame) != 0;
	int padding = Scaled(GetParam(el, bhl::sb::E_Param::IconPadding));
	int step = state.iCellWide + state.iCellGap;

	for (int i = 0; i < state.iShown; i++)
	{
		int cell = state.iFirst + i;
		int x = state.x + i * step;
		int alpha = cell < state.iCur ? alphaPassed : (cell == state.iCur ? alphaCurrent : alphaFuture);

		vgui2::surface()->DrawSetColor(GetColor(state, alpha));
		vgui2::surface()->DrawFilledRect(x, state.y, x + state.iCellWide, state.y + state.tall);

		if (cell == state.iCur && hasFrame)
		{
			vgui2::surface()->DrawSetColor(GetColor(state, 255));
			vgui2::surface()->DrawOutlinedRect(x, state.y, x + state.iCellWide, state.y + state.tall);
		}
	}

	for (int i = 0; i < state.iShown; i++)
	{
		int cell = state.iFirst + i;
		int sprite = m_iStripSprites[id][cell];
		const wrect_t &rc = GetScoreSpriteRect(sprite);

		if (sprite == -1 || rc.GetWidth() <= 0 || rc.GetHeight() <= 0)
			continue;

		float scale = std::min(float(state.iCellWide - 2 * padding) / rc.GetWidth(), float(state.tall - 2 * padding) / rc.GetHeight());
		int wide = static_cast<int>(rc.GetWidth() * scale);
		int tall = static_cast<int>(rc.GetHeight() * scale);

		if (wide <= 0 || tall <= 0)
			continue;

		int x = state.x + i * step + (state.iCellWide - wide) / 2;
		int y = state.y + (state.tall - tall) / 2;
		DrawPanelSprite(sprite, x, y, wide, tall, GetColor(state, cell > state.iCur ? alphaIconFuture : 255));
	}

	if (state.bTrack)
		PaintTrackMarkers(id, state, false);
}

void CHudServerPanel::PaintStripTexts(int id, const ElementState &state, const CScoreboardData::Element &el)
{
	const CScoreboardData::List *list = el.szList[0] ? CScoreboardData::Get().FindList(el.szList) : nullptr;
	int alphaIconFuture = GetParam(el, bhl::sb::E_Param::AlphaIconFuture);
	int step = state.iCellWide + state.iCellGap;

	// Icon items are sprites: one the client doesn't have leaves its cell empty
	for (int i = 0; list && i < state.iShown && state.iFirst + i < list->iCount; i++)
	{
		int cell = state.iFirst + i;
		const CScoreboardData::Value &item = list->items[cell];

		if (item.type == bhl::sb::E_ValueType::Icon || item.IsEmpty())
			continue;

		wchar_t text[bhl::sb::TEMPLATE_BUF_LEN];
		item.ToString(text, sizeof(text));

		if (!text[0])
			continue;

		int x = state.x + i * step + (state.iCellWide - GetTextWide(state.font, text)) / 2;
		DrawCellText(state.font, x, state.iTextY, GetColor(state, cell > state.iCur ? alphaIconFuture : 255), text);
	}

	Color color = GetColor(state, 255);

	if (state.iFirst > 0)
	{
		int markerWide = vgui2::surface()->GetCharacterWidth(state.font, MARKER_LEFT[0]);
		DrawCellText(state.font, state.x - state.iCellGap - markerWide, state.iTextY, color, MARKER_LEFT);
	}

	if (state.iFirst + state.iShown < state.iMax)
		DrawCellText(state.font, state.x + state.wide + state.iCellGap, state.iTextY, color, MARKER_RIGHT);

	if (state.bTrack)
		PaintTrackMarkers(id, state, true);
}

void CHudServerPanel::PaintTrackMarkers(int id, const ElementState &state, bool texts)
{
	int disc = std::max(2, Scaled(4));
	int gap = Scaled(2);
	Color color = GetColor(state, 255);

	for (int i = 0; i < state.iShown; i++)
	{
		int n = m_iTrackCounts[id][state.iFirst + i];
		wchar_t text[16];

		if (n <= 0 || (n > 2) != texts)
			continue;

		V_snwprintf(text, std::size(text), TRACK_FORMAT, n);
		Box box = LayoutTrackMarker(state.x + i * (state.iCellWide + state.iCellGap), state.iCellWide, state.y, n, disc, gap, GetTextWide(state.font, text), state.iFontTall);

		if (texts)
		{
			DrawCellText(state.font, box.x, box.y, color, text);
			continue;
		}

		for (int j = 0; j < n; j++)
			DrawTexture(m_iDotTexture, box.x + j * (disc + gap), box.y, disc, disc, color);
	}
}

void CHudServerPanel::PaintBoardShapes(int id, const ElementState &state, const CScoreboardData::Element &el)
{
	const RowsState &rows = GetPaintRows(id, state);
	int highlightAlpha = GetParam(el, bhl::sb::E_Param::HighlightAlpha);
	int fillAlpha = GetParam(el, bhl::sb::E_Param::FillAlpha);

	for (int i = 0; i < rows.iCount; i++)
	{
		const RowState &row = rows.rows[i];
		int y = state.y + i * rows.iStep;
		int fillWide = static_cast<int>(state.wide * row.flFill);

		if (i == rows.iOwn && highlightAlpha > 0)
		{
			vgui2::surface()->DrawSetColor(GetColor(state, highlightAlpha));
			vgui2::surface()->DrawFilledRect(state.x, y, state.x + state.wide, y + state.iFontTall);
		}

		if (fillAlpha > 0 && fillWide > 0)
		{
			vgui2::surface()->DrawSetColor(GetColor(state, fillAlpha));
			vgui2::surface()->DrawFilledRect(state.x, y, state.x + fillWide, y + state.iFontTall);
		}

		if (row.iSprite != -1)
		{
			int x = state.x + rows.iIconX + (rows.iIconWide - row.iIconWide) / 2;
			DrawPanelSprite(row.iSprite, x, y, row.iIconWide, state.iFontTall, GetRowColor(state, el, row.iVariant, 255));
		}
	}
}

void CHudServerPanel::PaintBoardTexts(int id, const ElementState &state, const CScoreboardData::Element &el)
{
	const RowsState &rows = GetPaintRows(id, state);

	for (int i = 0; i < rows.iCount; i++)
	{
		const RowState &row = rows.rows[i];
		Color color = GetRowColor(state, el, row.iVariant, 255);

		for (int c = 0; c < row.iColumns; c++)
		{
			const wchar_t *text = row.wszText + row.iColumnStart[c];

			if (!text[0])
				continue;

			int x = state.x + rows.iColumnX[c] + (rows.bColumnRight[c] ? rows.iColumnWide[c] - row.iColumnWide[c] : 0);
			DrawCellText(state.font, x, state.y + i * rows.iStep, color, text);
		}
	}
}

void CHudServerPanel::PaintFeed(int id, const ElementState &state, const CScoreboardData::Element &el, bool texts)
{
	const RowsState &rows = GetPaintRows(id, state);

	for (int i = 0; i < rows.iCount; i++)
	{
		const RowState &row = rows.rows[i];
		int x = state.x + AlignInBox(state.iAnchor, state.wide, row.iWide);
		int y = state.y + i * rows.iStep;
		Color color = GetRowColor(state, el, row.iVariant, ToPartAlpha(row.flAlpha));

		if (texts && row.wszText[0])
			DrawCellText(state.font, x + row.iTextX, y, color, row.wszText);
		else if (!texts && row.iSprite != -1)
			DrawPanelSprite(row.iSprite, x, y, row.iIconWide, state.iFontTall, color);
	}
}

void CHudServerPanel::PaintBox(const ElementState &state, const CScoreboardData::Element &el)
{
	unsigned char rgba[4];
	Color hudColor = gHUD.GetHudColor(HudPart::Common, 0);
	Color accent = el.GetSlotColor(bhl::sb::SLOT_ACCENT, rgba) ? ToColor(rgba) : Color(hudColor.r(), hudColor.g(), hudColor.b(), 255);
	int radius = Scaled(GetParam(el, bhl::sb::E_Param::Radius));
	int accentWide = Scaled(GetParam(el, bhl::sb::E_Param::AccentWide));
	int x1 = state.x + state.wide;
	int y1 = state.y + state.tall;

	DrawRoundedBox(state.x, state.y, x1, y1, radius, GetColor(state, 255));

	if (accentWide > 0)
		DrawRoundedBox(state.x, state.y, x1, y1, radius, GetColor(state, accent, 255), state.x + accentWide);
}

void CHudServerPanel::DrawRoundedBar(int x, int y, int wide, int tall, Color color)
{
	int radius = tall / 2;
	wide = std::max(wide, 2 * radius);

	DrawTexture(m_iCapTextures[0], x, y, radius, tall, color);
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawFilledRect(x + radius, y, x + wide - radius, y + tall);
	DrawTexture(m_iCapTextures[1], x + wide - radius, y, radius, tall, color);
}
