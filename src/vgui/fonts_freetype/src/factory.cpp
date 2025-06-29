#include <memory>
#include <tier1/KeyValues.h>
#include <tier2/tier2.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <bhl/logging/ILogger.h>
#include <bhl/logging/prefix_logger.h>
#include <vgui/fonts_freetype/factory.h>
#include <FileSystem.h>
#include "vgui_scheme_wrap.h"
#include "font_manager.h"
#include "font.h"

template <typename T>
T *FindInterfaceInFactory(CreateInterfaceFn *pFactories, int iNumFactories, const char *pszName)
{
	for (int i = 0; i < iNumFactories; i++)
	{
		void *ptr = pFactories[i](pszName, nullptr);

		if (ptr)
			return static_cast<T *>(ptr);
	}

	throw std::runtime_error(fmt::format("Interface {} not found", pszName));
}

class CFreeTypeSchemeManager : public CVGuiSchemeManagerWrap
{
public:
	CFreeTypeSchemeManager(
	    ILogger *pLogger,
	    IFileSystem *pFileSystem,
	    vgui2::ISchemeManager *pBaseSchemeManager,
	    CFontManager *pFontManager)
	{
		m_pBase = pBaseSchemeManager;
		m_pLogger = pLogger;
		m_pFileSystem = pFileSystem;
		m_pFontManager = pFontManager;
    }
	
	virtual vgui2::HScheme LoadSchemeFromFile(const char *fileName, const char *tag) override
	{
		// Load the original scheme
		vgui2::HScheme hScheme = m_pBase->LoadSchemeFromFile(fileName, tag);

		if (!hScheme)
		{
			m_pLogger->LogError("Failed to load the scheme {} in the engine", tag);
			return hScheme;
		}

		// Read the file
		KeyValuesAD scheme("Scheme");
		if (!scheme->LoadFromFile(m_pFileSystem, fileName))
		{
			m_pLogger->LogError("Failed to load the scheme {} KV file", tag);
			return hScheme;
		}

		// Iterate over all fonts
		KeyValues *fonts = scheme->FindKey("Fonts", false);

		if (!fonts)
		{
			m_pLogger->LogWarn("No fonts in scheme {}", tag);
			return hScheme;
		}

		vgui2::IScheme* pScheme = m_pBase->GetIScheme(hScheme);

		for (KeyValues *font = fonts->GetFirstSubKey(); font; font = font->GetNextKey())
		{
			// Find the font in the scheme
			const char* fontName = font->GetName();

			for (int i = 0; i <= 1; i++)
			{
				bool isProportional = (bool)i;
				vgui2::HFont hFont = pScheme->GetFont(fontName, isProportional);

				if (!hFont)
				{
					m_pLogger->LogError("Font {} (prop={}) not found in scheme {}", fontName, isProportional, tag);
					continue;
				}

				m_pLogger->LogDebug("Font: {}::{} (prop={}) = 0x{:X}", tag, fontName, isProportional, hFont);

				// Parse settings
				FontSettings settings = m_pFontManager->ParseFontSettings(font, isProportional);

				if (!settings.IsValid())
				{
					m_pLogger->LogDebug("Font: {}::{} (prop={}) failed to load settings", tag, fontName, isProportional);
					continue;
				}

				// Load the font
				CFont *pMyFont = m_pFontManager->FindOrCreateFont(settings);

				if (!pMyFont)
				{
					m_pLogger->LogDebug("Font: {}::{} (prop={}) failed to load font", tag, fontName, isProportional);
					continue;
				}
            }
        }

		return hScheme;
	}

private:
	ILogger *m_pLogger = nullptr;
	IFileSystem *m_pFileSystem = nullptr;
	CFontManager *m_pFontManager = nullptr;
};

class CFreeTypeFactory
{
public:
	CFreeTypeFactory(
	    CreateInterfaceFn *pFactories,
	    int iNumFactories,
	    ILogger *pLogger)
	    : m_Logger(pLogger, "VGuiFreeType")
	{
		auto pRealFileSystem = FindInterfaceInFactory<IFileSystem>(pFactories, iNumFactories, FILESYSTEM_INTERFACE_VERSION);
		auto pRealSurface = FindInterfaceInFactory<vgui2::ISurface>(pFactories, iNumFactories, VGUI_SURFACE_INTERFACE_VERSION_GS);
		auto pRealSchemeManager = FindInterfaceInFactory<vgui2::ISchemeManager>(pFactories, iNumFactories, VGUI_SCHEME_INTERFACE_VERSION_GS);

		m_pFontManager = std::make_unique<CFontManager>(
		    &m_Logger,
		    pRealSurface,
		    pRealSchemeManager
		);

		m_pSchemeManager = std::make_unique<CFreeTypeSchemeManager>(
		    &m_Logger,
		    pRealFileSystem,
		    pRealSchemeManager,
		    m_pFontManager.get());
	}

	void *CreateInterface(const char *pName, int *pReturnCode)
	{
		if (pReturnCode)
		    *pReturnCode = IFACE_OK;
		
		if (!strcmp(pName, VGUI_SCHEME_INTERFACE_VERSION_GS))
			return static_cast<vgui2::ISchemeManager *>(m_pSchemeManager.get());

		if (pReturnCode)
			*pReturnCode = IFACE_FAILED;

		return nullptr;
    }

private:
	CPrefixLogger m_Logger;
	std::unique_ptr<CFontManager> m_pFontManager;
	std::unique_ptr<CFreeTypeSchemeManager> m_pSchemeManager;
};

static std::unique_ptr<CFreeTypeFactory> g_pFactory;

void ConnectVGuiFreeTypeLibraries(CreateInterfaceFn *pFactories, int iNumFactories, ILogger* pLogger)
{
	g_pFactory = std::make_unique<CFreeTypeFactory>(pFactories, iNumFactories, pLogger);
}

CreateInterfaceFn GetFreeTypeFactory()
{
	return [](const char *pName, int *pReturnCode)
	{
		return g_pFactory->CreateInterface(pName, pReturnCode);
    };
}
