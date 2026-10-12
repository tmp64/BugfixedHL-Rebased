#include <algorithm>
#include <cmath>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#include "hud.h"
#include "cl_util.h"
#include "hud_renderer.h"
#include "vgui/score_panel_cells.h"

namespace
{

constexpr wchar_t ELLIPSIS = 0x2026;

int GetTextWide(vgui2::HFont font, const wchar_t *text)
{
	int wide = 0;

	for (; *text; text++)
		wide += vgui2::surface()->GetCharacterWidth(font, *text);

	return wide;
}

/**
 * Cuts the text with an ellipsis to fit into maxWide. Returns the width of the result.
 */
int FitText(vgui2::HFont font, wchar_t *text, int maxWide)
{
	int wide = GetTextWide(font, text);

	if (wide <= maxWide)
		return wide;

	int ellipsisWide = vgui2::surface()->GetCharacterWidth(font, ELLIPSIS);
	int len = 0;
	wide = 0;

	for (; text[len]; len++)
	{
		int charWide = vgui2::surface()->GetCharacterWidth(font, text[len]);

		if (wide + charWide + ellipsisWide > maxWide)
			break;

		wide += charWide;
	}

	text[len] = ELLIPSIS;
	text[len + 1] = L'\0';
	return wide + ellipsisWide;
}

/**
 * Draws a box with round corners line by line, so it doesn't depend on alpha blending.
 */
void DrawRoundedBox(int x0, int y0, int x1, int y1, int radius, Color color)
{
	radius = std::min(radius, std::min(x1 - x0, y1 - y0) / 2);
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawFilledRect(x0, y0 + radius, x1, y1 - radius);

	for (int i = 0; i < radius; i++)
	{
		float dy = radius - i - 0.5f;
		int inset = (int)(radius - sqrtf(radius * radius - dy * dy) + 0.5f);
		vgui2::surface()->DrawFilledRect(x0 + inset, y0 + i, x1 - inset, y0 + i + 1);
		vgui2::surface()->DrawFilledRect(x0 + inset, y1 - i - 1, x1 - inset, y1 - i);
	}
}

int GetBadgePadding(const ScoreCellStyle &style)
{
	return vgui2::surface()->GetFontTall(style.hBadgeFont) / 3;
}

int GetScoreBadgeWide(const ScoreCellStyle &style, const wchar_t *text)
{
	return GetTextWide(style.hBadgeFont, text) + 2 * GetBadgePadding(style);
}

}

CScoreCellImage::CScoreCellImage(const ScoreCellStyle &style)
    : m_Style(style)
{
}

void CScoreCellImage::Clear()
{
	m_Kind = Kind::Empty;
	m_iSprite = -1;
	m_iCur = 0;
	m_iMax = 0;
	m_MarkColor = Color(0, 0, 0, 0);
	m_wszText[0] = L'\0';
	m_wszBadge[0] = L'\0';
}

void CScoreCellImage::SetValue(bhl::sb::E_Display display, const CScoreboardData::Value &value)
{
	Clear();

	if (value.IsEmpty())
		return;

	if (value.type == bhl::sb::E_ValueType::Progress && value.iMax > 0
	    && (display == bhl::sb::E_Display::Meter || display == bhl::sb::E_Display::Steps))
	{
		m_Kind = display == bhl::sb::E_Display::Meter ? Kind::Meter : Kind::Steps;
		m_iCur = value.iValue;
		m_iMax = value.iMax;
		return;
	}

	m_Kind = Kind::Text;
	value.ToString(m_wszText, sizeof(m_wszText));

	if (value.type == bhl::sb::E_ValueType::Icon && display == bhl::sb::E_Display::Icon && value.szSprite[0])
	{
		m_Kind = Kind::Icon;
		m_iSprite = gHUD.GetSpriteIndex(value.szSprite);
	}
}

void CScoreCellImage::SetText(const wchar_t *text)
{
	Clear();
	m_Kind = Kind::Text;
	V_wcsncpy(m_wszText, text, sizeof(m_wszText));
}

void CScoreCellImage::SetPlace(int place, Color markColor)
{
	Clear();

	if (place <= 0)
		return;

	m_Kind = Kind::Place;
	m_iCur = place;
	m_MarkColor = markColor;
}

void CScoreCellImage::SetBadge(const wchar_t *badge)
{
	V_wcsncpy(m_wszBadge, badge ? badge : L"", sizeof(m_wszBadge));
}

void CScoreCellImage::SetAlign(int align)
{
	m_iAlign = align;
}

void CScoreCellImage::Paint()
{
	switch (m_Kind)
	{
	case Kind::Text:
	case Kind::Icon:
		PaintText();
		break;
	case Kind::Meter:
		PaintMeter();
		break;
	case Kind::Steps:
		PaintSteps();
		break;
	case Kind::Place:
		PaintPlace();
		break;
	case Kind::Empty:
		break;
	}
}

