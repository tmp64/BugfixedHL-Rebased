#include "font.h"

CFont::CFont(CFontManager *pFontManager, const FontSettings &settings)
{
	m_pFontManager = pFontManager;
	m_Settings = settings;
}

void CFont::LoadFont()
{
}
