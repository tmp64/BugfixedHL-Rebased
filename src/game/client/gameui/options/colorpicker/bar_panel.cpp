#include "bar_panel.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui/IImage.h>
#include <vgui/ISurface.h>
#include "../color_picker.h"
#include "texture_manager.h"
#include "sel_circle_panel.h"
#include "picker_panel.h"
#include "opengl.h"

namespace colorpicker
{

class CBarImage : public vgui2::IImage
{
public:
	CBarImage()
	{
		m_wide = PICKER_WIDE;
		m_tall = BAR_TALL;
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
			// Credits to Dear ImGui (ImGui::ColorPicker4 function).
			constexpr uint8_t hues[6 + 1][4] = {
				{ 255, 0, 0, 255 },
				{ 255, 255, 0, 255 },
				{ 0, 255, 0, 255 },
				{ 0, 255, 255, 255 },
				{ 0, 0, 255, 255 },
				{ 255, 0, 255, 255 },
				{ 255, 0, 0, 255 }
			};

			glDisable(GL_TEXTURE_2D);
			glShadeModel(GL_SMOOTH);

			glBegin(GL_QUADS);

			for (int i = 0; i < 6; i++)
			{
				float left = posX + i * (m_wide / 6.0f);
				float right = left + (m_wide / 6.0f);

				glColor4ubv(hues[i]);
				glVertex2f(left, posY);

				glColor4ubv(hues[i + 1]);
				glVertex2f(right, posY);

				glColor4ubv(hues[i + 1]);
				glVertex2f(right, posY + m_tall);

				glColor4ubv(hues[i]);
				glVertex2f(left, posY + m_tall);
			}

			glEnd();

			glEnable(GL_TEXTURE_2D);
		}
		else if (gTexMgr.IsReady())
		{
			vgui2::surface()->DrawSetTexture(gTexMgr.GetBarTextureId());
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
};

class CBarImagePanel : public vgui2::ImagePanel
{
	DECLARE_CLASS_SIMPLE(CBarImagePanel, vgui2::ImagePanel);

public:
	CBarImage *m_pPickerImg = nullptr;

	CBarImagePanel(colorpicker::CBarPanel *parent, const char *panelname)
	    : vgui2::ImagePanel(parent, panelname)
	{
		m_pParent = parent;
		m_pPickerImg = new CBarImage();
		SetImage(m_pPickerImg);
	}

	virtual ~CBarImagePanel()
	{
		SetImage(static_cast<vgui2::IImage *>(nullptr));
		delete m_pPickerImg;
		m_pPickerImg = nullptr;
	}

private:
	CBarPanel *m_pParent = nullptr;
};

class CBarMousePanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CBarMousePanel, vgui2::Panel);

public:
	CBarMousePanel(colorpicker::CBarPanel *parent, const char *panelname)
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
		int px, py;
		m_pParent->m_pImagePanel->GetPos(px, py);
		x = clamp(x, px, px + m_pParent->m_pImagePanel->GetWide() - 1);
		x = x - px;
		m_pParent->SetCirclePos(x);
	}

private:
	CBarPanel *m_pParent = nullptr;
	bool m_bIsMousePressed = false;
};

}

colorpicker::CBarPanel::CBarPanel(vgui2::Panel *pParent, const char *panelName)
    : vgui2::EditablePanel(pParent, panelName)
{
	m_pImagePanel = new CBarImagePanel(this, "PickerImage");
	m_pCircle = new CSelCirclePanel(this, "CirclePanel");
	m_pMousePanel = new CBarMousePanel(this, "MousePanel");
}

colorpicker::CBarPanel::~CBarPanel()
{
}

void colorpicker::CBarPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	int wide, tall;
	GetSize(wide, tall);

	int panelY = (tall - BAR_TALL) / 2;

	m_pImagePanel->SetBounds(CIRCLE_SIZE / 2, panelY, wide - CIRCLE_SIZE, BAR_TALL);
	m_pMousePanel->SetBounds(0, 0, wide, tall);

	if (wide - CIRCLE_SIZE != PICKER_WIDE)
		Warning("colorpicker::CBarPanel: wide doesn't match\n");
	if (tall != CIRCLE_SIZE)
		Warning("colorpicker::CBarPanel: tall doesn't match\n");
}

void colorpicker::CBarPanel::SetCirclePos(int x)
{
	m_pCircle->SetPos(x, 0);
	float hue = 360.f * static_cast<float>(x) / static_cast<float>(m_pImagePanel->GetWide() + 1);
	Color c;
	HSVtoRGB(hue, 100, 100, c);
	m_pCircle->SetDrawColor(c);
	GetParentPicker()->m_pPickerPanel->SetHue(hue);

	GetParentPicker()->OnColorHSVChanged();
}

void colorpicker::CBarPanel::SetHue(float hue)
{
	int x = hue / 360.f * (m_pImagePanel->GetWide() + 1);
	m_pCircle->SetPos(x, 0);

	Color c;
	HSVtoRGB(hue, 100, 100, c);
	m_pCircle->SetDrawColor(c);
}

float colorpicker::CBarPanel::GetHue()
{
	int x, y;
	m_pCircle->GetPos(x, y);
	return 360.0f * static_cast<float>(x) / GetWide() + 1;
}

CColorPicker *colorpicker::CBarPanel::GetParentPicker()
{
	return static_cast<CColorPicker *>(BaseClass::GetParent());
}
