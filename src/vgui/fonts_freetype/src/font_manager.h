#pragma once
#include <memory>
#include <vector>
#include <fontconfig/fontconfig.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <bhl/logging/ILogger.h>

struct FontSettings;
class KeyValues;
class CFont;

class CFontManager
{
public:
	CFontManager(ILogger *pLogger, vgui2::ISurface *pVGuiSurface, vgui2::ISchemeManager *pVGuiSchemeManager);

	//! @returns The FreeType library instance.
	FT_Library GetFreeType() const { return m_hFTLib; }

	//! Parses font settings from the KeyValues. Automcatically selects first compatible font.
	FontSettings ParseFontSettings(KeyValues *kv, bool isProportional);

	//! Finds already loaded font or creates a new one.
	CFont *FindOrCreateFont(const FontSettings &settings);

private:
	ILogger *m_pLogger = nullptr;
	vgui2::ISurface* m_pVGuiSurface = nullptr;
	vgui2::ISchemeManager *m_pVGuiSchemeManager = nullptr;

	FcConfig *m_hFontConfig = nullptr;
	FT_Library m_hFTLib = nullptr;
	
	std::vector<std::unique_ptr<CFont>> m_Fonts;

	void InitFreeType();
	std::string FindSystemFontPath(const char *pszFontName);
};
