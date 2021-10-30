#include "picker_panel.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui/IImage.h>
#include <vgui/ISurface.h>
#include "../color_picker.h"
#include "texture_manager.h"
#include "sel_circle_panel.h"
#include "opengl.h"

namespace colorpicker
{

class CPickerImage : public vgui2::IImage
{
public:
	CPickerImage()
	{
		m_wide = PICKER_WIDE;
		m_tall = PICKER_TALL;
	}

	void SetHue(float hue)
	{
		m_iTextureIdx = gTexMgr.GetPickerTextureIndex(hue);
		HSVtoRGB(hue, 100, 100, m_HueColor);
	}

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint()
	{
		int posX = m_nX + m_offX;
		int posY = m_nY + m_offY;

		if (CClientOpenGL::Get().IsAvailable())
		{
			// The code uses hardware linear interpolation instead of textures.
			// The color is a bit off but it's good enough and doesn't waste VRAM.
			// It is done by firstly drawing white-saturatedColor horizontal gradient
			// and then drawing transparent-black vertical gradient to darken the image.
			// Credits to Dear ImGui (ImGui::ColorPicker4 function).
			uint8_t hueColor[4] = { (uint8_t)m_HueColor.r(), (uint8_t)m_HueColor.g(), (uint8_t)m_HueColor.b(), 255 };

			glDisable(GL_TEXTURE_2D);
			glShadeModel(GL_SMOOTH);

			glBegin(GL_QUADS);

			// Colored rect
			glColor4ub(255, 255, 255, 255);
			glVertex2f(posX, posY);

			glColor4ubv(hueColor);
			glVertex2f(posX + m_wide, posY);

			glColor4ubv(hueColor);
			glVertex2f(posX + m_wide, posY + m_tall);

			glColor4ub(255, 255, 255, 255);
			glVertex2f(posX, posY + m_tall);

			// Black semi-transparent rect
			glColor4ub(0, 0, 0, 0);
			glVertex2f(posX, posY);

			glColor4ub(0, 0, 0, 0);
			glVertex2f(posX + m_wide, posY);

			glColor4ub(0, 0, 0, 255);
			glVertex2f(posX + m_wide, posY + m_tall);

			glColor4ub(0, 0, 0, 255);
			glVertex2f(posX, posY + m_tall);

			glEnd();

			glEnable(GL_TEXTURE_2D);
		}
		else if (gTexMgr.IsReady())
		{
			vgui2::surface()->DrawSetTexture(gTexMgr.GetPickerTexture(m_iTextureIdx));
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

	int GetTextureIndex()
	{
		return m_iTextureIdx;
	}

protected:
	Color m_Color;
	Color m_HueColor;
	int m_iTextureIdx = 0;
	int m_nX = 0, m_nY = 0;
	int m_wide = 0, m_tall = 0;
	int m_offX = 0, m_offY = 0;
};

class CPickerImagePanel : public vgui2::ImagePanel
{
	DECLARE_CLASS_SIMPLE(CPickerImagePanel, vgui2::ImagePanel);

public:
	CPickerImage *m_pPickerImg = nullptr;

	CPickerImagePanel(colorpicker::CPickerPanel *parent, const char *panelname)
	    : vgui2::ImagePanel(parent, panelname)
	{
		m_pParent = parent;
		m_pPickerImg = new CPickerImage();
		SetImage(m_pPickerImg);
	}

	void SetHue(float h)
	{
		m_pPickerImg->SetHue(h);
	}

	virtual ~CPickerImagePanel()
	{
		SetImage(static_cast<vgui2::IImage *>(nullptr));
		delete m_pPickerImg;
		m_pPickerImg = nullptr;
	}

private:
	CPickerPanel *m_pParent = nullptr;
};

class CPickerMousePanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CPickerImagePanel, vgui2::Panel);

public:
	CPickerMousePanel(colorpicker::CPickerPanel *parent, const char *panelname)
	    : vgui2::Panel(parent, panelname)
	{
		m_pParent = parent;
	}

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetBgColor(Color(0, 0, 0, 0));
	}

