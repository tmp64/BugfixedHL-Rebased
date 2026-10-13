#ifndef VGUI_SCORE_PANEL_CELLS_H
#define VGUI_SCORE_PANEL_CELLS_H
#include <climits>
#include <Color.h>
#include <vgui/IImage.h>
#include <vgui/VGUI2.h>
#include "scoreboard_data.h"

/**
 * Fonts, colors and sizes of custom scoreboard cells.
 * Owned by CScorePanel and shared by all cells.
 * Fills are drawn opaque: after text the engine may draw translucent fills without blending.
 */
struct ScoreCellStyle
{
	vgui2::HFont hFont = vgui2::INVALID_FONT;
	vgui2::HFont hBadgeFont = vgui2::INVALID_FONT;
	Color filledColor; //!< Filled dots and bar, passed steps
	Color emptyColor; //!< Empty dots and bar, next steps
	Color currentColor; //!< Current step
	Color lastColor; //!< Last step
	Color badgeBgColor;
	Color badgeFgColor;
	Color iconColor; //!< Tint of sprite icons
	int iDotSize = 0; //!< Maximum diameter of a dot
	int iDotGap = 0;
	int iStripTall = 0; //!< Height of the steps strip
	int iStripGap = 0; //!< Distance between the steps text and the strip
	int iSpacing = 0; //!< Distance between icon, text, badge and counter
	int iMargin = 0; //!< Free width at the right of a cell, so its content doesn't touch the next column
};

/**
 * Image of a custom scoreboard cell: text, icon, meter or steps, followed by badges.
 * Draws within the bounds set by the row or the section header. An empty cell draws only its badges.
 * Texts are drawn as they are: a VGUI2 text image would take '&' for a hotkey mark.
 */
class CScoreCellImage : public vgui2::IImage
{
public:
	CScoreCellImage(const ScoreCellStyle &style);

	/**
	 * Sets a value of the store drawn as the column shows it (docs/scoreboard-protocol.md, 6.6).
	 * Clears the badges.
	 */
	void SetValue(const CScoreboardData::Column &column, const CScoreboardData::Value &value);

	/**
	 * Sets a text, drawn like a Text cell. Clears the badges.
	 */
	void SetText(const wchar_t *text);

	/**
	 * Adds a badge after the content (6.7). Badges are drawn in the order they were added,
	 * as many as fit into the cell. An empty text adds nothing.
	 */
	void AddBadge(const wchar_t *text);

	//! @param	align	0 left, 1 center, 2 right
	void SetAlign(int align);

	// IImage overrides
	virtual void Paint() override;
	virtual void SetPos(int x, int y) override;
	virtual void GetContentSize(int &wide, int &tall) override;
	virtual void GetSize(int &wide, int &tall) override;
	virtual void SetSize(int wide, int tall) override;
	virtual void SetColor(Color col) override;

private:
	enum class Kind
	{
		Empty,
		Text,
		Icon,
		Meter,
		Steps,
	};

	//! Room for the badge texts of one cell, each with its NUL
	static constexpr int BADGES_LEN = 128;

	const ScoreCellStyle &m_Style;
	Kind m_Kind = Kind::Empty;
	bool m_bMarkLast = false;
	int m_iAlign = 0;
	int m_iX = 0, m_iY = 0;
	int m_iWide = 0, m_iTall = 0;
	int m_iSprite = -1;
	int m_iCur = 0, m_iMax = 0;
	Color m_Color = Color(255, 255, 255, 255);
	wchar_t m_wszText[128] = {};
	wchar_t m_wszBadges[BADGES_LEN] = {}; //!< Badge texts one after another
	int m_iBadgesLen = 0; //!< Used part of m_wszBadges
	int m_iBadgeCount = 0;

	void Clear();
	int GetAreaWide() const;
	int GetAlignOffset(int contentWide, int areaWide);

	/**
	 * Returns the width of the first badges that fit into maxWide, each with the spacing before it.
	 * @param	count	Receives the number of these badges
	 */
	int GetBadgesWide(int maxWide, int &count);

	//! Draws the first count badges after x
	void PaintBadges(int x, int yCenter, int count);

	void PaintText();
	void PaintMeter();
	void PaintSteps();
};

/**
 * Draws a box with round corners line by line, so it doesn't depend on alpha blending.
 * @param	clipX1	Only the part left of it is drawn
 */
void DrawRoundedBox(int x0, int y0, int x1, int y1, int radius, Color color, int clipX1 = INT_MAX);

/**
 * Draws a badge vertically centered on yCenter.
 */
void DrawScoreBadge(const ScoreCellStyle &style, int x, int yCenter, const wchar_t *text);

int GetScoreBadgeWide(const ScoreCellStyle &style, const wchar_t *text);

/**
 * Draws a sprite of hud.txt stretched to wide x tall and tinted with color, additively like the HUD.
 * OpenGL draws it with TriangleAPI, which blends on its own. The software renderer draws a VGUI2 texture
 * of that size with an alpha test, because VGUI2 doesn't blend there and after text in a paint pass.
 */
void DrawScoreSprite(int sprite, int x, int y, int wide, int tall, Color color);

/**
 * Sprite of an Icon value for DrawScoreSprite: a name from hud.txt, or the path "sprites/<name>.spr" of a sprite file
 * the server precaches (4.3). A file is loaded on first use and drawn whole. -1 if the client doesn't have the sprite.
 */
int GetScoreSpriteIndex(const char *name);

//! Rectangle of a sprite from GetScoreSpriteIndex, empty for -1 or a file that isn't loaded
const wrect_t &GetScoreSpriteRect(int sprite);

//! Forgets the handles of sprite files: the engine reloads its sprite list on a new map
void ResetScoreSpriteFiles();

void DrawCellText(vgui2::HFont font, int x, int y, Color color, const wchar_t *text);

/**
 * Returns the width of a text drawn by DrawCellText.
 */
int GetCellTextWide(vgui2::HFont font, const wchar_t *text);

/**
 * Cuts a text with an ellipsis to fit into maxWide. Returns the width of the result.
 */
int FitCellText(vgui2::HFont font, wchar_t *text, int maxWide);

#endif
