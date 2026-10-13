#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <map>
#include <vector>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include <com_model.h>
#include "hud.h"
#include "cl_util.h"
#include "hud_renderer.h"
#include "vgui/score_panel_cells.h"

namespace
{

constexpr wchar_t ELLIPSIS = 0x2026;

constexpr int SPRITE_IDENT = ('P' << 24) | ('S' << 16) | ('D' << 8) | 'I';
constexpr int SPRITE_VERSION = 2;
constexpr int SPRITE_INDEXALPHA = 2;
constexpr int SPRITE_ALPHATEST = 3;
constexpr int SPRITE_MAX_SIZE = 1024;

//! Without blending, darker texels of an additive sprite would draw as a dark fringe
constexpr int SPRITE_ALPHA_TEST_MIN = 48;

//! Sprite textures of the software renderer, one per drawn size; VGUI2 can't delete textures, so they are reused
constexpr int MAX_SPRITE_TEXTURES = 64;

constexpr int TEXTURE_NOT_CREATED = -2;
constexpr int TEXTURE_FAILED = -1;

struct SpriteTexture
{
	int id = TEXTURE_NOT_CREATED;
	unsigned lastUse = 0;
};

//! Sprite textures by sprite index, wide and tall
std::map<std::array<int, 3>, SpriteTexture> s_SpriteTextures;
unsigned s_iSpriteTextureUse = 0;

int GetBadgePadding(const ScoreCellStyle &style)
{
	return vgui2::surface()->GetFontTall(style.hBadgeFont) / 3;
}

/**
 * Reads the colors of rectangle rc of the first frame of a sprite file, 3 bytes per texel.
 */
bool LoadSpriteRGB(const char *path, const wrect_t &rc, std::vector<unsigned char> &rgb, int &wide, int &tall)
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
		rgb.assign(wide * tall * 3, 0);

		for (int y = 0; y < tall; y++)
		{
			for (int x = 0; x < wide; x++)
			{
				int index = pixels[(y0 + y) * frame[2] + x0 + x];
				unsigned char *out = &rgb[(y * wide + x) * 3];

				for (int i = 0; i < 3; i++)
				{
					if (texFormat == SPRITE_INDEXALPHA)
						out[i] = paletteSize == 256 ? palette[255][i] * index / 255 : 0;
					else if (texFormat == SPRITE_ALPHATEST && index == 255)
						out[i] = 0;
					else
						out[i] = index < paletteSize ? palette[index][i] : 0;
				}
			}
		}

		bValid = wide > 0 && tall > 0;
	}

	gEngfuncs.COM_FreeFile(file);
	return bValid;
}

//! Sprite files have indexes -2, -3, ...: gHUD.GetSprite and gHUD.GetSpriteRect take them as no sprite
constexpr int FILE_SPRITE_FIRST = -2;

//! The engine has 256 HUD sprite slots and stops with an error when they run out
constexpr int MAX_FILE_SPRITES = 32;

struct FileSprite
{
	char szPath[bhl::sb::MAX_TEXT_LEN + 1] = {};
	HSPRITE hSprite = 0;
	wrect_t rc = {};
	bool bTried = false; //!< Loaded or failed since the last reset
};

//! Sprite files by index, kept for the whole game so an index always means the same path
FileSprite s_FileSprites[MAX_FILE_SPRITES];
int s_iFileSpriteCount = 0;

FileSprite *GetFileSprite(int sprite)
{
	int i = FILE_SPRITE_FIRST - sprite;
	return i >= 0 && i < s_iFileSpriteCount ? &s_FileSprites[i] : nullptr;
}

HSPRITE GetSpriteHandle(int sprite)
{
	if (sprite >= 0)
		return gHUD.GetSprite(sprite);

	FileSprite *pFile = GetFileSprite(sprite);
	return pFile ? pFile->hSprite : 0;
}

/**
 * The engine stops with an error on a missing file when developer is above 1, and on a broken one always:
 * the file is read and checked first.
 */
void LoadFileSprite(FileSprite &file)
{
	std::vector<unsigned char> rgb;
	int wide = 0;
	int tall = 0;

	if (!LoadSpriteRGB(file.szPath, wrect_t { 0, SPRITE_MAX_SIZE, 0, SPRITE_MAX_SIZE }, rgb, wide, tall))
		return;

	file.hSprite = gEngfuncs.pfnSPR_Load(file.szPath);
	file.rc = wrect_t { 0, wide, 0, tall };
}

