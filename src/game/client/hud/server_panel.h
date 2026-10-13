#ifndef HUD_SERVER_PANEL_H
#define HUD_SERVER_PANEL_H
#include <memory>
#include <Color.h>
#include <vgui_controls/Panel.h>
#include "hud/base.h"
#include "scoreboard_data.h"

/**
 * HUD elements defined by the server: Text, Icon, Meter, Strip, Board, Feed and Box (docs/scoreboard-protocol.md, 8).
 * One full-screen panel lays out every element from the store and draws them in two passes, each in id order:
 * the shapes and sprites of all elements, then all texts (DrawFlushText turns off alpha blending, so texts go last).
 * Boxes are drawn before every other shape.
 * Kind params default to the values in HudServerPanel of HudLayoutBase.res, under the names of
 * bhl::sb::PARAM_INFO; a param sent by the server wins. Sizes are proportional units of a 640x480 screen.
 */
class CHudServerPanel : public CHudElemBase<CHudServerPanel>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CHudServerPanel, vgui2::Panel);

	CHudServerPanel();

	void Init() override;
	void VidInit() override;
	void Think() override;
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	void ApplySettings(KeyValues *inResourceData) override;
	void PaintBackground() override;
	void Paint() override;

private:
	//! Rows of a Board (the rows param, the separator and the own row) and lines of a Feed
	static constexpr int MAX_ROWS = 18;

	//! Columns of a Board row: its text split at TAB characters
	static constexpr int MAX_ROW_COLUMNS = 4;

	//! Time of an effect that never ran
	static constexpr float NEVER = -1.0e30f;

	//! Box and drawing state of an element, computed every frame parents first (8.4) for elements that are drawn or are parents
	struct ElementState
	{
		bool bDrawn = false; //!< Passes every rule of 8.3 and hold_ms, or fades out
		bool bGhost = false; //!< The last drawn state of an element that fades out (fade_ms)
		bhl::sb::E_ElementKind kind = bhl::sb::E_ElementKind::None;
		int x = 0, y = 0;
		int wide = 0, tall = 0;
		Color color; //!< rgb and the alpha of the element, before the part alpha and hud_draw
		float flAlpha = 1; //!< fade_ms and warn_pulse_ms; exactly 1 without them
		float flFlash = 0; //!< flash_ms: how far rgb goes toward white, for the variant colors of rows
		bool bWarn = false; //!< warn_at holds: color has the warn color, rows don't use their variant colors
		vgui2::HFont font = vgui2::INVALID_FONT;
		int iFontTall = 0;
		int iAnchor = 0; //!< Horizontal anchor of the box: alignment of Feed lines

		//! Text, the counter of a Meter or the text after an Icon
		wchar_t wszText[bhl::sb::TEMPLATE_BUF_LEN] = {};
		bool bHasText = false;
		int iTextWide = 0;
		int iTextX = 0, iTextY = 0; //!< Text; also the line of the markers and texts of a Strip

		int iCur = 0; //!< Meter: cur clamped to [0, max]; Strip: current cell
		int iMax = 0; //!< Meter: max; Strip: count

		// Meter (8.7)
		bool bDots = false; //!< A dot per unit: the counter param 1 hides the counter
		int iDotGap = 0;
		int iFillWide = 0;
		bool bRing = false; //!< form 1
		int iDotSize = 0;
		int iRingDots = 0;
		int iRingFilled = 0;

		// Strip (8.8)
		int iCellWide = 0;
		int iCellGap = 0;
		int iShown = 0;
		int iFirst = 0;
		bool bTrack = false; //!< track 1: markers of the other players above the cells

		// Icon (8.9)
		int iSprite = -1;
		int iIconX = 0, iIconY = 0;
		int iIconWide = 0, iIconTall = 0;
	};

	//! A row of a Board or a line of a Feed
	struct RowState
	{
		wchar_t wszText[bhl::sb::TEMPLATE_BUF_LEN] = {}; //!< Board: the columns are separated by NULs
		int iColumnStart[MAX_ROW_COLUMNS] = {};
		int iColumnWide[MAX_ROW_COLUMNS] = {};
		int iColumns = 0;
		int iVariant = 0; //!< Template variant: its color slot applies
		int iSprite = -1;
		int iIconWide = 0; //!< The icon is as tall as the font
		float flFill = 0; //!< Board: fill_alpha bar, part of the row width
		float flAlpha = 1; //!< Feed: fade of the line
		int iTextX = 0; //!< Feed: offset of the text in the line
		int iWide = 0; //!< Feed: width of the line

		//! The last icon sprite name of the row and its index, failures too
		char szSpriteName[bhl::sb::MAX_TEXT_LEN + 1] = {};
		int iSpriteOfName = -1;
	};

	//! Rows of a Board or lines of a Feed, allocated for elements of these kinds
	struct RowsState
	{
		int iCount = 0;
		int iOwn = -1; //!< Board: the highlighted row
		int iStep = 0; //!< Font height and the row or line gap
		int iColumns = 0;
		int iColumnX[MAX_ROW_COLUMNS] = {}; //!< Offsets in the box
		int iColumnWide[MAX_ROW_COLUMNS] = {};
		bool bColumnRight[MAX_ROW_COLUMNS] = {};
		int iIconX = 0;
		int iIconWide = 0;
		RowState rows[MAX_ROWS];
	};

	//! Effects of an element (8.13), kept between frames
	struct EffectState
	{
		bhl::sb::E_ElementKind kind = bhl::sb::E_ElementKind::None; //!< Another kind resets the effects
		int iRemovals = 0; //!< CScoreboardData::GetElementRemovals(): a removal resets the effects
		CScoreboardData::Subject subject; //!< Another subject resets the content signature
		bool bPresent = false; //!< Shown with content, before hold_ms
		bool bDrawn = false; //!< Also the direction of the fade since flFadeTime
		bool bHasGhost = false; //!< m_pGhosts holds the last drawn state
		bool bHasSignature = false;
		unsigned int iSignature = 0; //!< Hash of what is drawn
		float flHoldTime = NEVER; //!< Appearance or content change
		float flFlashTime = NEVER;
		bool bWarn = false;
		float flWarnTime = NEVER;
		float flFadeFrom = 0;
		float flFadeTime = 0;
		int iFadeMs = 0;
		float flFade = 0; //!< Fade factor of this frame
	};

	//! A line of an announcer Feed and its interval, all 0 for none
	struct AnnouncerAnchor
	{
		int iSeq = 0; //!< CScoreboardData::HudLine::iSeq
		float flStart = 0;
		float flEnd = 0;
	};

	//! Param defaults by bhl::sb::E_Param, from HudLayoutBase.res and clamped to bhl::sb::PARAM_INFO
	int m_iParamDefaults[bhl::sb::MAX_PARAM_ID + 1] = {};

	//! Fonts by bhl::sb::E_Font, from bhl::sb::FONT_NAMES of the scheme
	vgui2::HFont m_Fonts[bhl::sb::FONT_COUNT] = {};

	ElementState m_States[bhl::sb::MAX_ELEMENTS];

	//! Sprite indexes of the list items of Strip elements, valid while the store serial is m_iStripSpriteSerial
	int m_iStripSprites[bhl::sb::MAX_ELEMENTS][bhl::sb::MAX_LIST_ITEMS] = {};
	int m_iStripSpriteSerial = -1;

	//! Sprite of the value of Icon elements and its name: the value follows the subject, not the store serial
	char m_szIconSprites[bhl::sb::MAX_ELEMENTS][bhl::sb::MAX_TEXT_LEN + 1] = {};
	int m_iIconSprites[bhl::sb::MAX_ELEMENTS] = {};

	EffectState m_Effects[bhl::sb::MAX_ELEMENTS];
	std::unique_ptr<ElementState> m_pGhosts[bhl::sb::MAX_ELEMENTS];
	std::unique_ptr<RowsState> m_pRows[bhl::sb::MAX_ELEMENTS];
	std::unique_ptr<RowsState> m_pGhostRows[bhl::sb::MAX_ELEMENTS]; //!< Rows of the ghost: swapped with m_pRows, not copied

	//! Markers of a Strip with track 1: players other than the subject by cell
	unsigned char m_iTrackCounts[bhl::sb::MAX_ELEMENTS][bhl::sb::MAX_LIST_ITEMS] = {};

	//! Announcer Feeds (queue 1): the last started line, so evicting older lines doesn't move the later ones
	AnnouncerAnchor m_Anchors[bhl::sb::MAX_ELEMENTS];

	//! CScoreboardData::GetResetCount() seen last: after a Reset the content signatures start over
	int m_iResetCount = -1;

	int m_iDotTexture = -1;
	int m_iRingTexture = -1;
	int m_iCapTextures[2] = { -1, -1 }; //!< Left and right halves of a disc: caps of a rounded bar

	//! Lays out every element. Returns true if any is drawn.
	bool UpdateLayout();

	/**
	 * Content and size of the box (8.6-8.9).
	 * @param	freeWide	Width between the side zones of the numbers row for a Strip on it
	 * @return	true if the element has content (8.3 rule 7)
	 */
	bool MeasureElement(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, int freeWide, ElementState &state);
	bool MeasureMeter(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state);
	bool MeasureStrip(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, int freeWide, ElementState &state);
	bool MeasureIcon(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state);
	bool MeasureBoard(int id, const CScoreboardData::Element &el, const CScoreboardData::Subject &subject, ElementState &state);
	bool MeasureFeed(int id, const CScoreboardData::Element &el, ElementState &state);

	/**
	 * Positions of the texts and the icon in the placed box.
	 * @param	anchor	Horizontal anchor of the box: with the right one the Meter counter goes to the left
	 */
	void PlaceContent(const CScoreboardData::Element &el, int anchor, ElementState &state);

	void UpdateStripSprites();
	int GetIconSprite(int id, const char *sprite);
	void SetRowIcon(RowState &row, const char *sprite, int tall);
	void UpdateTrack(int id, const CScoreboardData::Subject &subject, int count);

	//! Rows of a Board or Feed, allocated on first use
	RowsState &GetRows(int id);

	//! Rows to draw: those of the ghost for a ghost
	const RowsState &GetPaintRows(int id, const ElementState &state) const;

	/**
	 * Keeps the state of the elements drawn in the last frame that have fade_ms,
	 * so an element that stops being drawn fades out as it was drawn.
	 */
	void SaveGhosts();

	//! Global cutoffs: nothing fades out. keepPresence keeps hold_ms and the signatures running.
	void StopEffects(bool keepPresence);

	//! True if an element with fade_ms is drawn or fades out
	bool IsFading() const;

	/**
	 * Effects of an element (8.13) after its layout: hold_ms, fade_ms, flash_ms, warn_at and the variant colors.
	 * Resets them when the kind or the subject changes.
	 * @param	present	Shown and has content
	 * @return	true if the element is drawn this frame, or its ghost while it fades out
	 */
	bool UpdateEffects(int id, const CScoreboardData::Element &el, bool present, float time);

	//! Hash of what the element draws (8.13, flash_ms)
	unsigned int GetSignature(int id, const CScoreboardData::Subject &subject, const ElementState &state) const;

	//! Adds the box of the element and the parts it draws outside it to bounds (x0, y0, x1, y1)
	void AddDrawnBounds(int id, const ElementState &state, int *bounds) const;

	//! The server's param or the default
	int GetParam(const CScoreboardData::Element &el, bhl::sb::E_Param param) const;

	//! Color for a part of the element (8.5): part * a / 255 * hud_draw, with the fade and pulse
	Color GetColor(const ElementState &state, int partAlpha) const;
	Color GetColor(const ElementState &state, Color color, int partAlpha) const;

	//! Color of a Board row or a Feed line: the color slot of its variant unless the element warns
	Color GetRowColor(const ElementState &state, const CScoreboardData::Element &el, int variant, int partAlpha) const;

	void PaintMeter(const ElementState &state, const CScoreboardData::Element &el);
	void PaintStripCells(int id, const ElementState &state, const CScoreboardData::Element &el);
	void PaintStripTexts(int id, const ElementState &state, const CScoreboardData::Element &el);
	void PaintTrackMarkers(int id, const ElementState &state, bool texts);
	void PaintBoardShapes(int id, const ElementState &state, const CScoreboardData::Element &el);
	void PaintBoardTexts(int id, const ElementState &state, const CScoreboardData::Element &el);
	void PaintFeed(int id, const ElementState &state, const CScoreboardData::Element &el, bool texts);
	void PaintBox(const ElementState &state, const CScoreboardData::Element &el);
	void DrawRoundedBar(int x, int y, int wide, int tall, Color color);
};

#endif
