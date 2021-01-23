#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include "tga_image.h"

CTGAImage::CTGAImage()
{
	m_iTextureID = vgui2::surface()->CreateNewTextureID();
}

CTGAImage::CTGAImage(const char *pFilePath)
    : CTGAImage()
{
	LoadImage(pFilePath);
}

void CTGAImage::LoadImage(const char *pFilePath)
{
	vgui2::surface()->DrawSetTextureFile(m_iTextureID, pFilePath, true, false);
}

CTGAImage::~CTGAImage()
{
	if (m_iTextureID != -1)
		vgui2::surface()->DeleteTextureByID(m_iTextureID);
}

void CTGAImage::Paint()
{
	if (m_iTextureID != -1)
	{
		vgui2::surface()->DrawSetTexture(m_iTextureID);
		vgui2::surface()->DrawSetColor(m_Color);
		vgui2::surface()->DrawTexturedRect(m_nX, m_nY, m_nX + m_wide, m_nY + m_tall);
	}
}

void CTGAImage::SetPos(int x, int y)
{
	m_nX = x;
	m_nY = y;
}

void CTGAImage::GetContentSize(int &wide, int &tall)
{
	wide = m_wide;
	tall = m_tall;
}

void CTGAImage::GetSize(int &wide, int &tall)
{
	GetContentSize(wide, tall);
}

void CTGAImage::SetSize(int wide, int tall)
{
	m_wide = wide;
	m_tall = tall;
}

void CTGAImage::SetColor(Color col)
{
	m_Color = col;
}
