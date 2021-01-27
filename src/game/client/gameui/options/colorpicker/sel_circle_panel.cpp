#include <vgui/IImage.h>
#include <vgui/ISurface.h>
#include "sel_circle_panel.h"
#include "../color_picker.h"
#include "hud.h"
#include "client_vgui.h"

namespace colorpicker
{

//----------------------------------------------------------
// colorpicker::CSelCircleImage
//----------------------------------------------------------
class CSelCircleImage : public vgui2::IImage
{
public:
	CSelCircleImage()
	{
		m_wide = CIRCLE_SIZE;
		m_tall = CIRCLE_SIZE;
		m_Color = Color(255, 0, 0, 255);

		if (!m_sIsTextureReady)
		{
			m_sBgTexture = vgui2::surface()->CreateNewTextureID();
			vgui2::surface()->DrawSetTextureFile(m_sBgTexture, VGUI2_ROOT_DIR "gfx/circle_bg", true, false);

			m_sFgTexture = vgui2::surface()->CreateNewTextureID();
			vgui2::surface()->DrawSetTextureFile(m_sFgTexture, VGUI2_ROOT_DIR "gfx/circle_fg", true, false);

			m_sIsTextureReady = true;
		}
	}

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint()
	{
		int posX = m_nX + m_offX;
		int posY = m_nY + m_offY;

		if (m_sIsTextureReady)
		{
			vgui2::surface()->DrawSetTexture(m_sBgTexture);
			vgui2::surface()->DrawSetColor(Color(255, 255, 255, static_cast<int>(0.5 * 255)));
			vgui2::surface()->DrawTexturedRect(posX, posY, posX + m_wide, posY + m_tall);

			vgui2::surface()->DrawSetTexture(m_sFgTexture);
			vgui2::surface()->DrawSetColor(m_Color);
			vgui2::surface()->DrawTexturedRect(posX, posY, posX + m_wide, posY + m_tall);
		}
	}

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

	virtual int GetWide()
	{
		return m_wide;
	}

	virtual int GetTall()
	{
		return m_tall;
	}

protected:
	Color m_Color;
	int m_nX = 0, m_nY = 0;
	int m_wide = 0, m_tall = 0;
	int m_offX = 0, m_offY = 0;

	static bool m_sIsTextureReady;
	static int m_sBgTexture;
	static int m_sFgTexture;
};

}

bool colorpicker::CSelCircleImage::m_sIsTextureReady = false;
int colorpicker::CSelCircleImage::m_sBgTexture = -1;
int colorpicker::CSelCircleImage::m_sFgTexture = -1;

//----------------------------------------------------------
// colorpicker::CSelCirclePanel
//----------------------------------------------------------
colorpicker::CSelCirclePanel::CSelCirclePanel(vgui2::Panel *pParent, const char *panelName)
    : vgui2::ImagePanel(pParent, panelName)
{
	m_pImage = new CSelCircleImage();
	SetImage(m_pImage);
}

colorpicker::CSelCirclePanel::~CSelCirclePanel()
{
	SetImage(static_cast<vgui2::IImage *>(nullptr));
	delete m_pImage;
	m_pImage = nullptr;
}

void colorpicker::CSelCirclePanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetSize(CIRCLE_SIZE, CIRCLE_SIZE);
}
