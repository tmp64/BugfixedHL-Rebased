//
// This code is based on Dear ImGui FreeType intergration
// https://github.com/ocornut/imgui/blob/master/misc/freetype/imgui_freetype.cpp
// Copyright (c) 2014-2025 Omar Cornut
//

#include <algorithm>
#include <fstream>
#include <tier0/dbg.h>
#include "font_manager.h"
#include "font.h"

// Glyph metrics:
// --------------
//
//                       xmin                     xmax
//                        |                         |
//                        |<-------- width -------->|
//                        |                         |
//              |         +-------------------------+----------------- ymax
//              |         |    ggggggggg   ggggg    |     ^        ^
//              |         |   g:::::::::ggg::::g    |     |        |
//              |         |  g:::::::::::::::::g    |     |        |
//              |         | g::::::ggggg::::::gg    |     |        |
//              |         | g:::::g     g:::::g     |     |        |
//    offsetX  -|-------->| g:::::g     g:::::g     |  offsetY     |
//              |         | g:::::g     g:::::g     |     |        |
//              |         | g::::::g    g:::::g     |     |        |
//              |         | g:::::::ggggg:::::g     |     |        |
//              |         |  g::::::::::::::::g     |     |      height
//              |         |   gg::::::::::::::g     |     |        |
//  baseline ---*---------|---- gggggggg::::::g-----*--------      |
//            / |         |             g:::::g     |              |
//     origin   |         | gggggg      g:::::g     |              |
//              |         | g:::::gg   gg:::::g     |              |
//              |         |  g::::::ggg:::::::g     |              |
//              |         |   gg:::::::::::::g      |              |
//              |         |     ggg::::::ggg        |              |
//              |         |         gggggg          |              v
//              |         +-------------------------+----------------- ymin
//              |                                   |
//              |------------- advanceX ----------->|

constexpr int FT_SCALEFACTOR = 64;

CFont::CFont(CFontManager *pFontManager, ILogger *pLogger, const FontSettings &settings)
	: m_Logger(pLogger, settings.fontPath)
{
	m_pFontManager = pFontManager;
	m_Settings = settings;
}

void CFont::LoadFont()
{
	ReadFontFile();
	
	FT_Error error = FT_New_Memory_Face(
		m_pFontManager->GetFreeType(),
		(uint8_t *)m_FontFileData.data(),
		(uint32_t)m_FontFileData.size(),
		(uint32_t)0,
		&m_Face);

	CheckFreeTypeError(error);
	
	error = FT_Select_Charmap(m_Face, FT_ENCODING_UNICODE);
	CheckFreeTypeError(error);

	m_LoadFlags = 0;

	if (m_Settings.antialias)
		m_LoadFlags |= FT_LOAD_TARGET_NORMAL;
	else
		m_LoadFlags |= FT_LOAD_TARGET_MONO;

	FT_New_Size(m_Face, &m_Size);
	FT_Activate_Size(m_Size);

	FT_Size_RequestRec req;
	req.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
	req.width = 0;
	req.height = (uint32_t)(m_Settings.tall * FT_SCALEFACTOR);
	req.horiResolution = 0;
	req.vertResolution = 0;
	FT_Request_Size(m_Face, &req);
}

