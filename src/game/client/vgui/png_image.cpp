#include <vgui/ISurface.h>
#include "avatar_image.h"
#include "png_image.h"
#include "lodepng.h"
#include "hud.h"
#include "cl_dll.h"

CPngImage::CPngImage(const char *filename)
{
	m_Color = Color(255, 255, 255, 255);
	char fullPath[256];
	gEngfuncs.COM_ExpandFilename(filename, fullPath, sizeof(fullPath));

	byte *out = nullptr;
	unsigned width, height;
	unsigned error = lodepng_decode32_file(&out, &width, &height, fullPath);
	if (error)
		Error("CPngImage: Failed to load '%s': %s\n", fullPath, lodepng_error_text(error));
	else
	{
		m_iTextureID = vgui2::surface()->CreateNewTextureID(true);
		vgui2::surface()->DrawSetTextureRGBA(m_iTextureID, out, width, height, false, false);
		m_wide = width;
		m_tall = height;
	}
	free(out);
}

void CPngImage::Paint()
{
	int posX = m_nX + m_offX;
	int posY = m_nY + m_offY;

	if (m_iTextureID != -1)
	{
		vgui2::surface()->DrawSetTexture(m_iTextureID);
		vgui2::surface()->DrawSetColor(m_Color);
		vgui2::surface()->DrawTexturedRect(posX, posY, posX + m_wide, posY + m_tall);
	}
}
