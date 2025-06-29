#pragma once
#include <string>

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

class CFont
{
public:
	CFont(CFontManager* pFontManager, const FontSettings& settings);
	
	//! @returns The font settings that were used to create the font.
	const FontSettings &GetSettings() const { return m_Settings; }

	//! Loads the font.
	void LoadFont();

private:
	CFontManager *m_pFontManager = nullptr;
	FontSettings m_Settings;
};
