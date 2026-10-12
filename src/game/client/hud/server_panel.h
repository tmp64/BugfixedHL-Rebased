#ifndef HUD_SERVER_PANEL_H
#define HUD_SERVER_PANEL_H
#include <Color.h>
#include <vgui_controls/Panel.h>
#include "hud/base.h"
#include "scoreboard_data.h"

/**
 * HUD elements defined by the server: Text, Icon, Meter and Strip (docs/scoreboard-protocol.md, 8).
 * One full-screen panel lays out every element from the store and draws them in two passes, each in id order:
 * the shapes and sprites of all elements, then all texts (DrawFlushText turns off alpha blending, so texts go last).
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
	//! Box and drawing state of an element, computed every frame parents first (8.4) for elements that are drawn or are parents
	struct ElementState
	{
		bool bDrawn = false; //!< Passes every rule of 8.3
		bhl::sb::E_ElementKind kind = bhl::sb::E_ElementKind::None;
		int x = 0, y = 0;
		int wide = 0, tall = 0;
		Color color; //!< rgb and the alpha of the element, before the part alpha and hud_draw
		vgui2::HFont font = vgui2::INVALID_FONT;
		int iFontTall = 0;

		//! Text, the counter of a Meter or the text after an Icon
		wchar_t wszText[bhl::sb::TEMPLATE_BUF_LEN] = {};
		bool bHasText = false;
		int iTextWide = 0;
		int iTextX = 0, iTextY = 0; //!< Text; also the line of the markers and texts of a Strip

		int iCur = 0; //!< Meter: cur clamped to [0, max]; Strip: current cell
		int iMax = 0; //!< Meter: max; Strip: count

		// Meter (8.7)
		bool bDots = false;
		int iDotGap = 0;
		int iFillWide = 0;

		// Strip (8.8)
		int iCellWide = 0;
		int iCellGap = 0;
		int iShown = 0;
		int iFirst = 0;

		// Icon (8.9)
		int iSprite = -1;
		int iIconX = 0, iIconY = 0;
		int iIconWide = 0, iIconTall = 0;
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

	/**
	 * Positions of the texts and the icon in the placed box.
	 * @param	anchor	Horizontal anchor of the box: with the right one the Meter counter goes to the left
	 */
	void PlaceContent(const CScoreboardData::Element &el, int anchor, ElementState &state);

	void UpdateStripSprites();
	int GetIconSprite(int id, const char *sprite);

	//! The server's param or the default
	int GetParam(const CScoreboardData::Element &el, bhl::sb::E_Param param) const;

	//! Color for a part of the element (8.5): part * a / 255 * hud_draw
	Color GetColor(const ElementState &state, int partAlpha) const;

	void PaintMeter(const ElementState &state, const CScoreboardData::Element &el);
	void PaintStripCells(int id, const ElementState &state, const CScoreboardData::Element &el);
	void PaintStripTexts(const ElementState &state, const CScoreboardData::Element &el);
	void DrawRoundedBar(int x, int y, int wide, int tall, Color color);
};

#endif