struct ScaleTap
{
	int index;
	float weight;
};

/**
 * Returns the source texels of destination pixel i when srcSize texels are scaled to dstSize pixels:
 * the two nearest ones with linear weights when the image grows, the covered ones when it shrinks.
 */
std::vector<ScaleTap> GetScaleTaps(int i, int srcSize, int dstSize)
{
	std::vector<ScaleTap> taps;
	float scale = (float)srcSize / dstSize;

	if (scale <= 1.0f)
	{
		float center = (i + 0.5f) * scale - 0.5f;
		int i0 = (int)floorf(center);
		float frac = center - i0;
		taps.push_back({ std::clamp(i0, 0, srcSize - 1), 1.0f - frac });
		taps.push_back({ std::clamp(i0 + 1, 0, srcSize - 1), frac });
	}
	else
	{
		int i0 = (int)(i * scale);
		int i1 = std::clamp((int)((i + 1) * scale), i0 + 1, srcSize);

		for (int j = i0; j < i1; j++)
			taps.push_back({ j, 1.0f / (i1 - i0) });
	}

	return taps;
}

/**
 * Scales the sprite colors to the drawn size, so the software renderer doesn't scale them into blocks.
 * Pixels bright enough are opaque, the rest transparent: the texture is drawn without blending.
 */
void ScaleSpriteToRGBA(const std::vector<unsigned char> &rgb, int srcWide, int srcTall, int wide, int tall, std::vector<unsigned char> &rgba)
{
	std::vector<std::vector<ScaleTap>> tapsX(wide);
	std::vector<std::vector<ScaleTap>> tapsY(tall);

	for (int x = 0; x < wide; x++)
		tapsX[x] = GetScaleTaps(x, srcWide, wide);

	for (int y = 0; y < tall; y++)
		tapsY[y] = GetScaleTaps(y, srcTall, tall);

	rgba.assign(wide * tall * 4, 0);

	for (int y = 0; y < tall; y++)
	{
		for (int x = 0; x < wide; x++)
		{
			float color[3] = {};

			for (const ScaleTap &ty : tapsY[y])
			{
				for (const ScaleTap &tx : tapsX[x])
				{
					const unsigned char *texel = &rgb[(ty.index * srcWide + tx.index) * 3];

					for (int i = 0; i < 3; i++)
						color[i] += texel[i] * tx.weight * ty.weight;
				}
			}

			unsigned char *out = &rgba[(y * wide + x) * 4];

			if (std::max({ color[0], color[1], color[2] }) < SPRITE_ALPHA_TEST_MIN)
				continue;

			for (int i = 0; i < 3; i++)
				out[i] = static_cast<unsigned char>(std::clamp(color[i] + 0.5f, 0.0f, 255.0f));

			out[3] = 255;
		}
	}
}

int GetSpriteTexture(int sprite, int wide, int tall)
{
	SpriteTexture &texture = s_SpriteTextures[{ sprite, wide, tall }];
	texture.lastUse = ++s_iSpriteTextureUse;

	if (texture.id != TEXTURE_NOT_CREATED)
		return texture.id;

	texture.id = TEXTURE_FAILED;

	const model_t *model = gEngfuncs.GetSpritePointer(GetSpriteHandle(sprite));
	std::vector<unsigned char> rgb;
	std::vector<unsigned char> rgba;
	int srcWide = 0;
	int srcTall = 0;

	if (!model || !LoadSpriteRGB(model->name, GetScoreSpriteRect(sprite), rgb, srcWide, srcTall))
		return TEXTURE_FAILED;

	ScaleSpriteToRGBA(rgb, srcWide, srcTall, wide, tall, rgba);

	// Over the limit the texture of the least recently drawn size is reused
	int id = TEXTURE_FAILED;

	if (s_SpriteTextures.size() > MAX_SPRITE_TEXTURES)
	{
		auto oldest = s_SpriteTextures.end();

		for (auto it = s_SpriteTextures.begin(); it != s_SpriteTextures.end(); ++it)
		{
			if (it->second.id >= 0 && (oldest == s_SpriteTextures.end() || it->second.lastUse < oldest->second.lastUse))
				oldest = it;
		}

		if (oldest != s_SpriteTextures.end())
		{
			id = oldest->second.id;
			s_SpriteTextures.erase(oldest);
		}
	}

	bool bReused = id != TEXTURE_FAILED;

	if (!bReused)
		id = vgui2::surface()->CreateNewTextureID(true);

	vgui2::surface()->DrawSetTextureRGBA(id, rgba.data(), wide, tall, false, bReused);
	texture.id = id;
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
	m_bMarkLast = false;
	m_iSprite = -1;
	m_iCur = 0;
	m_iMax = 0;
	m_wszText[0] = L'\0';
	m_wszBadges[0] = L'\0';
	m_iBadgesLen = 0;
	m_iBadgeCount = 0;
}

