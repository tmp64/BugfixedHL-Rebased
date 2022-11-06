#include <cmath>
#include <mathlib/mathlib.h>
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
	int cx = m_nX + m_wide / 2;
	int cy = m_nY + m_tall / 2;
	int gap = m_Settings.gap;
	int thick = m_Settings.thickness;
	int outline = m_Settings.outline;
	int size = m_Settings.size;

	// Gap must be as odd/even as thickness. Otherwise lines will be misaligned.
	if (thick % 2 == 0)
		gap = gap * 2;
	else
		gap = gap * 2 + 1;

	int halfGapPos = (int)std::floor(gap / 2.0f);
	int halfGapNeg = (int)std::floor(-gap / 2.0f);

	// Draw outline
	if (outline > 0)
	{
		int olSize = size + 2 * outline;
		int olThick = thick + 2 * outline;
		int olHalfGapPos = halfGapPos - outline;
		int olHalfGapNeg = halfGapNeg + outline;
		vgui2::surface()->DrawSetColor(0, 0, 0, 255);
		if (!m_Settings.t)
			DrawVLine(cx, cy + olHalfGapNeg, -olSize, olThick); // N
		DrawVLine(cx, cy + olHalfGapPos, +olSize, olThick); // S
		DrawHLine(cx + olHalfGapNeg, cy, -olSize, olThick); // W
		DrawHLine(cx + olHalfGapPos, cy, +olSize, olThick); // E

		if (m_Settings.dot)
			DrawDot(cx, cy, olThick);
	}

	vgui2::surface()->DrawSetColor(m_Settings.color);

	// Draw dot
	if (m_Settings.dot)
		DrawDot(cx, cy, thick);

	// Draw crosshair
	if (!m_Settings.t)
		DrawVLine(cx, cy + halfGapNeg, -size, thick); // N
	DrawVLine(cx, cy + halfGapPos, +size, thick); // S
	DrawHLine(cx + halfGapNeg, cy, -size, thick); // W
	DrawHLine(cx + halfGapPos, cy, +size, thick); // E
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

void CCrosshairImage::DrawVLine(int x0, int y0, int len, int thick)
{
	// Line end pos
	int x1 = x0;
	int y1 = y0 + len;

	// Half-thickness, positive and negative
	int htp = (int)std::floor(thick / 2.0f);
	int htn = (int)std::floor(-thick / 2.0f);

	// Rect coords
	int rx0 = x0 + htn;
	int rx1 = x1 + htp;
	int ry0 = y0;
	int ry1 = y1;

	// Restore CW vertex order
	if (len < 0)
		V_swap(ry0, ry1);

	vgui2::surface()->DrawFilledRect(rx0, ry0, rx1, ry1);
}

void CCrosshairImage::DrawHLine(int x0, int y0, int len, int thick)
{
	// Line end pos
	int x1 = x0 + len;
	int y1 = y0;

	// Half-thickness, positive and negative
	int htp = (int)std::floor(thick / 2.0f);
	int htn = (int)std::floor(-thick / 2.0f);

	// Rect coords
	int rx0 = x0;
	int rx1 = x1;
	int ry0 = y0 + htn;
	int ry1 = y1 + htp;

	// Restore CW vertex order
	if (len < 0)
		V_swap(rx0, rx1);

	vgui2::surface()->DrawFilledRect(rx0, ry0, rx1, ry1);
}

void CCrosshairImage::DrawDot(int x, int y, int size)
{
	int halfSizePos = (int)std::floor(+size / 2.0f);
	int halfSizeNeg = (int)std::floor(-size / 2.0f);
	int x0 = x + halfSizeNeg;
	int y0 = y + halfSizeNeg;
	int x1 = x + halfSizePos;
	int y1 = y + halfSizePos;
	vgui2::surface()->DrawFilledRect(x0, y0, x1, y1);
}
