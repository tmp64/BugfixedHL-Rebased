#ifndef VGUI_SCORE_PANEL_CELLS_H
#define VGUI_SCORE_PANEL_CELLS_H
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
	int iDotSize = 0; //!< Maximum diameter of a dot
	int iDotGap = 0;
	int iStripTall = 0; //!< Height of the steps strip
	int iStripGap = 0; //!< Distance between the steps text and the strip
	int iSpacing = 0; //!< Distance between icon, text, badge and counter
};

/**
 * Image of a custom scoreboard cell: text, icon, meter, steps or place.
 * Draws within the bounds set by the row. An empty cell draws nothing.
 */
class CScoreCellImage : public vgui2::IImage
{
public:
	CScoreCellImage(const ScoreCellStyle &style);

	/**
	 * Sets a value of the store drawn as display (docs/scoreboard-protocol.md, 6.6).
	 * Content setters clear the badge.
	 */
	void SetValue(bhl::sb::E_Display display, const CScoreboardData::Value &value);
	void SetText(const wchar_t *text);

	/**
	 * Sets a place with a round mark behind it.
	 * @param	place		Place, 0 for an empty cell
	 * @param	markColor	Color of the mark, alpha 0 for no mark
	 */
	void SetPlace(int place, Color markColor);

	//! Sets the badge after the text, nullptr for none
	void SetBadge(const wchar_t *badge);

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
		Place,
	};

	const ScoreCellStyle &m_Style;
	Kind m_Kind = Kind::Empty;
	int m_iAlign = 0;
	int m_iX = 0, m_iY = 0;
	int m_iWide = 0, m_iTall = 0;
	int m_iSprite = -1;
	int m_iCur = 0, m_iMax = 0;
	Color m_Color = Color(255, 255, 255, 255);
	Color m_MarkColor = Color(0, 0, 0, 0);
	wchar_t m_wszText[128] = {};
	wchar_t m_wszBadge[32] = {};

	void Clear();
	int GetAlignOffset(int contentWide);
	void PaintText();
	void PaintMeter();
	void PaintSteps();
	void PaintPlace();
};

/**
 * Draws a badge vertically centered on yCenter.
 */
void DrawScoreBadge(const ScoreCellStyle &style, int x, int yCenter, const wchar_t *text);

void DrawCellText(vgui2::HFont font, int x, int y, Color color, const wchar_t *text);

/**
 * Builds a string from a localization token with %s1..%s3.
 * A missing token gives an empty string.
 */
void ConstructScoreText(wchar_t *buf, int bufSizeBytes, const char *token, int count, const wchar_t *arg1, const wchar_t *arg2 = nullptr, const wchar_t *arg3 = nullptr);

/**
 * Builds "N kills" with the plural form of the client language.
 */
void FormatScoreKills(wchar_t *buf, int bufSizeBytes, int kills);

/**
 * Converts the string to uppercase, including Latin-1 and Cyrillic letters (towupper doesn't in the C locale).
 */
void ScoreTextToUpper(wchar_t *text);

#endif
