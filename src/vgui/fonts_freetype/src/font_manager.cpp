#include <tier1/KeyValues.h>
#include "font_manager.h"
#include "font.h"

CFontManager::CFontManager(ILogger *pLogger, vgui2::ISurface* pVGuiSurface, vgui2::ISchemeManager* pVGuiSchemeManager)
{
	m_pLogger = pLogger;
	m_pVGuiSurface = pVGuiSurface;
	m_pVGuiSchemeManager = pVGuiSchemeManager;
}

FontSettings CFontManager::ParseFontSettings(KeyValues *kv, bool isProportional)
{
	const char *rootName = kv->GetName();

	int screenWide, screenTall;
	m_pVGuiSurface->GetScreenSize(screenWide, screenTall);
	
	for (KeyValues *font = kv->GetFirstSubKey(); font; font = font->GetNextKey())
	{
		const char *name = font->GetString("name");
		bool isCustom = font->GetBool("custom", false);
		bool isThisProportional = isProportional;
		std::string fontPath;

        // Find the font
		if (isCustom)
		{
			// TODO
			fontPath = name;
		}
		else
		{
			fontPath = FindSystemFontPath(name);

			if (fontPath.empty())
			{
				m_pLogger->LogDebug("Font {}: font {} not found", rootName, name);
				continue;
			}

			m_pLogger->LogDebug("Font {}: {} -> {}", rootName, name, fontPath);
		}

		// Check YRes
		const char *yres = font->GetString("yres");

		if (yres[0] != '\0')
		{
            int yresMin = 0, yresMax = 0;

            if (sscanf(yres, "%d %d", &yresMin, &yresMax) != 2)
            {
                m_pLogger->LogDebug("Font {}: invalid yres = '{}'", rootName, yres);
                continue;
			}

			if (!(screenTall >= yresMin && screenTall <= yresMax))
			{
				m_pLogger->LogDebug("Font {}: yres = '{}', tall = {} - not in range", rootName, yres, screenTall);
                continue;
            }

			isThisProportional = false;
		}

		// Proportinal scale
		int tall = font->GetInt("tall");

		if (tall <= 0)
		{
			m_pLogger->LogDebug("Font {}: tall = {} - too small", rootName, tall);
            continue;
		}

		if (isThisProportional)
		{
			tall = m_pVGuiSchemeManager->GetProportionalScaledValue(tall);
        }

		// Fill settings
		FontSettings settings;
		settings.fontPath = fontPath;
		settings.tall = tall;
		settings.weight = font->GetInt("weight");
		settings.italic = font->GetBool("italic");
		settings.antialias = font->GetBool("antialias");
		settings.outline = font->GetBool("outline");
		settings.underline = font->GetBool("underline");
		settings.dropshadow = font->GetBool("dropshadow");
		settings.symbol = font->GetBool("symbol");
		settings.custom = isCustom;

		return settings;
	}

	return FontSettings();
}

CFont *CFontManager::FindOrCreateFont(const FontSettings &settings)
{
	if (!settings.IsValid())
		return nullptr;

	// Try to find the font
	for (auto &pFont : m_Fonts)
	{
		if (pFont->GetSettings() == settings)
			return pFont.get();
	}

	// Create a new font
	try
	{
		std::unique_ptr<CFont> pFont = std::make_unique<CFont>(this, settings);
		pFont->LoadFont();
		return m_Fonts.emplace_back(std::move(pFont)).get();
	}
    catch (const std::exception& e)
	{
		m_pLogger->LogError("Failed to load font {}: {}", settings.fontPath, e.what());
		return nullptr;
    }
}

std::string CFontManager::FindSystemFontPath(const char *pszFontName)
{
    std::string fontFile;

	// https://stackoverflow.com/a/14634033
	if (!m_hFontConfig)
	{
		m_hFontConfig = FcInitLoadConfigAndFonts();
    }

    // configure the search pattern, 
    // assume "name" is a std::string with the desired font name in it
    FcPattern* pat = FcNameParse((const FcChar8*)pszFontName);
    FcConfigSubstitute(m_hFontConfig, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    // Find the font
    FcResult res;
    FcPattern* font = FcFontMatch(m_hFontConfig, pat, &res);
    if (font)
    {
		FcChar8 *file = NULL;
		
        if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch)
        {
            // save the file to another std::string
            fontFile = (char*)file;
		}
		
        FcPatternDestroy(font);
    }

	FcPatternDestroy(pat);
	return fontFile;
}