void CScoreCellImage::SetValue(const CScoreboardData::Column &column, const CScoreboardData::Value &value)
{
	Clear();
	m_bMarkLast = column.IsMarkLast();

	if (value.IsEmpty())
		return;

	bhl::sb::E_Display display = column.display;

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
		m_iSprite = GetScoreSpriteIndex(value.szSprite);
	}
}

void CScoreCellImage::SetText(const wchar_t *text)
{
	Clear();

	if (text[0])
	{
		m_Kind = Kind::Text;
		V_wcsncpy(m_wszText, text, sizeof(m_wszText));
	}
}

void CScoreCellImage::AddBadge(const wchar_t *text)
{
	int len = static_cast<int>(wcslen(text));

	if (len == 0 || m_iBadgesLen + len + 1 > BADGES_LEN)
		return;

	memcpy(m_wszBadges + m_iBadgesLen, text, (len + 1) * sizeof(wchar_t));
	m_iBadgesLen += len + 1;
	m_iBadgeCount++;
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
		// Badges come from cells of their own: they are drawn even when the host cell is empty
		if (m_iBadgeCount > 0)
			PaintText();
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

int CScoreCellImage::GetAreaWide() const
{
	return std::max(m_iWide - m_Style.iMargin, 0);
}

int CScoreCellImage::GetAlignOffset(int contentWide, int areaWide)
{
	int freeWide = std::max(areaWide - contentWide, 0);

	if (m_iAlign == 1)
		return freeWide / 2;
	else if (m_iAlign == 2)
		return freeWide;

	return 0;
}

int CScoreCellImage::GetBadgesWide(int maxWide, int &count)
{
	const wchar_t *badge = m_wszBadges;
	int wide = 0;
	count = 0;

	for (int i = 0; i < m_iBadgeCount; i++)
	{
		int badgeWide = m_Style.iSpacing + GetScoreBadgeWide(m_Style, badge);

		if (wide + badgeWide > maxWide)
			break;

		wide += badgeWide;
		count++;
		badge += wcslen(badge) + 1;
	}

	return wide;
}

void CScoreCellImage::PaintBadges(int x, int yCenter, int count)
{
	const wchar_t *badge = m_wszBadges;

	for (int i = 0; i < count; i++)
	{
		x += m_Style.iSpacing;
		DrawScoreBadge(m_Style, x, yCenter, badge);
		x += GetScoreBadgeWide(m_Style, badge);
		badge += wcslen(badge) + 1;
	}
}

void CScoreCellImage::PaintText()
{
	vgui2::HFont font = m_Style.hFont;
	int spacing = m_Style.iSpacing;
	int yCenter = m_iY + m_iTall / 2;
	int areaWide = GetAreaWide();

	// Sprite scaled to the row height with its aspect ratio kept
	int iconWide = 0;
	int iconTall = 0;

	if (m_Kind == Kind::Icon && m_iSprite != -1)
	{
		const wrect_t &rc = GetScoreSpriteRect(m_iSprite);

		if (rc.GetWidth() > 0 && rc.GetHeight() > 0)
		{
			iconTall = std::max(m_iTall - 2 * spacing, 1);
			iconWide = rc.GetWidth() * iconTall / rc.GetHeight();

			if (iconWide > areaWide)
			{
				iconWide = areaWide;
				iconTall = rc.GetHeight() * iconWide / rc.GetWidth();
			}
		}
	}

	// Badges come first: the text is cut to the rest
	int iconSpace = iconWide > 0 ? iconWide + spacing : 0;
	int badgeCount = 0;
	int badgesWide = GetBadgesWide(areaWide - iconSpace, badgeCount);

	wchar_t text[ARRAYSIZE(m_wszText)];
	V_wcsncpy(text, m_wszText, sizeof(text));
	int textWide = FitCellText(font, text, std::max(areaWide - iconSpace - badgesWide, 0));

	int x = m_iX + GetAlignOffset(iconSpace + textWide + badgesWide, areaWide);

	if (iconWide > 0 && iconTall > 0)
	{
		DrawScoreSprite(m_iSprite, x, yCenter - iconTall / 2, iconWide, iconTall, m_Style.iconColor);
		x += iconSpace;
	}

	// The text stands where a label puts it, so it lines up with text columns
	if (text[0])
		DrawCellText(font, x, m_iY + (m_iTall - vgui2::surface()->GetFontTall(font)) / 2, m_Color, text);

	PaintBadges(x + textWide, yCenter, badgeCount);
}

void CScoreCellImage::PaintMeter()
{
	vgui2::HFont font = m_Style.hFont;
	int maxDots = bhl::sb::MAX_DOTS;
	bool bBar = m_iMax > maxDots;
	int badgeCount = 0;
	int cellWide = GetAreaWide();
	int badgesWide = GetBadgesWide(cellWide, badgeCount);
	int areaWide = cellWide - badgesWide;

	// Only the bar has a counter: dots are easy to count
	wchar_t counter[32] = L"";
	int counterSpace = 0;

	if (bBar)
	{
		V_snwprintf(counter, ARRAYSIZE(counter), L"%d/%d", m_iCur, m_iMax);
		counterSpace = m_Style.iSpacing + GetCellTextWide(font, counter);
	}

	int gap = m_Style.iDotGap;
	int dot = (areaWide - counterSpace - (maxDots - 1) * gap) / maxDots;
	dot = std::min(std::min(dot, m_Style.iDotSize), m_iTall);

	// The bar has the length of a full row of dots
	int barWide = dot > 0 ? maxDots * dot + (maxDots - 1) * gap : 0;
	int contentWide = bBar ? barWide + counterSpace : std::max(m_iMax * (dot + gap) - gap, 0);
	int x = m_iX + GetAlignOffset(contentWide + badgesWide, cellWide);
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

	PaintBadges(x + contentWide, m_iY + m_iTall / 2, badgeCount);
}

void CScoreCellImage::PaintSteps()
{
	vgui2::HFont font = m_Style.hFont;
	wchar_t text[32];
	V_snwprintf(text, ARRAYSIZE(text), L"%d/%d", m_iCur, m_iMax);

	// The strip takes the width that the badges leave
	int badgeCount = 0;
	int cellWide = GetAreaWide();
	int badgesWide = GetBadgesWide(cellWide, badgeCount);
	int stripWide = cellWide - badgesWide;

	int textTall = vgui2::surface()->GetFontTall(font);
	int stripTall = m_Style.iStripTall;
	int y = m_iY + (m_iTall - (textTall + m_Style.iStripGap + stripTall)) / 2;
	DrawCellText(font, m_iX + GetAlignOffset(GetCellTextWide(font, text), stripWide), y, m_Color, text);
	PaintBadges(m_iX + stripWide, m_iY + m_iTall / 2, badgeCount);

	int stripY = y + textTall + m_Style.iStripGap;

	if (m_iMax * 2 > stripWide)
	{
		// Too many steps for separate segments
		int fillWide = stripWide * std::clamp(m_iCur, 0, m_iMax) / m_iMax;
		vgui2::surface()->DrawSetColor(m_Style.filledColor);
		vgui2::surface()->DrawFilledRect(m_iX, stripY, m_iX + fillWide, stripY + stripTall);
		vgui2::surface()->DrawSetColor(m_Style.emptyColor);
		vgui2::surface()->DrawFilledRect(m_iX + fillWide, stripY, m_iX + stripWide, stripY + stripTall);
		return;
	}

	int gap = m_iMax * 3 <= stripWide ? 1 : 0;

	for (int i = 0; i < m_iMax; i++)
	{
		int x0 = m_iX + i * (stripWide + gap) / m_iMax;
		int x1 = m_iX + (i + 1) * (stripWide + gap) / m_iMax - gap;
		Color color;

		if (i == m_iCur - 1)
			color = m_Style.currentColor;
		else if (i < m_iCur - 1)
			color = m_Style.filledColor;
		else if (m_bMarkLast && i == m_iMax - 1)
			color = m_Style.lastColor;
		else
			color = m_Style.emptyColor;

		vgui2::surface()->DrawSetColor(color);
		vgui2::surface()->DrawFilledRect(x0, stripY, x1, stripY + stripTall);
	}
}

void DrawRoundedBox(int x0, int y0, int x1, int y1, int radius, Color color, int clipX1)
{
	radius = std::min(radius, std::min(x1 - x0, y1 - y0) / 2);
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawFilledRect(x0, y0 + radius, std::min(x1, clipX1), y1 - radius);

	for (int i = 0; i < radius; i++)
	{
		float dy = radius - i - 0.5f;
		int inset = (int)(radius - sqrtf(radius * radius - dy * dy) + 0.5f);
		int right = std::min(x1 - inset, clipX1);

		if (x0 + inset >= right)
			continue;

		vgui2::surface()->DrawFilledRect(x0 + inset, y0 + i, right, y0 + i + 1);
		vgui2::surface()->DrawFilledRect(x0 + inset, y1 - i - 1, right, y1 - i);
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
	return GetCellTextWide(style.hBadgeFont, text) + 2 * GetBadgePadding(style);
}

void DrawScoreSprite(int sprite, int x, int y, int wide, int tall, Color color)
{
	HSPRITE hSprite = GetSpriteHandle(sprite);

	if (hSprite <= 0 || wide <= 0 || tall <= 0 || wide > SPRITE_MAX_SIZE || tall > SPRITE_MAX_SIZE)
		return;

	// Neither way blends with the alpha of the color, a dimmer color stands in for translucency
	int r = color.r() * color.a() / 255;
	int g = color.g() * color.a() / 255;
	int b = color.b() * color.a() / 255;

	if (CHudRenderer::Get().IsAvailable())
	{
		// VGUI2 buffers text and draws it with the texture bound at the flush, which would be the sprite after TriangleAPI
		vgui2::surface()->DrawFlushText();

		// TriangleAPI sets its own blending, while VGUI2 turns blending off once a panel has drawn text
		CHudRenderer::SpriteSet(hSprite, r, g, b);
		CHudRenderer::SpriteDrawAdditiveScaled(0, x, y, wide, tall, &GetScoreSpriteRect(sprite));
		return;
	}

	int texture = GetSpriteTexture(sprite, wide, tall);

	if (texture == TEXTURE_FAILED)
		return;

	vgui2::surface()->DrawSetTexture(texture);
	vgui2::surface()->DrawSetColor(Color(r, g, b, 255));
	vgui2::surface()->DrawTexturedRect(x, y, x + wide, y + tall);
}

void DrawCellText(vgui2::HFont font, int x, int y, Color color, const wchar_t *text)
{
	vgui2::surface()->DrawSetTextFont(font);
	vgui2::surface()->DrawSetTextColor(color);
	vgui2::surface()->DrawSetTextPos(x, y);
	vgui2::surface()->DrawPrintText(text, wcslen(text));
}

int GetCellTextWide(vgui2::HFont font, const wchar_t *text)
{
	int wide = 0;

	for (; *text; text++)
		wide += vgui2::surface()->GetCharacterWidth(font, *text);

	return wide;
}

int FitCellText(vgui2::HFont font, wchar_t *text, int maxWide)
{
	int wide = GetCellTextWide(font, text);

	if (wide <= maxWide)
		return wide;

	int ellipsisWide = vgui2::surface()->GetCharacterWidth(font, ELLIPSIS);

	if (ellipsisWide > maxWide)
	{
		text[0] = L'\0';
		return 0;
	}

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

int GetScoreSpriteIndex(const char *name)
{
	if (!bhl::sb::IsSpriteFilePath(name))
		return gHUD.GetSpriteIndex(name);

	int i = 0;

	while (i < s_iFileSpriteCount && Q_stricmp(s_FileSprites[i].szPath, name))
		i++;

	if (i == s_iFileSpriteCount)
	{
		if (i == MAX_FILE_SPRITES)
			return -1;

		V_strcpy_safe(s_FileSprites[i].szPath, name);
		s_iFileSpriteCount++;
	}

	FileSprite &file = s_FileSprites[i];

	if (!file.bTried)
	{
		file.bTried = true;
		LoadFileSprite(file);
	}

	return file.hSprite > 0 ? FILE_SPRITE_FIRST - i : -1;
}

const wrect_t &GetScoreSpriteRect(int sprite)
{
	static const wrect_t empty = {};

	if (sprite >= 0)
		return gHUD.GetSpriteRect(sprite);

	FileSprite *pFile = GetFileSprite(sprite);
	return pFile && pFile->hSprite > 0 ? pFile->rc : empty;
}

void ResetScoreSpriteFiles()
{
	for (int i = 0; i < s_iFileSpriteCount; i++)
	{
		s_FileSprites[i].hSprite = 0;
		s_FileSprites[i].bTried = false;
	}
}