void CScoreCellImage::SetPos(int x, int y)
{
	m_iX = x;
	m_iY = y;
}

void CScoreCellImage::GetContentSize(int &wide, int &tall)
{
	wide = m_iWide;
	tall = m_iTall;
}

void CScoreCellImage::GetSize(int &wide, int &tall)
{
	GetContentSize(wide, tall);
}

void CScoreCellImage::SetSize(int wide, int tall)
{
	m_iWide = wide;
	m_iTall = tall;
}

void CScoreCellImage::SetColor(Color col)
{
	m_Color = col;
}

int CScoreCellImage::GetAlignOffset(int contentWide)
{
	int freeWide = std::max(m_iWide - contentWide, 0);

	if (m_iAlign == 1)
		return freeWide / 2;
	else if (m_iAlign == 2)
		return freeWide;

	return 0;
}

void CScoreCellImage::PaintText()
{
	vgui2::HFont font = m_Style.hFont;
	int spacing = m_Style.iSpacing;
	int yCenter = m_iY + m_iTall / 2;

	// Sprite scaled to the row height with its aspect ratio kept
	wrect_t rc = {};
	int iconWide = 0;
	int iconTall = 0;

	if (m_Kind == Kind::Icon && m_iSprite != -1 && CHudRenderer::Get().IsAvailable())
	{
		rc = gHUD.GetSpriteRect(m_iSprite);

		if (rc.GetWidth() > 0 && rc.GetHeight() > 0)
		{
			iconTall = std::max(m_iTall - 2 * spacing, 1);
			iconWide = rc.GetWidth() * iconTall / rc.GetHeight();

			if (iconWide > m_iWide)
			{
				iconWide = m_iWide;
				iconTall = rc.GetHeight() * iconWide / rc.GetWidth();
			}
		}
	}

	int iconSpace = iconWide > 0 ? iconWide + spacing : 0;
	int badgeSpace = m_wszBadge[0] ? GetScoreBadgeWide(m_Style, m_wszBadge) + spacing : 0;

	wchar_t text[ARRAYSIZE(m_wszText)];
	V_wcsncpy(text, m_wszText, sizeof(text));
	int textWide = FitText(font, text, std::max(m_iWide - iconSpace - badgeSpace, 0));

	int x = m_iX + GetAlignOffset(iconSpace + textWide + badgeSpace);

	if (iconWide > 0 && iconTall > 0)
	{
		CHudRenderer::SpriteSet(gHUD.GetSprite(m_iSprite), m_Color.r(), m_Color.g(), m_Color.b());
		CHudRenderer::SpriteDrawAdditiveScaled(0, x, yCenter - iconTall / 2, iconWide, iconTall, &rc);
		x += iconSpace;
	}

	DrawCellText(font, x, yCenter - vgui2::surface()->GetFontTall(font) / 2, m_Color, text);

	if (badgeSpace > 0)
		DrawScoreBadge(m_Style, x + textWide + spacing, yCenter, m_wszBadge);
}

void CScoreCellImage::PaintMeter()
{
	vgui2::HFont font = m_Style.hFont;
	wchar_t counter[32];
	V_snwprintf(counter, ARRAYSIZE(counter), L"%d/%d", m_iCur, m_iMax);

	int counterWide = GetTextWide(font, counter);
	int spacing = m_Style.iSpacing;
	int gap = m_Style.iDotGap;
	int maxDots = bhl::sb::MAX_DOTS;
	int dot = (m_iWide - counterWide - spacing - (maxDots - 1) * gap) / maxDots;
	dot = std::min(std::min(dot, m_Style.iDotSize), m_iTall);

	// The bar has the length of a full row of dots
	int barWide = dot > 0 ? maxDots * dot + (maxDots - 1) * gap : 0;
	int x = m_iX + GetAlignOffset(barWide + spacing + counterWide);
	int y = m_iY + (m_iTall - dot) / 2;
	int cur = std::clamp(m_iCur, 0, m_iMax);

	if (dot > 0 && m_iMax <= maxDots)
	{
		for (int i = 0; i < m_iMax; i++)
		{
			int dotX = x + i * (dot + gap);
			DrawRoundedBox(dotX, y, dotX + dot, y + dot, dot / 2, i < cur ? m_Style.filledColor : m_Style.emptyColor);
		}
	}
	else if (dot > 0)
	{
		DrawRoundedBox(x, y, x + barWide, y + dot, dot / 2, m_Style.emptyColor);

		if (cur > 0)
			DrawRoundedBox(x, y, x + std::max(barWide * cur / m_iMax, dot), y + dot, dot / 2, m_Style.filledColor);
	}

	DrawCellText(font, x + barWide + spacing, m_iY + (m_iTall - vgui2::surface()->GetFontTall(font)) / 2, m_Color, counter);
}

