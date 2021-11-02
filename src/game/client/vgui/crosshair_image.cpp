#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include "crosshair_image.h"

CCrosshairImage::CCrosshairImage()
{
}

void CCrosshairImage::SetSettings(const CrosshairSettings &settings)
{
	m_Settings = settings;
}

void CCrosshairImage::Paint()
{
	int cx = m_wide / 2;
	int cy = m_tall / 2;
	int gap = m_Settings.gap / 2;
	int thick = m_Settings.thickness;
	int outline = m_Settings.outline;
	int size = m_Settings.size;

	// Draw outline
	if (outline > 0)
	{
		// Don't read that if you don't want eye cancer
		vgui2::surface()->DrawSetColor(0, 0, 0, 255);
		vgui2::surface()->DrawFilledRect(cx + gap - outline, cy - thick / 2 - outline,
		    cx + gap - outline + size + outline * 2, cy - thick / 2 - outline + thick + outline * 2);
		vgui2::surface()->DrawFilledRect(cx - gap - size - outline, cy - thick / 2 - outline,
		    cx - gap - size - outline + size + outline * 2, cy - thick / 2 - outline + thick + outline * 2);
		vgui2::surface()->DrawFilledRect(cx - thick / 2 - outline, cy + gap - outline,
		    cx - thick / 2 - outline + thick + outline * 2, cy + gap - outline + size + outline * 2);

		if (!m_Settings.t)
			vgui2::surface()->DrawFilledRect(cx - thick / 2 - outline, cy - gap - size - outline,
			    cx - thick / 2 - outline + thick + outline * 2, cy - gap - size - outline + size + outline * 2);
	}

	vgui2::surface()->DrawSetColor(m_Settings.color);

	// Draw dot
	if (m_Settings.dot)
	{
		vgui2::surface()->DrawFilledRect(cx - thick / 2, cy - thick / 2, thick + cx - thick / 2, thick + cy - thick / 2);
	}

	// Draw crosshair
	vgui2::surface()->DrawFilledRect(cx + gap, cy - thick / 2, cx + gap + size, cy - thick / 2 + thick);
	vgui2::surface()->DrawFilledRect(cx - gap - size, cy - thick / 2, cx - gap, cy - thick / 2 + thick);
	vgui2::surface()->DrawFilledRect(cx - thick / 2, cy + gap, cx - thick / 2 + thick, cy + gap + size);
	if (!m_Settings.t)
		vgui2::surface()->DrawFilledRect(cx - thick / 2, cy - gap - size, cx - thick / 2 + thick, cy - gap);
}

void CCrosshairImage::SetPos(int x, int y)
{
	m_nX = x;
	m_nY = y;
}

void CCrosshairImage::GetContentSize(int &wide, int &tall)
{
	wide = m_wide;
	tall = m_tall;
}

void CCrosshairImage::GetSize(int &wide, int &tall)
{
	GetContentSize(wide, tall);
}

void CCrosshairImage::SetSize(int wide, int tall)
{
	m_wide = wide;
	m_tall = tall;
}

void CCrosshairImage::SetColor(Color)
{
}
