#pragma once
#include <stdexcept>
#include <fmt/format.h>
#include <ft2build.h>
#include FT_FREETYPE_H          // <freetype/freetype.h>
#include FT_MODULE_H            // <freetype/ftmodapi.h>
#include FT_GLYPH_H             // <freetype/ftglyph.h>
#include FT_SIZES_H             // <freetype/ftsizes.h>
#include FT_SYNTHESIS_H         // <freetype/ftsynth.h>

inline void CheckFreeTypeError(FT_Error error)
{
	if (error != 0)
		throw std::runtime_error(fmt::format("FT error: {}", FT_Error_String(error)));
}