void CScoreCellImage::PaintSteps()
{
	vgui2::HFont font = m_Style.hFont;
	wchar_t text[32];
	V_snwprintf(text, ARRAYSIZE(text), L"%d/%d", m_iCur, m_iMax);

	int textTall = vgui2::surface()->GetFontTall(font);
	int stripTall = m_Style.iStripTall;
	int y = m_iY + (m_iTall - (textTall + m_Style.iStripGap + stripTall)) / 2;
	DrawCellText(font, m_iX + GetAlignOffset(GetTextWide(font, text)), y, m_Color, text);

	int stripY = y + textTall + m_Style.iStripGap;

	if (m_iMax * 2 > m_iWide)
	{
		// Too many steps for separate segments
		int fillWide = m_iWide * std::clamp(m_iCur, 0, m_iMax) / m_iMax;
		vgui2::surface()->DrawSetColor(m_Style.filledColor);
		vgui2::surface()->DrawFilledRect(m_iX, stripY, m_iX + fillWide, stripY + stripTall);
		vgui2::surface()->DrawSetColor(m_Style.emptyColor);
		vgui2::surface()->DrawFilledRect(m_iX + fillWide, stripY, m_iX + m_iWide, stripY + stripTall);
		return;
	}

	int gap = m_iMax * 3 <= m_iWide ? 1 : 0;

	for (int i = 0; i < m_iMax; i++)
	{
		int x0 = m_iX + i * (m_iWide + gap) / m_iMax;
		int x1 = m_iX + (i + 1) * (m_iWide + gap) / m_iMax - gap;
		Color color;

		if (i == m_iCur - 1)
			color = m_Style.currentColor;
		else if (i < m_iCur - 1)
			color = m_Style.filledColor;
		else if (i == m_iMax - 1)
			color = m_Style.lastColor;
		else
			color = m_Style.emptyColor;

		vgui2::surface()->DrawSetColor(color);
		vgui2::surface()->DrawFilledRect(x0, stripY, x1, stripY + stripTall);
	}
}

void CScoreCellImage::PaintPlace()
{
	vgui2::HFont font = m_Style.hFont;
	wchar_t text[16];
	V_snwprintf(text, ARRAYSIZE(text), L"%d", m_iCur);

	int textWide = GetTextWide(font, text);
	int textTall = vgui2::surface()->GetFontTall(font);
	int textY = m_iY + (m_iTall - textTall) / 2;

	if (m_MarkColor.a() == 0)
	{
		DrawCellText(font, m_iX + GetAlignOffset(textWide), textY, m_Color, text);
		return;
	}

	int size = std::min(std::max(textWide, textTall) + m_Style.iSpacing, m_iTall);
	int x = m_iX + GetAlignOffset(size);
	int y = m_iY + (m_iTall - size) / 2;
	DrawRoundedBox(x, y, x + size, y + size, size / 2, m_MarkColor);
	DrawCellText(font, x + (size - textWide) / 2, textY, m_Color, text);
}

void DrawScoreBadge(const ScoreCellStyle &style, int x, int yCenter, const wchar_t *text)
{
	int padding = GetBadgePadding(style);
	int tall = vgui2::surface()->GetFontTall(style.hBadgeFont);
	int y = yCenter - tall / 2;

	DrawRoundedBox(x, y, x + GetScoreBadgeWide(style, text), y + tall, padding, style.badgeBgColor);
	DrawCellText(style.hBadgeFont, x + padding, y, style.badgeFgColor, text);
}

void DrawCellText(vgui2::HFont font, int x, int y, Color color, const wchar_t *text)
{
	vgui2::surface()->DrawSetTextFont(font);
	vgui2::surface()->DrawSetTextColor(color);
	vgui2::surface()->DrawSetTextPos(x, y);
	vgui2::surface()->DrawPrintText(text, wcslen(text));
}

void ConstructScoreText(wchar_t *buf, int bufSizeBytes, const char *token, int count, const wchar_t *arg1, const wchar_t *arg2, const wchar_t *arg3)
{
	wchar_t *format = g_pVGuiLocalize->Find(token);

	if (!format)
	{
		buf[0] = L'\0';
		return;
	}

	g_pVGuiLocalize->ConstructString(buf, bufSizeBytes, format, count, arg1, arg2, arg3);
}

void FormatScoreKills(wchar_t *buf, int bufSizeBytes, int kills)
{
	char token[32];
	wchar_t number[16];
	snprintf(token, sizeof(token), "BHL_GG_Kills%s", CScoreboardData::GetPluralSuffix(kills));
	V_snwprintf(number, ARRAYSIZE(number), L"%d", kills);
	ConstructScoreText(buf, bufSizeBytes, token, 1, number);
}

void ScoreTextToUpper(wchar_t *text)
{
	for (; *text; text++)
	{
		wchar_t c = *text;

		if ((c >= L'a' && c <= L'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7) || (c >= 0x430 && c <= 0x44F))
			*text = c - 0x20;
		else if (c >= 0x450 && c <= 0x45F)
			*text = c - 0x50;
	}
}