void CFont::BlitGlyph(const FT_Bitmap *ft_bitmap, std::vector<uint8_t> dstBuffer)
{
	const uint32_t w = ft_bitmap->width;
	const uint32_t h = ft_bitmap->rows;
	const uint8_t *src = ft_bitmap->buffer;
	const uint32_t src_pitch = ft_bitmap->pitch;

	dstBuffer.resize(4 * w * h);

	const uint32_t dst_pitch = 4 * w;
	uint8_t* dst = dstBuffer.data();

	switch (ft_bitmap->pixel_mode)
	{
	case FT_PIXEL_MODE_GRAY:
	{
		// Grayscale image, 1 byte per pixel.
		for (uint32_t y = 0; y < h; y++, src += src_pitch, dst += dst_pitch)
		{
			for (uint32_t x = 0; x < w; x++)
			{
				dst[4 * x + 0] = 255;
				dst[4 * x + 1] = 255;
				dst[4 * x + 2] = 255;
				dst[4 * x + 3] = src[x];
			}
		}

		break;
	}
	case FT_PIXEL_MODE_MONO:
	{
		// Monochrome image, 1 bit per pixel. The bits in each byte are ordered from MSB to LSB.
		for (uint32_t y = 0; y < h; y++, src += src_pitch, dst += dst_pitch)
		{
			uint8_t bits = 0;
			const uint8_t *bits_ptr = src;
			for (uint32_t x = 0; x < w; x++, bits <<= 1)
			{
				if ((x & 7) == 0)
					bits = *bits_ptr++;

				dst[4 * x + 0] = 255;
				dst[4 * x + 1] = 255;
				dst[4 * x + 2] = 255;
				dst[4 * x + 3] = (bits & 0x80) ? 255 : 0;
			}
		}

		break;
	}
	case FT_PIXEL_MODE_BGRA:
	{
		// FIXME: Converting pre-multiplied alpha to straight. Doesn't smell good.
#define DE_MULTIPLY(color, alpha) std::min((uint32_t)(255.0f * (float)color / (float)(alpha + FLT_MIN) + 0.5f), 255u)
		
		for (uint32_t y = 0; y < h; y++, src += src_pitch, dst += dst_pitch)
		{
			for (uint32_t x = 0; x < w; x++)
			{
				uint8_t r = src[x * 4 + 2], g = src[x * 4 + 1], b = src[x * 4], a = src[x * 4 + 3];
				dst[4 * x + 0] = DE_MULTIPLY(r, a);
				dst[4 * x + 1] = DE_MULTIPLY(g, a);
				dst[4 * x + 2] = DE_MULTIPLY(b, a);
				dst[4 * x + 3] = a;
			}
		}

#undef DE_MULTIPLY
		break;
	}
	default:
		Assert(!("CFont::BlitGlyph(): Unknown bitmap pixel mode!"));
	}
}

void CFont::ReadFontFile()
{
	if (m_Settings.custom)
	{
		// TODO: Read using IFileSystem
	}
	else
	{
		// Read from the FS
		std::ifstream fs;
		fs.exceptions(std::ios::failbit | std::ios::badbit);
		fs.open(m_Settings.fontPath, std::ios::binary);
		
		fs.seekg(0, std::ios::end);
		uint64_t fileSize = fs.tellg();

		if (fileSize > std::numeric_limits<ssize_t>::max())
			throw std::runtime_error("Font file too large");

		fs.seekg(0, std::ios::beg);
		m_FontFileData.resize((size_t)fileSize);
		fs.read((char*)m_FontFileData.data(), fileSize);
	}
}

const GlyphBitmap &CFont::RasterizeGlyph(uint32_t codepoint)
{
	auto it = m_GlyphCache.find(codepoint);

	if (it != m_GlyphCache.end())
		return it->second;

	try
	{
		GlyphBitmap glyph = RasterizeGlyphInternal(codepoint);
		return m_GlyphCache.emplace(codepoint, std::move(glyph)).first->second;
	}
	catch (const std::exception &e)
	{
		m_Logger.LogError("Failed to rasterize codepoint {}: {}", codepoint, e.what());

		// Cache empty glyph
		return m_GlyphCache.emplace(codepoint, GlyphBitmap()).first->second;
	}
}

GlyphBitmap CFont::RasterizeGlyphInternal(uint32_t codepoint)
{
	uint32_t glyphIndex = FT_Get_Char_Index(m_Face, codepoint);

	if (glyphIndex == 0)
		throw std::runtime_error("Codepoint not found in font");

	
	FT_Error error = FT_Load_Glyph(m_Face, glyphIndex, m_LoadFlags);
	CheckFreeTypeError(error);

	// Need an outline for this to work
	FT_GlyphSlot slot = m_Face->glyph;

	if (slot->format != FT_GLYPH_FORMAT_OUTLINE)
		throw std::runtime_error(fmt::format("Unknown glyph format {}", (int)slot->format));

	// Activate current size
	FT_Activate_Size(m_Size);

	// Render glyph into a bitmap (currently held by FreeType)
	FT_Render_Mode renderMode = m_Settings.antialias ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO;
	error = FT_Render_Glyph(slot, renderMode);
	CheckFreeTypeError(error);

	const FT_Bitmap* ftBitmap = &slot->bitmap;

	GlyphBitmap bitmap;
	bitmap.wide = ftBitmap->width;
	bitmap.tall = ftBitmap->rows;
	bitmap.advance = std::ceil((float)slot->advance.x / FT_SCALEFACTOR);
	bitmap.metrics = slot->metrics;

	BlitGlyph(ftBitmap, bitmap.data);
	return bitmap;
}
