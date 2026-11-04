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
};

//! Resolves the auto anchor by the position, as set_hudmessage does
int GetAnchor(int pos, int anchor)
{
	if (anchor != bhl::sb::ANCHOR_AUTO)
		return anchor;

	if (pos == bhl::sb::POS_CENTER)
		return bhl::sb::ANCHOR_CENTER;

	return pos < 0 ? bhl::sb::ANCHOR_END : bhl::sb::ANCHOR_START;
}

//! Horizontal anchor of a placed box: the side of the box that stays when its width changes
int GetBoxAnchor(const CScoreboardData::Element &el)
{
	if (el.place == bhl::sb::E_Place::ParentLeft)
		return bhl::sb::ANCHOR_END;

	if (el.place == bhl::sb::E_Place::ParentRight)
		return bhl::sb::ANCHOR_START;

	return GetAnchor(el.x, el.hAlign);
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
	case bhl::sb::ANCHOR_CENTER:
		return ref - size / 2;
	case bhl::sb::ANCHOR_END:
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
 * @param	gap		P(el.iGap)
 * @param	parent	Placed box of the parent, used by the parent places
 */
void PlaceBox(const CScoreboardData::Element &el, int gap, const Box &parent, const NumbersRow &row, int screenWide, int screenTall, Box &box)
{
	switch (el.place)
	{
	case bhl::sb::E_Place::ParentLeft:
	case bhl::sb::E_Place::ParentRight:
		box.x = el.place == bhl::sb::E_Place::ParentLeft ? parent.x - gap - box.wide : parent.x + parent.wide + gap;
		box.y = parent.y + (parent.tall - box.tall) / 2;
		break;
	case bhl::sb::E_Place::Parent:
		box.x = PlaceOnAxis(el.x, el.hAlign, screenWide, box.wide);
		box.y = el.vAlign == bhl::sb::ANCHOR_END ? parent.y - gap - box.tall : parent.y + parent.tall + gap;
		break;
	case bhl::sb::E_Place::NumbersRow:
	{
		// The zones are tested where the box will be drawn
		box.x = MoveInside(PlaceOnAxis(el.x, el.hAlign, screenWide, box.wide), box.wide, screenWide);
		bool lifted = box.x < row.iLeft + gap || box.x + box.wide > row.iRight - gap;
		box.y = lifted ? row.iTop - gap - box.tall : row.iTop + (row.iTall - box.tall) / 2;
		break;
	}
	default:
		box.x = PlaceOnAxis(el.x, el.hAlign, screenWide, box.wide);
		box.y = PlaceOnAxis(el.y, el.vAlign, screenTall, box.tall);
		break;
	}

	box.x = MoveInside(box.x, box.wide, screenWide);
	box.y = MoveInside(box.y, box.tall, screenTall);
}

/**
 * Orders the elements parents first (8.4). Leaves out Boxes, which are never parents: they cover their group
 * after the layout. Leaves out the unplaced ones too: undefined, banner places, and parent places whose parent
 * is undefined, a Box, has a banner place, is the element itself, is in a cycle or is unplaced.
 * @param	order	Receives the ids of the placed elements
 * @return	Number of placed elements
 */
int GetLayoutOrder(const CScoreboardData &data, int *order)
{
	bool placed[bhl::sb::MAX_ELEMENTS] = {};
	int count = 0;

	// Each pass places the elements whose parent is placed: a chain has at most MAX_ELEMENTS links
	for (int pass = 0; pass < bhl::sb::MAX_ELEMENTS; pass++)
	{
		for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
		{
			const CScoreboardData::Element &el = data.GetElement(id);
			bool hasParent = el.iParent >= 0 && el.iParent < bhl::sb::MAX_ELEMENTS && placed[el.iParent];

			if (!placed[id] && el.bValid && el.kind != bhl::sb::E_ElementKind::Box && !bhl::sb::IsBannerPlace(el.place) && (!bhl::sb::IsParentPlace(el.place) || hasParent))
			{
				placed[id] = true;
				order[count++] = id;
			}
		}
	}

	return count;
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

//! Length of count rows, cells or dots of size pixels with gap between them
int GetStackLength(int count, int size, int gap)
{
	return count > 0 ? count * size + (count - 1) * gap : 0;
}

//! True if the parent chain of element id reaches root, id included. A Box ends the chain like an undefined element.
bool IsInGroup(const CScoreboardData &data, int id, int root)
{
	for (int i = 0; i < bhl::sb::MAX_ELEMENTS && id >= 0 && id < bhl::sb::MAX_ELEMENTS; i++)
	{
		const CScoreboardData::Element &el = data.GetElement(id);

		if (id == root)
			return true;

		if (!el.bValid || el.kind == bhl::sb::E_ElementKind::Box || !bhl::sb::IsParentPlace(el.place))
			return false;

		id = el.iParent;
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

//! warn_pulse_ms factor of the alpha while warning: 1 at the start of the warning
float GetPulseFactor(float elapsed, int periodMs)
{
	if (periodMs <= 0)
		return 1.0f;

	return 0.55f + 0.45f * static_cast<float>(std::cos(2 * PI * elapsed * 1000.0 / periodMs));
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
 * @param	lines	In arrival order, iSeq growing
 * @return	Line index or -1; anchorStart is then its start
 */
int FindAnnouncerLine(const CScoreboardData::HudLine *const *lines, const float *life, int count, float now, int &anchorSeq, float &anchorStart, float &anchorEnd)
{
	float end = anchorEnd;

	for (int i = 0; i < count; i++)
	{
		if (lines[i]->iSeq < anchorSeq)
			continue;

		float lineStart = lines[i]->iSeq == anchorSeq ? anchorStart : std::max(lines[i]->flTime, end);
		end = lineStart + life[i];

		if (lineStart > now)
			break;

		anchorSeq = lines[i]->iSeq;
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

//----------------------------------------------------------------------------------------------------
// Drawing helpers
//----------------------------------------------------------------------------------------------------

constexpr int CIRCLE_TEXTURE_SIZE = 64;

constexpr wchar_t MARKER_LEFT[] = L"\u2039";
constexpr wchar_t MARKER_RIGHT[] = L"\u203A";
constexpr wchar_t BOARD_SEPARATOR = 0x2026;
constexpr wchar_t TRACK_FORMAT[] = L"\u00D7%d";

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
	auto flash = [k](int value) { return k > 0 ? value + static_cast<int>((255 - value) * k + 0.5f) : value; };
	return Color(flash(color.r()), flash(color.g()), flash(color.b()), color.a());
}

//! The sprite of name, looked up only when name isn't cachedName: failures are cached too
int GetCachedSprite(const char *name, char (&cachedName)[bhl::sb::MAX_TEXT_LEN + 1], int &cachedSprite)
{
	if (strcmp(cachedName, name))
	{
		V_strcpy_safe(cachedName, name);
		cachedSprite = GetScoreSpriteIndex(name);
	}

	return cachedSprite;
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
		m_Effects[id] = EffectState();

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

	float time = CScoreboardData::GetTime();

	// No flash for what a Reset or full sync changes, and the announcers start over
	if (m_iResetCount != m_Data.GetResetCount())
	{
		m_iResetCount = m_Data.GetResetCount();

		for (EffectState &fx : m_Effects)
		{
			fx.bHasSignature = false;
			fx.anchor = AnnouncerAnchor();
		}
	}

	if (!CScoreboardData::IsHudEnabled() || hud_draw.GetFloat() <= 0 || gHUD.m_iIntermission || (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL))
	{
		StopEffects(true);
		return false;
	}

	if (m_Data.GetVisibleMask() == 0 && !IsFading())
	{
		StopEffects(false);
		return false;
	}

	UpdateStripSprites();
	SaveGhosts();

	bool isParent[bhl::sb::MAX_ELEMENTS] = {};
	bool isPresent[bhl::sb::MAX_ELEMENTS] = {};

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = m_Data.GetElement(id);

		if (el.bValid && bhl::sb::IsParentPlace(el.place) && el.iParent >= 0 && el.iParent < bhl::sb::MAX_ELEMENTS)
			isParent[el.iParent] = true;
	}

	int order[bhl::sb::MAX_ELEMENTS];
	int count = GetLayoutOrder(m_Data, order);
	int originX, originY, screenWide, screenTall;
	GetBounds(originX, originY, screenWide, screenTall);

	NumbersRow row = GetNumbersRow(originX, originY, screenWide, screenTall);
	bool isScoreboardOpen = g_pViewport->IsScoreBoardVisible();

	for (int i = 0; i < count; i++)
	{
		int id = order[i];
		const CScoreboardData::Element &el = m_Data.GetElement(id);
		ElementState &state = m_States[id];
		CScoreboardData::Subject subject;

		// The subject is needed even when the element is hidden: a parent's box counts anyway
		bool hasSubject = m_Data.GetElementSubject(id, subject);
		bool isShown = hasSubject && m_Data.IsElementVisible(id) && !(el.HasFlag(bhl::sb::E_ElementFlags::NoScoreboard) && isScoreboardOpen);

		if (!isShown && !isParent[id])
			continue;

		int gap = Scaled(el.iGap);
		int freeWide = el.place == bhl::sb::E_Place::NumbersRow ? (row.iRight - gap) - (row.iLeft + gap) : INT_MAX;
		bool hasContent = MeasureElement(id, el, subject, freeWide, state);

		// Only the parent places read the parent's box
		const ElementState &parent = bhl::sb::IsParentPlace(el.place) ? m_States[el.iParent] : state;
		Box box = { 0, 0, state.wide, state.tall };
		PlaceBox(el, gap, { parent.x, parent.y, parent.wide, parent.tall }, row, screenWide, screenTall, box);

		state.x = box.x;
		state.y = box.y;
		isPresent[id] = isShown && hasContent;
		PlaceContent(el, state);
	}

	bool isAnyDrawn = false;

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = m_Data.GetElement(id);

		if (el.kind != bhl::sb::E_ElementKind::Box && UpdateEffects(id, el, isPresent[id], time))
			isAnyDrawn = true;
	}

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = m_Data.GetElement(id);

		if (el.kind != bhl::sb::E_ElementKind::Box)
			continue;

		// The group: what the elements under the root draw, ghosts that fade out included
		int bounds[4] = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };
		float groupFade = 0;

		for (int member = 0; member < bhl::sb::MAX_ELEMENTS; member++)
		{
			const ElementState &memberState = m_States[member];

			if (!memberState.bDrawn || memberState.kind == bhl::sb::E_ElementKind::Box || !IsInGroup(m_Data, member, el.iParent))
				continue;

			AddDrawnBounds(member, memberState, bounds);
			groupFade = std::max(groupFade, m_Effects[member].flFade);
		}

		// Placed when its root is
		ElementState &state = m_States[id];
		CScoreboardData::Subject subject;
		bool isPlaced = bhl::sb::IsParentPlace(el.place) && std::find(order, order + count, el.iParent) != order + count;
		bool isPresentBox = isPlaced && m_Data.GetElementSubject(id, subject) && m_Data.IsElementVisible(id)
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

	state = ElementState();
	state.kind = el.kind;
	state.font = m_Fonts[font < bhl::sb::FONT_COUNT ? font : 0];
	state.iFontTall = vgui2::surface()->GetFontTall(state.font);
	state.color = Color(rgb.r(), rgb.g(), rgb.b(), el.a);
	state.tall = state.iFontTall;

	switch (el.kind)
	{
	case bhl::sb::E_ElementKind::Text:
		state.iTextWide = m_Data.GetElementText(id, subject, state.wszText, sizeof(state.wszText)) ? GetTextWide(state.font, state.wszText) : 0;
		state.wide = state.iTextWide;
		return state.wszText[0] != L'\0';
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
	CScoreboardData::Value value;
	int dot = std::max(2, Scaled(GetParam(el, bhl::sb::E_Param::DotSize)));
	state.tall = dot;

	if (!m_Data.GetElementValue(id, subject, value) || value.type != bhl::sb::E_ValueType::Progress || value.iMax <= 0)
		return false;

	state.iMax = value.iMax;
	state.iCur = clamp(value.iValue, 0, value.iMax);
	state.iDotGap = Scaled(GetParam(el, bhl::sb::E_Param::DotGap));

	int dots = GetParam(el, bhl::sb::E_Param::Dots);
	int barDots = dots > 0 ? dots : METER_BAR_DOTS;
	int barWide = Scaled(GetParam(el, bhl::sb::E_Param::BarWide));
	state.bDots = dots > 0 && state.iMax <= dots;
	state.wide = !state.bDots && barWide > 0 ? barWide : GetStackLength(state.bDots ? state.iMax : barDots, dot, state.iDotGap);
	state.iFillWide = state.iCur > 0 ? std::max(static_cast<int>(static_cast<long long>(state.wide) * state.iCur / state.iMax), dot) : 0;

	// Form 1 (8.7): dots on a circle, a dot per unit while max fits the dots, else barDots of them filled to round(cur * barDots / max)
	if (GetParam(el, bhl::sb::E_Param::Form) == static_cast<int>(bhl::sb::E_MeterForm::Ring))
	{
		state.bRing = true;
		state.iDotSize = dot;
		state.iRingDots = state.iMax <= dots ? state.iMax : barDots;
		state.bDots = state.iRingDots == state.iMax;
		state.iRingFilled = state.bDots ? state.iCur : static_cast<int>((2LL * state.iCur * state.iRingDots + state.iMax) / (2LL * state.iMax));
		state.wide = std::max(3 * dot, static_cast<int>(std::ceil(state.iRingDots * (dot + state.iDotGap) / PI)) + dot);
		state.tall = state.wide;
	}

	int counter = GetParam(el, bhl::sb::E_Param::Counter);

	if (counter == 2 || (counter == 1 && !state.bDots))
		state.iTextWide = m_Data.GetElementText(id, subject, state.wszText, sizeof(state.wszText)) ? GetTextWide(state.font, state.wszText) : 0;

	return true;
}

bool CHudServerPanel::MeasureStrip(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, int freeWide, ElementState &state)
{
	const CScoreboardData::List *list = el.szList[0] ? m_Data.FindList(el.szList) : nullptr;
	CScoreboardData::Value value;
	bool hasValue = m_Data.GetElementValue(id, subject, value);
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
	state.iCellGap = Scaled(GetParam(el, bhl::sb::E_Param::CellGap));
	state.tall = Scaled(GetParam(el, bhl::sb::E_Param::CellTall));

	// Without cells the strip has no width
	if (count == 0)
		return false;

	state.iCur = clamp(position, 1, count) - 1;

	int cellWide = GetParam(el, bhl::sb::E_Param::CellWide);
	int cellMin = std::max(1, Scaled(std::min(GetParam(el, bhl::sb::E_Param::CellMinWide), cellWide)));
	int cellMax = std::max(cellMin, Scaled(cellWide));
	int maxWide = GetParam(el, bhl::sb::E_Param::MaxWide);
	int limit = maxWide > 0 ? Scaled(maxWide) : GetWide();
	int minWide = count * cellMin + (count - 1) * state.iCellGap;

	// On the numbers row the strip fits between the side zones unless it is lifted above them
	if (el.place == bhl::sb::E_Place::NumbersRow && !(minWide > freeWide && freeWide < limit))
		limit = std::min(limit, freeWide);

	state.iCellWide = std::clamp((limit - (count - 1) * state.iCellGap) / count, cellMin, cellMax);
	state.iShown = count;

	if (minWide > limit)
	{
		int marker = vgui2::surface()->GetCharacterWidth(state.font, MARKER_LEFT[0]) + state.iCellGap;
		state.iShown = std::clamp((limit - 2 * marker + state.iCellGap) / std::max(cellMin + state.iCellGap, 1), 1, count);
		state.iFirst = std::clamp(state.iCur - state.iShown / 2, 0, count - state.iShown);
	}

	state.wide = GetStackLength(state.iShown, state.iCellWide, state.iCellGap);
	state.bTrack = GetParam(el, bhl::sb::E_Param::Track) == 1;

	if (state.bTrack)
		UpdateTrack(id, subject, count);

	return hasPosition;
}

bool CHudServerPanel::MeasureIcon(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state)
{
	CScoreboardData::Value value;

	// Without an Icon value the element has no content, not even its text (8.3, 8.9)
	if (!m_Data.GetElementValue(id, subject, value) || value.type != bhl::sb::E_ValueType::Icon)
		return false;

	int iconTall = GetParam(el, bhl::sb::E_Param::IconTall);
	int tall = iconTall > 0 ? Scaled(iconTall) : state.iFontTall;
	int sprite = value.szSprite[0] ? GetCachedSprite(value.szSprite, m_szIconSprites[id], m_iIconSprites[id]) : -1;
	const wrect_t &rc = GetScoreSpriteRect(sprite);

	if (rc.GetWidth() > 0 && rc.GetHeight() > 0 && tall > 0)
	{
		state.iSprite = sprite;
		state.iIconWide = rc.GetWidth() * tall / rc.GetHeight();
		state.iIconTall = tall;
	}

	state.iTextWide = m_Data.GetElementText(id, subject, state.wszText, sizeof(state.wszText)) ? GetTextWide(state.font, state.wszText) : 0;

	int gap = state.iSprite != -1 && state.wszText[0] ? Scaled(GetParam(el, bhl::sb::E_Param::IconGap)) : 0;
	state.wide = state.iIconWide + gap + state.iTextWide;
	state.tall = std::max(state.iIconTall, state.iFontTall);
	return state.iSprite != -1 || state.wszText[0];
}

bool CHudServerPanel::MeasureBoard(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state)
{
	bool isList = el.szList[0] != '\0';
	const CScoreboardData::List *list = isList ? m_Data.FindList(el.szList) : nullptr;

	// In list mode @value is the item of the list named by the value ref
	bool hasValueList = isList && el.valueRef.builtin == bhl::sb::E_Builtin::None && el.valueRef.szKey[0];
	const CScoreboardData::List *values = hasValueList ? m_Data.FindList(el.valueRef.szKey) : nullptr;

	CScoreboardData::Subject entities[MAX_PLAYERS];
	int count = 0;
	int own = -1;

	if (isList)
	{
		CScoreboardData::Value select;
		int number = 0;
		count = list ? list->iCount : 0;

		if (subject.IsValid() && m_Data.GetRefValue(el.selectRef, subject, id, select) && select.GetNumber(number) && number >= 1 && number <= count)
			own = number - 1;
	}
	else
	{
		count = m_Data.GetOrderedEntities(entities, MAX_PLAYERS);
		const CScoreboardData::Subject *found = std::find(entities, entities + count, m_Data.GetRankedEntity(subject));
		own = subject.IsValid() && found != entities + count ? static_cast<int>(found - entities) : -1;
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

	const bhl::sb::CTemplate *templates[2] = { m_Data.GetTemplate(id, 0), m_Data.GetTemplate(id, 1) };
	CScoreboardData::Value fills[MAX_ROWS];
	int maxInt = 0;
	rows.iIconWide = 0;
	std::fill(std::begin(rows.iColumnWide), std::end(rows.iColumnWide), 0);
	std::fill(std::begin(rows.bColumnRight), std::end(rows.bColumnRight), true);

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
			rows.iColumnWide[0] = std::max(rows.iColumnWide[0], row.iColumnWide[0]);
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
			m_Data.GetElementValue(id, rowSubject, fills[i]);
		}

		row.wszText[0] = L'\0';

		if (templates[row.iVariant])
			m_Data.EvaluateTemplate(*templates[row.iVariant], rowSubject, id, row.wszText, sizeof(row.wszText), &extras);

		row.iColumns = SplitColumns(row.wszText, row.iColumnStart, MAX_ROW_COLUMNS);
		rows.iColumns = std::max(rows.iColumns, row.iColumns);

		for (int c = 0; c < row.iColumns; c++)
		{
			const wchar_t *text = row.wszText + row.iColumnStart[c];
			row.iColumnWide[c] = GetTextWide(state.font, text);
			rows.iColumnWide[c] = std::max(rows.iColumnWide[c], row.iColumnWide[c]);

			// Only digits, spaces and + - . , / : % make a right-aligned column
			rows.bColumnRight[c] = rows.bColumnRight[c] && wcsspn(text, L"0123456789 +-.,/:%") == wcslen(text);
		}

		const CScoreboardData::Value &icon = isList ? list->items[index] : fills[i];

		if (icon.type == bhl::sb::E_ValueType::Icon && icon.szSprite[0])
		{
			SetRowIcon(row, icon.szSprite, state.iFontTall);
			rows.iIconWide = std::max(rows.iIconWide, row.iIconWide);
		}

		if (fills[i].type == bhl::sb::E_ValueType::Int)
			maxInt = std::max(maxInt, fills[i].iValue);
	}

	// fill_alpha bars: cur / max of a Progress, an Int over the largest Int of the rows
	for (int i = 0; i < rows.iCount; i++)
	{
		const CScoreboardData::Value &value = fills[i];
		int maxValue = value.type == bhl::sb::E_ValueType::Progress ? value.iMax : (value.type == bhl::sb::E_ValueType::Int ? maxInt : 0);
		rows.rows[i].flFill = value.iValue > 0 && maxValue > 0 ? std::min(static_cast<float>(value.iValue) / maxValue, 1.0f) : 0.0f;
	}

	state.wide = LayoutBoardColumns(rows.iColumns, rows.iColumnWide, rows.iIconWide, Scaled(GetParam(el, bhl::sb::E_Param::ColumnGap)), rows.iColumnX, rows.iIconX);
	state.tall = GetStackLength(rows.iCount, state.iFontTall, rows.iStep - state.iFontTall);
	return true;
}

bool CHudServerPanel::MeasureFeed(int id, const CScoreboardData::Element &el, ElementState &state)
{
	const CScoreboardData::HudLine *lines[CScoreboardData::MAX_HUD_LINES];
	float lives[CScoreboardData::MAX_HUD_LINES];
	int count = m_Data.GetHudLines(id, lines);
	float time = CScoreboardData::GetTime();
	float life = GetParam(el, bhl::sb::E_Param::LifeMs) / 1000.0f;
	int fadeMs = GetParam(el, bhl::sb::E_Param::LineFadeMs);

	for (int i = 0; i < count; i++)
		lives[i] = lines[i]->flLife > 0 ? lines[i]->flLife : life;

	int picked[MAX_ROWS];
	float alphas[MAX_ROWS];
	int shown = 0;

	if (GetParam(el, bhl::sb::E_Param::Queue) == 1)
	{
		AnnouncerAnchor &anchor = m_Effects[id].anchor;
		int line = FindAnnouncerLine(lines, lives, count, time, anchor.iSeq, anchor.flStart, anchor.flEnd);

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
			float age = time - lines[i]->flTime;

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

	state.tall = GetStackLength(shown, state.iFontTall, rows.iStep - state.iFontTall);
	return true;
}

void CHudServerPanel::PlaceContent(const CScoreboardData::Element &el, ElementState &state)
{
	state.iTextX = state.x;
	state.iTextY = state.y + (state.tall - state.iFontTall) / 2;
	state.iAnchor = GetBoxAnchor(el);

	if (state.bRing)
	{
		state.iTextX = state.x + (state.wide - state.iTextWide) / 2;
	}
	else if (el.kind == bhl::sb::E_ElementKind::Meter)
	{
		// The counter stands outside the box, so the anchored edge of the box stays in place
		int gap = Scaled(GetParam(el, bhl::sb::E_Param::CounterGap));
		state.iTextX = state.iAnchor == bhl::sb::ANCHOR_END ? state.x - gap - state.iTextWide : state.x + state.wide + gap;
	}
	else if (el.kind == bhl::sb::E_ElementKind::Icon && state.iSprite != -1)
	{
		state.iTextX = state.x + state.iIconWide + Scaled(GetParam(el, bhl::sb::E_Param::IconGap));
	}
}

void CHudServerPanel::UpdateStripSprites()
{
	if (m_iStripSpriteSerial == m_Data.GetSerial())
		return;

	m_iStripSpriteSerial = m_Data.GetSerial();

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = m_Data.GetElement(id);
		const CScoreboardData::List *list = el.bValid && el.kind == bhl::sb::E_ElementKind::Strip && el.szList[0] ? m_Data.FindList(el.szList) : nullptr;

		for (int i = 0; i < bhl::sb::MAX_LIST_ITEMS; i++)
		{
			const CScoreboardData::Value *item = list && i < list->iCount ? &list->items[i] : nullptr;
			bool hasIcon = item && item->type == bhl::sb::E_ValueType::Icon && item->szSprite[0];
			m_iStripSprites[id][i] = hasIcon ? GetScoreSpriteIndex(item->szSprite) : -1;
		}
	}
}

void CHudServerPanel::SetRowIcon(RowState &row, const char *sprite, int tall)
{
	const wrect_t &rc = GetScoreSpriteRect(GetCachedSprite(sprite, row.szSpriteName, row.iSpriteOfName));
	bool isValid = rc.GetWidth() > 0 && rc.GetHeight() > 0;
	row.iSprite = isValid ? row.iSpriteOfName : -1;
	row.iIconWide = isValid ? rc.GetWidth() * tall / rc.GetHeight() : 0;
}

void CHudServerPanel::UpdateTrack(int id, const CScoreboardData::Subject &subject, int count)
{
	unsigned char *counts = m_iTrackCounts[id];
	std::fill(counts, counts + bhl::sb::MAX_LIST_ITEMS, 0);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CScoreboardData::Subject player = { i, 0 };
		CScoreboardData::Value value;
		int position = 0;

		if (i == subject.iPlayer || !CScoreboardData::IsParticipant(i) || !m_Data.GetElementValue(id, player, value) || !value.GetNumber(position))
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

int CHudServerPanel::GetParam(const CScoreboardData::Element &el, bhl::sb::E_Param param) const
{
	int value = m_iParamDefaults[static_cast<int>(param)];
	el.GetParam(param, value);
	return value;
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
	EffectState &fx = m_Effects[id];
	ElementState &state = m_States[id];
	int removals = m_Data.GetElementRemovals(id);

	// Removal and redefinition with another kind are instant: the old element doesn't fade out
	if (fx.kind != el.kind || fx.iRemovals != removals)
	{
		fx = EffectState();
		fx.kind = el.kind;
		fx.iRemovals = removals;
	}

	if (!el.bValid)
		return false;

	CScoreboardData::Subject subject;
	m_Data.GetElementSubject(id, subject);

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
		bool warn = warnAt > 0 && m_Data.GetElementValue(id, subject, value) && value.GetNumber(number) && number <= warnAt;

		if (warn && !fx.bWarn)
			fx.flWarnTime = time;

		fx.bWarn = warn;
	}

	// hold_ms: drawn that long after the appearance or a change; always without it
	bool drawn = present && (holdMs <= 0 || (time - fx.flHoldTime) * 1000.0f < holdMs);

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

		if (el.GetSlotColor(m_Data.GetElementVariant(id, subject), rgba))
			state.color = ToColor(rgba);

		if (fx.bWarn)
			state.color = el.GetSlotColor(bhl::sb::SLOT_WARN, rgba) ? ToColor(rgba) : ToColor(bhl::sb::DEFAULT_WARN_COLOR);

		state.bWarn = fx.bWarn;
		state.flFlash = GetFlashFactor(time - fx.flFlashTime, flashMs);
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
		hash = HashInt(hash, m_Data.GetElementVariant(id, subject));
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

	return state.wszText[0] ? HashText(hash, state.wszText) : hash;
}

void CHudServerPanel::AddDrawnBounds(int id, const ElementState &state, int *bounds) const
{
	AddToBounds(bounds, state.x, state.y, state.wide, state.tall);

	if (state.kind == bhl::sb::E_ElementKind::Meter && state.wszText[0])
		AddToBounds(bounds, state.iTextX, state.iTextY, state.iTextWide, state.iFontTall);

	if (state.kind != bhl::sb::E_ElementKind::Strip)
		return;

	int leftWide = vgui2::surface()->GetCharacterWidth(state.font, MARKER_LEFT[0]);
	int rightWide = vgui2::surface()->GetCharacterWidth(state.font, MARKER_RIGHT[0]);

	if (state.iFirst > 0)
		AddToBounds(bounds, state.x - state.iCellGap - leftWide, state.iTextY, leftWide, state.iFontTall);

	if (state.iFirst + state.iShown < state.iMax)
		AddToBounds(bounds, state.x + state.wide + state.iCellGap, state.iTextY, rightWide, state.iFontTall);

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

	// Boxes go under every other shape, and before any sprite turns blending off
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		if (m_States[id].bDrawn && m_States[id].kind == bhl::sb::E_ElementKind::Box)
			PaintBox(m_States[id], m_Data.GetElement(id));
	}

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const ElementState &state = m_States[id];

		if (!state.bDrawn)
			continue;

		const CScoreboardData::Element &el = m_Data.GetElement(id);

		switch (state.kind)
		{
		case bhl::sb::E_ElementKind::Meter:
			PaintMeter(state, el);
			break;
		case bhl::sb::E_ElementKind::Strip:
			PaintStripCells(id, state, el);
			break;
		case bhl::sb::E_ElementKind::Icon:
			DrawPanelSprite(state.iSprite, state.x, state.y + (state.tall - state.iIconTall) / 2, state.iIconWide, state.iIconTall, GetColor(state, 255));
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
	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const ElementState &state = m_States[id];

		if (!state.bDrawn)
			continue;

		if (state.kind == bhl::sb::E_ElementKind::Strip)
			PaintStripTexts(id, state, m_Data.GetElement(id));
		else if (state.kind == bhl::sb::E_ElementKind::Board)
			PaintBoardTexts(id, state, m_Data.GetElement(id));
		else if (state.kind == bhl::sb::E_ElementKind::Feed)
			PaintFeed(id, state, m_Data.GetElement(id), true);
		else if (state.wszText[0])
			DrawCellText(state.font, state.iTextX, state.iTextY, GetColor(state, 255), state.wszText);
	}
}

void CHudServerPanel::PaintMeter(const ElementState &state, const CScoreboardData::Element &el)
{
	int dot = state.tall;

	if (state.bRing)
	{
		Color color = GetColor(state, 255);
		double center = (state.wide - state.iDotSize) / 2.0;

		// Dot 0 on top, clockwise
		for (int i = 0; i < state.iRingDots; i++)
		{
			double angle = -PI / 2 + 2 * PI * i / state.iRingDots;
			int x = static_cast<int>(std::floor(center + center * std::cos(angle) + 0.5));
			int y = static_cast<int>(std::floor(center + center * std::sin(angle) + 0.5));
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
	const CScoreboardData::List *list = el.szList[0] ? m_Data.FindList(el.szList) : nullptr;
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
		DrawCellText(state.font, state.x - state.iCellGap - vgui2::surface()->GetCharacterWidth(state.font, MARKER_LEFT[0]), state.iTextY, color, MARKER_LEFT);

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
		int space = state.wide - row.iWide;
		int x = state.x + (state.iAnchor == bhl::sb::ANCHOR_END ? space : (state.iAnchor == bhl::sb::ANCHOR_CENTER ? space / 2 : 0));
		int y = state.y + i * rows.iStep;
		Color color = GetRowColor(state, el, row.iVariant, static_cast<int>(row.flAlpha * 255 + 0.5f));

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