	virtual void OnMousePressed(vgui2::MouseCode code)
	{
		BaseClass::OnMousePressed(code);
		if (code == vgui2::MOUSE_LEFT)
			m_bIsMousePressed = true;
	}

	virtual void OnMouseReleased(vgui2::MouseCode code)
	{
		BaseClass::OnMouseReleased(code);
		if (code == vgui2::MOUSE_LEFT)
			m_bIsMousePressed = false;
	}

	virtual void OnCursorExited()
	{
		BaseClass::OnCursorExited();
		m_bIsMousePressed = false;
	}

	virtual void OnCursorMoved(int x, int y)
	{
		BaseClass::OnCursorMoved(x, y);
		if (!m_bIsMousePressed)
			return;

		int px, py, pwide, ptall;
		m_pParent->m_pImagePanel->GetBounds(px, py, pwide, ptall);
		x = clamp(x, px, px + pwide - 1);
		y = clamp(y, py, py + ptall - 1);
		x = x - px;
		y = y - py;

		m_pParent->SetCirclePos(x, y);
	}

private:
	CPickerPanel *m_pParent = nullptr;
	bool m_bIsMousePressed = false;
};

}

colorpicker::CPickerPanel::CPickerPanel(vgui2::Panel *pParent, const char *panelName)
    : vgui2::EditablePanel(pParent, panelName)
{
	m_pImagePanel = new CPickerImagePanel(this, "PickerImage");
	m_pCircle = new CSelCirclePanel(this, "CirclePanel");
	m_pMousePanel = new CPickerMousePanel(this, "MousePanel");
}

colorpicker::CPickerPanel::~CPickerPanel()
{
}

void colorpicker::CPickerPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	int wide, tall;
	GetSize(wide, tall);
	m_pImagePanel->SetBounds(CIRCLE_SIZE / 2, CIRCLE_SIZE / 2, wide - CIRCLE_SIZE, tall - CIRCLE_SIZE);
	m_pMousePanel->SetBounds(0, 0, wide, tall);

	if (wide - CIRCLE_SIZE != PICKER_WIDE)
		Warning("colorpicker::CPickerPanel: wide doesn't match\n");
	if (tall - CIRCLE_SIZE != PICKER_TALL)
		Warning("colorpicker::CPickerPanel: tall doesn't match\n");
}

void colorpicker::CPickerPanel::SetCirclePos(int x, int y)
{
	m_pCircle->SetPos(x, y);
	m_pCircle->SetDrawColor(gTexMgr.GetColorForPickerPixel(m_pImagePanel->m_pPickerImg->GetTextureIndex(), x, y));

	m_flSat = 100.f * static_cast<float>(x) / static_cast<float>(m_pImagePanel->GetWide() + 1);
	m_flVal = 100.f * static_cast<float>(y) / static_cast<float>(m_pImagePanel->GetTall() + 1);
	m_flVal = 100.f - m_flVal;

	GetParentPicker()->OnColorHSVChanged();
}

void colorpicker::CPickerPanel::SetHSV(float hue, float sat, float val)
{
	int x = sat / 100.f * (m_pImagePanel->GetWide() - 1);
	int y = (100.f - val) / 100.f * (m_pImagePanel->GetTall() - 1);
	m_pCircle->SetPos(x, y);

	m_flHue = hue;
	m_flSat = sat;
	m_flVal = val;

	SetHue(m_flHue);
}

void colorpicker::CPickerPanel::SetHue(float hue)
{
	m_flHue = hue;

	Color c;
	HSVtoRGB(m_flHue, m_flSat, m_flVal, c);
	m_pCircle->SetDrawColor(c);
	m_pImagePanel->SetHue(hue);
}

void colorpicker::CPickerPanel::GetHSV(float &hue, float &sat, float &val)
{
	hue = m_flHue;
	sat = m_flSat;
	val = m_flVal;
}

CColorPicker *colorpicker::CPickerPanel::GetParentPicker()
{
	return static_cast<CColorPicker *>(BaseClass::GetParent());
}
