#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#include <com_model.h>
#include "hud.h"
#include "cl_util.h"
#include "vgui/score_panel_cells.h"

namespace
{

constexpr wchar_t ELLIPSIS = 0x2026;

constexpr int SPRITE_IDENT = ('P' << 24) | ('S' << 16) | ('D' << 8) | 'I';
constexpr int SPRITE_VERSION = 2;
constexpr int SPRITE_INDEXALPHA = 2;
constexpr int SPRITE_ALPHATEST = 3;
constexpr int SPRITE_MAX_SIZE = 1024;

constexpr int TEXTURE_NOT_CREATED = -2;
constexpr int TEXTURE_FAILED = -1;

//! VGUI2 textures of hud.txt sprites by sprite index
std::vector<int> s_SpriteTextures;

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

/**
 * Reads rectangle rc of the first frame of a sprite file as RGBA.
 * Alpha is the brightness and colors are scaled up by it, so blending looks like additive drawing.
 */
bool LoadSpriteRGBA(const char *path, const wrect_t &rc, std::vector<unsigned char> &rgba, int &wide, int &tall)
{
	int length = 0;
	unsigned char *file = gEngfuncs.COM_LoadFile(path, 5, &length);

	if (!file)
		return false;

	int pos = 0;
	auto fnRead = [&](void *dest, int size) {
		if (size < 0 || pos + size > length)
			return false;

		memcpy(dest, file + pos, size);
		pos += size;
		return true;
	};

	// dsprite_t: ident, version, type, texFormat, boundingradius, width, height, numframes, beamlength, synctype
	int header[10];
	short paletteSize = 0;
	unsigned char palette[256][3];
	int frameType = 0;
	int frame[4]; // origin x, origin y, width, height

	bool bValid = fnRead(header, sizeof(header)) && header[0] == SPRITE_IDENT && header[1] == SPRITE_VERSION && header[7] > 0
	    && fnRead(&paletteSize, sizeof(paletteSize)) && paletteSize > 0 && paletteSize <= 256
	    && fnRead(palette, paletteSize * 3) && fnRead(&frameType, sizeof(frameType)) && frameType == 0
	    && fnRead(frame, sizeof(frame)) && frame[2] > 0 && frame[3] > 0 && frame[2] <= SPRITE_MAX_SIZE && frame[3] <= SPRITE_MAX_SIZE
	    && pos + frame[2] * frame[3] <= length;

	if (bValid)
	{
		int texFormat = header[3];
		int x0 = std::clamp(rc.left, 0, frame[2]);
		int x1 = std::clamp(rc.right, x0, frame[2]);
		int y0 = std::clamp(rc.top, 0, frame[3]);
		int y1 = std::clamp(rc.bottom, y0, frame[3]);
		const unsigned char *pixels = file + pos;
		wide = x1 - x0;
		tall = y1 - y0;
		// Transparent texels are white, so filtering doesn't darken the edges
		rgba.assign(wide * tall * 4, 255);

		for (int y = 0; y < tall; y++)
		{
			for (int x = 0; x < wide; x++)
			{
				int index = pixels[(y0 + y) * frame[2] + x0 + x];
				int color[3];

				for (int i = 0; i < 3; i++)
				{
					if (texFormat == SPRITE_INDEXALPHA)
						color[i] = paletteSize == 256 ? palette[255][i] * index / 255 : 0;
					else if (texFormat == SPRITE_ALPHATEST && index == 255)
						color[i] = 0;
					else
						color[i] = index < paletteSize ? palette[index][i] : 0;
				}

				int alpha = std::max(std::max(color[0], color[1]), color[2]);
				unsigned char *out = &rgba[(y * wide + x) * 4];

				for (int i = 0; i < 3 && alpha > 0; i++)
					out[i] = static_cast<unsigned char>(color[i] * 255 / alpha);

				out[3] = static_cast<unsigned char>(alpha);
			}
		}

		bValid = wide > 0 && tall > 0;
	}

	gEngfuncs.COM_FreeFile(file);
	return bValid;
}

int CreateSpriteTexture(int sprite)
{
	const model_t *model = gEngfuncs.GetSpritePointer(gHUD.GetSprite(sprite));
	std::vector<unsigned char> rgba;
	int wide = 0;
	int tall = 0;

	if (!model || !LoadSpriteRGBA(model->name, gHUD.GetSpriteRect(sprite), rgba, wide, tall))
		return TEXTURE_FAILED;

	int id = vgui2::surface()->CreateNewTextureID(true);
	vgui2::surface()->DrawSetTextureRGBA(id, rgba.data(), wide, tall, true, false);
	return id;
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
	int iconWide = 0;
	int iconTall = 0;

	if (m_Kind == Kind::Icon && m_iSprite != -1)
	{
		const wrect_t &rc = gHUD.GetSpriteRect(m_iSprite);

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
		DrawScoreSprite(m_iSprite, x, yCenter - iconTall / 2, iconWide, iconTall, m_Color);
		x += iconSpace;
	}

	DrawCellText(font, x, yCenter - vgui2::surface()->GetFontTall(font) / 2, m_Color, text);

	if (badgeSpace > 0)
		DrawScoreBadge(m_Style, x + textWide + spacing, yCenter, m_wszBadge);
}

void CScoreCellImage::PaintMeter()
{
	vgui2::HFont font = m_Style.hFont;
	int maxDots = bhl::sb::MAX_DOTS;
	bool bBar = m_iMax > maxDots;

	// Only the bar has a counter: dots are easy to count
	wchar_t counter[32] = L"";
	int counterSpace = 0;

	if (bBar)
	{
		V_snwprintf(counter, ARRAYSIZE(counter), L"%d/%d", m_iCur, m_iMax);
		counterSpace = m_Style.iSpacing + GetTextWide(font, counter);
	}

	int gap = m_Style.iDotGap;
	int dot = (m_iWide - counterSpace - (maxDots - 1) * gap) / maxDots;
	dot = std::min(std::min(dot, m_Style.iDotSize), m_iTall);

	// The bar has the length of a full row of dots
	int barWide = dot > 0 ? maxDots * dot + (maxDots - 1) * gap : 0;
	int contentWide = bBar ? barWide + counterSpace : std::max(m_iMax * (dot + gap) - gap, 0);
	int x = m_iX + GetAlignOffset(contentWide);
	int y = m_iY + (m_iTall - dot) / 2;
	int cur = std::clamp(m_iCur, 0, m_iMax);

	if (dot > 0 && !bBar)
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

	if (bBar)
		DrawCellText(font, x + barWide + m_Style.iSpacing, m_iY + (m_iTall - vgui2::surface()->GetFontTall(font)) / 2, m_Color, counter);
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

void DrawScoreBadge(const ScoreCellStyle &style, int x, int yCenter, const wchar_t *text)
{
	int padding = GetBadgePadding(style);
	int tall = vgui2::surface()->GetFontTall(style.hBadgeFont);
	int y = yCenter - tall / 2;

	DrawRoundedBox(x, y, x + GetScoreBadgeWide(style, text), y + tall, padding, style.badgeBgColor);
	DrawCellText(style.hBadgeFont, x + padding, y, style.badgeFgColor, text);
}

int GetScoreBadgeWide(const ScoreCellStyle &style, const wchar_t *text)
{
	return GetTextWide(style.hBadgeFont, text) + 2 * GetBadgePadding(style);
}

void DrawScoreSprite(int sprite, int x, int y, int wide, int tall, Color color)
{
	if (sprite < 0 || wide <= 0 || tall <= 0)
		return;

	if (sprite >= static_cast<int>(s_SpriteTextures.size()))
		s_SpriteTextures.resize(sprite + 1, TEXTURE_NOT_CREATED);

	if (s_SpriteTextures[sprite] == TEXTURE_NOT_CREATED)
		s_SpriteTextures[sprite] = CreateSpriteTexture(sprite);

	if (s_SpriteTextures[sprite] == TEXTURE_FAILED)
		return;

	vgui2::surface()->DrawSetTexture(s_SpriteTextures[sprite]);
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawTexturedRect(x, y, x + wide, y + tall);
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
