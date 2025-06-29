#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <bhl/logging/ILogger.h>
#include <bhl/logging/prefix_logger.h>

class CFontManager;

struct FontSettings
{
	std::string fontPath;
	int tall = 0;
	int weight = 0;
	bool italic = false;
	bool antialias = false;
	bool outline = false;
	bool underline = false;
	bool dropshadow = false;
	bool symbol = false;
	bool custom = false;

	bool IsValid() const { return tall > 0; }

	bool operator==(const FontSettings &other) const
	{
		return
		    fontPath == other.fontPath &&
		    tall == other.tall &&
		    weight == other.weight &&
		    italic == other.italic &&
		    antialias == other.antialias &&
		    outline == other.outline &&
		    underline == other.underline &&
		    dropshadow == other.dropshadow &&
		    symbol == other.symbol &&
		    custom == other.custom;
	}

	bool operator!=(const FontSettings &other) const { return !(*this == other); }
};

struct GlyphBitmap
{
	int wide = 0;
	int tall = 0;
	int advance = 0;
	std::vector<uint8_t> data;
	FT_Glyph_Metrics metrics;
};

class CFont
{
public:
	CFont(CFontManager* pFontManager, ILogger* pLogger, const FontSettings& settings);
	
	//! @returns The font settings that were used to create the font.
	const FontSettings &GetSettings() const { return m_Settings; }

	//! Loads the font.
	void LoadFont();

	//! Rasterizes a glyph. Results are cached.
	const GlyphBitmap &RasterizeGlyph(uint32_t codepoint);

private:
	CPrefixLogger m_Logger;
	CFontManager *m_pFontManager = nullptr;
	FontSettings m_Settings;
	std::vector<uint8_t> m_FontFileData;
	FT_Face m_Face = {};
	FT_Int32 m_LoadFlags = 0;
	FT_Size m_Size = {};

	std::unordered_map<uint32_t, GlyphBitmap> m_GlyphCache;

	static void BlitGlyph(const FT_Bitmap *ft_bitmap, std::vector<uint8_t> dstBuffer);

	void ReadFontFile();
	GlyphBitmap RasterizeGlyphInternal(uint32_t codepoint);
};
