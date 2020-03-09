//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef CPNGIMAGE_H
#define CPNGIMAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Image.h>

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CPngImage : public vgui2::IImage
{
public:
	CPngImage(const char *filename);

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint();

	// Set the position of the image
	virtual void SetPos(int x, int y)
	{
		m_nX = x;
		m_nY = y;
	}

	virtual void SetOffset(int x, int y)
	{
		m_offX = x;
		m_offY = y;
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall)
	{
		wide = m_wide;
		tall = m_tall;
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall)
	{
		GetContentSize(wide, tall);
	}

	// Sets the size of the image
	virtual void SetSize(int wide, int tall)
	{
		m_wide = wide;
		m_tall = tall;
	}

	// Set the draw color
	virtual void SetColor(Color col)
	{
		m_Color = col;
	}

	int GetWide() { return m_wide; }
	int GetTall() { return m_tall; }

private:
	Color m_Color;
	int m_iTextureID = -1;
	int m_nX = 0, m_nY = 0;
	int m_wide = 0, m_tall = 0;
	bool m_bValid;
	int m_offX = 0, m_offY = 0;
};

#endif // VGUI_AVATARIMAGE_H
