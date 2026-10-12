#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include "hud.h"
#include "hud/server_panel.h"
#include "vgui/client_viewport.h"
#include "vgui/hud_ammo.h"
#include "vgui/hud_battery.h"
#include "vgui/hud_health.h"
#include "vgui/score_panel_cells.h"

//----------------------------------------------------------------------------------------------------
// Layout math (docs/scoreboard-protocol.md, 8.4, 8.7, 8.8). Pixels; sizes are already scaled with P().
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

}

//----------------------------------------------------------------------------------------------------
// Drawing helpers
//----------------------------------------------------------------------------------------------------
namespace
{

constexpr int CIRCLE_TEXTURE_SIZE = 64;

constexpr wchar_t MARKER_LEFT[] = L"\u2039";
constexpr wchar_t MARKER_RIGHT[] = L"\u203A";

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

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		m_szIconSprites[id][0] = '\0';
		m_iIconSprites[id] = -1;
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

	if (!CScoreboardData::IsHudEnabled() || data.GetVisibleMask() == 0)
		return false;

	if (hud_draw.GetFloat() <= 0 || gHUD.m_iIntermission || (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL))
		return false;

	UpdateStripSprites();

	LayoutNode nodes[bhl::sb::MAX_ELEMENTS];
	bool isParent[bhl::sb::MAX_ELEMENTS] = {};

	for (int id = 0; id < bhl::sb::MAX_ELEMENTS; id++)
	{
		const CScoreboardData::Element &el = data.GetElement(id);
		nodes[id].bValid = el.bValid;
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
	bool isAnyDrawn = false;

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
		state.bDrawn = isShown && hasContent;
		PlaceContent(el, GetBoxAnchor(in), state);

		isAnyDrawn = isAnyDrawn || state.bDrawn;
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
	state.wszText[0] = L'\0';
	state.bHasText = false;
	state.iTextWide = 0;
	state.iCur = 0;
	state.iMax = 0;
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
	const wrect_t &rc = gHUD.GetSpriteRect(sprite);

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

void CHudServerPanel::PlaceContent(const CScoreboardData::Element &el, int anchor, ElementState &state)
{
	state.iTextX = state.x;
	state.iTextY = state.y + (state.tall - state.iFontTall) / 2;

	if (el.kind == bhl::sb::E_ElementKind::Meter)
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
			m_iStripSprites[id][i] = hasIcon ? gHUD.GetSpriteIndex(item->szSprite) : -1;
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
		m_iIconSprites[id] = gHUD.GetSpriteIndex(sprite);
	}

	return m_iIconSprites[id];
}

int CHudServerPanel::GetParam(const CScoreboardData::Element &el, bhl::sb::E_Param param) const
{
	int value;

	if (el.GetParam(param, value))
		return value;

	return m_iParamDefaults[static_cast<int>(param)];
}

Color CHudServerPanel::GetColor(const ElementState &state, int partAlpha) const
{
	float alpha = clamp(partAlpha, 0, 255) * state.color.a() / 255.0f * gHUD.GetHudTransparency();
	return Color(state.color.r(), state.color.g(), state.color.b(), static_cast<int>(alpha));
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
			DrawScoreSprite(state.iSprite, state.iIconX, state.iIconY, state.iIconWide, state.iIconTall, GetColor(state, 255), true);
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
			PaintStripTexts(state, data.GetElement(id));
		else if (state.bHasText)
			DrawCellText(state.font, state.iTextX, state.iTextY, GetColor(state, 255), state.wszText);
	}
}

void CHudServerPanel::PaintMeter(const ElementState &state, const CScoreboardData::Element &el)
{
	int dot = state.tall;

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
		const wrect_t &rc = gHUD.GetSpriteRect(sprite);

		if (sprite == -1 || rc.GetWidth() <= 0 || rc.GetHeight() <= 0)
			continue;

		float scale = std::min(float(state.iCellWide - 2 * padding) / rc.GetWidth(), float(state.tall - 2 * padding) / rc.GetHeight());
		int wide = static_cast<int>(rc.GetWidth() * scale);
		int tall = static_cast<int>(rc.GetHeight() * scale);

		if (wide <= 0 || tall <= 0)
			continue;

		int x = state.x + i * step + (state.iCellWide - wide) / 2;
		int y = state.y + (state.tall - tall) / 2;
		DrawScoreSprite(sprite, x, y, wide, tall, GetColor(state, cell > state.iCur ? alphaIconFuture : 255), true);
	}
}

void CHudServerPanel::PaintStripTexts(const ElementState &state, const CScoreboardData::Element &el)
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
