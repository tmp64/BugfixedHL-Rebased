#include <atomic>
#include <cctype>
#include <map>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Image.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/TextEntry.h>
#include "client_vgui.h"
#include "color_picker.h"
#include "colorpicker/picker_panel.h"
#include "colorpicker/bar_panel.h"

namespace colorpicker
{
}

void colorpicker::HSVtoRGB(float h, float s, float v, Color &output)
{
	Assert(h >= 0.f && h < 360.f);
	Assert(s >= 0.f && s <= 100.f);
	Assert(v >= 0.f && v <= 100.f);

	int i;
	float f, p, q, t;
	uint8_t r, g, b;

	s /= 100;
	v /= 100;

	if (s == 0)
	{
		// Achromatic (gray)
		output[0] = output[1] = output[2] = (uint8_t)round(v * 255);
		output[3] = 255;
		return;
	}

	h /= 60.0;
	i = (int)floor(h); // sector 0 to 5
	f = h - (float)i; // factorial part of h
	p = (float)(v * (1.0 - s));
	q = (float)(v * (1.0 - s * f));
	t = (float)(v * (1.0 - s * (1 - f)));
	switch (i)
	{
	case 0:
		r = (uint8_t)round(255 * v);
		g = (uint8_t)round(255 * t);
		b = (uint8_t)round(255 * p);
		break;
	case 1:
		r = (uint8_t)round(255 * q);
		g = (uint8_t)round(255 * v);
		b = (uint8_t)round(255 * p);
		break;
	case 2:
		r = (uint8_t)round(255 * p);
		g = (uint8_t)round(255 * v);
		b = (uint8_t)round(255 * t);
		break;
	case 3:
		r = (uint8_t)round(255 * p);
		g = (uint8_t)round(255 * q);
		b = (uint8_t)round(255 * v);
		break;
	case 4:
		r = (uint8_t)round(255 * t);
		g = (uint8_t)round(255 * p);
		b = (uint8_t)round(255 * v);
		break;
	default: // case 5:
		r = (uint8_t)round(255 * v);
		g = (uint8_t)round(255 * p);
		b = (uint8_t)round(255 * q);
	}

	output[0] = r;
	output[1] = g;
	output[2] = b;
	output[3] = 255;
}

void colorpicker::RGBtoHSV(Color rgb, float &hue, float &sat, float &val)
{
	double delta, min;
	double h = 0, s, v;

	min = std::min(std::min(rgb.r(), rgb.g()), rgb.b());
	v = std::max(std::max(rgb.r(), rgb.g()), rgb.b());
	delta = v - min;

	if (v == 0.0)
		s = 0;
	else
		s = delta / v;

	if (s == 0)
		h = 0.0;

	else
	{
		if (rgb.r() == v)
			h = (rgb.g() - rgb.b()) / delta;
		else if (rgb.g() == v)
			h = 2 + (rgb.b() - rgb.r()) / delta;
		else if (rgb.b() == v)
			h = 4 + (rgb.r() - rgb.g()) / delta;

		h *= 60;

		if (h < 0.0)
			h = h + 360;
	}

	//return HSV(h, s, (v / 255));
	hue = h;
	sat = s * 100;
	val = v / 255 * 100;
}

//----------------------------------------------------------
// CColorPreviewImage
//----------------------------------------------------------
namespace colorpicker
{

class CColorPreviewImage : public vgui2::IImage
{
public:
	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint()
	{
		int posX = m_nX + m_offX;
		int posY = m_nY + m_offY;

		int constexpr BORDER_THICK = 2;

		vgui2::surface()->DrawSetColor(Color(0, 0, 0, 255));
		vgui2::surface()->DrawFilledRect(posX, posY, posX + m_wide, posY + m_tall);

		vgui2::surface()->DrawSetColor(m_OldColor);
		vgui2::surface()->DrawFilledRect(
		    posX + BORDER_THICK, posY + BORDER_THICK,
		    posX + m_wide - BORDER_THICK, posY + m_tall / 2);

		vgui2::surface()->DrawSetColor(m_NewColor);
		vgui2::surface()->DrawFilledRect(
		    posX + BORDER_THICK, posY + m_tall / 2,
		    posX + m_wide - BORDER_THICK, posY + m_tall - BORDER_THICK);
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
	}

	void SetOldColor(Color col)
	{
		m_OldColor = col;
	}

	void SetNewColor(Color col)
	{
		m_NewColor = col;
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
	Color m_OldColor, m_NewColor;
	int m_nX = 0, m_nY = 0;
	int m_wide = 0, m_tall = 0;
	int m_offX = 0, m_offY = 0;
};

}

//----------------------------------------------------------
// CColorPicker
//----------------------------------------------------------
CColorPicker::CColorPicker(vgui2::Panel *parent, const char *panelName, const char *title)
    : vgui2::Frame(parent, panelName)
{
	SetTitle(title, true);
	SetMaximizeButtonVisible(false);
	SetMinimizeButtonVisible(false);
	SetCloseButtonVisible(true);
	SetSizeable(false);

	m_pPickerPanel = new colorpicker::CPickerPanel(this, "PickerPanel");
	m_pBarPanel = new colorpicker::CBarPanel(this, "BarPanel");
	m_pPreviewImage = new colorpicker::CColorPreviewImage();

	m_pPreviewPanel = new vgui2::ImagePanel(this, "PreviewImage");
	m_pPreviewPanel->SetImage(m_pPreviewImage);

	m_pRgbTextPanel = new vgui2::TextEntry(this, "RgbText");
	m_pRgbTextPanel->AddActionSignalTarget(this);

	m_pOkButton = new vgui2::Button(this, "OkBtn", "#VGUI_Ok", this, "OkClicked");
	m_pCancelButton = new vgui2::Button(this, "CancelBtn", "#VGUI_Cancel", this, "CancelClicked");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/ColorPicker.res");
	InvalidateLayout();
	MakePopup();
	SetVisible(false); // Hide by default

	SetInitialColor(Color(127, 255, 0, 255)); // Reset color
}

CColorPicker::~CColorPicker()
{
	m_pPreviewPanel->SetImage(static_cast<vgui2::IImage *>(nullptr));
	delete m_pPreviewImage;
	m_pPreviewImage = nullptr;
}

void CColorPicker::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	/*m_pPickerImage->SetImage(CColorPickerColorImg::GetImage(
		m_pPickerImage->GetWide(), m_pPickerImage->GetTall()
	));*/

	/*m_pBarImage->SetImage(CColorPickerBarImg::GetImage(
		m_pPickerImage->GetWide(), m_pBarImage->GetTall(), m_pPickerImage->GetTall()
	));*/
	int wide, tall;
	m_pPreviewPanel->GetSize(wide, tall);
	m_pPreviewImage->SetSize(wide, tall);
}

void CColorPicker::Activate()
{
	MoveToCenterOfScreen();
	BaseClass::Activate();
}

void CColorPicker::OnCommand(const char *pMsg)
{
	if (!strcmp(pMsg, "OkClicked"))
	{
		SetVisible(false);
		KeyValues *kv = new KeyValues("ColorPicked");
		kv->SetInt("color", m_SelectedColor.GetRawColor());
		PostActionSignal(kv);
	}
	else if (!strcmp(pMsg, "CancelClicked"))
	{
		SetVisible(false);
	}
	else
		BaseClass::OnCommand(pMsg);
}

void CColorPicker::SetColor(Color color)
{
	float h, s, v;
	colorpicker::RGBtoHSV(color, h, s, v);
	m_pBarPanel->SetHue(h);
	m_pPickerPanel->SetHSV(h, s, v);
	OnColorHSVChanged();
}

void CColorPicker::SetInitialColor(Color color)
{
	m_pPreviewImage->SetOldColor(color);
	SetColor(color);
}

void CColorPicker::OnColorHSVChanged()
{
	float h, s, v;
	m_pPickerPanel->GetHSV(h, s, v);
	Color c;
	colorpicker::HSVtoRGB(h, s, v, c);
	m_SelectedColor = c;

	char buf[128];
	snprintf(buf, sizeof(buf), "%d %d %d", c.r(), c.g(), c.b());
	m_pRgbTextPanel->SetText(buf);

	m_pPreviewImage->SetNewColor(m_SelectedColor);
}

void CColorPicker::OnTextChanged(KeyValues *kv)
{
	if (kv->GetPtr("panel") == m_pRgbTextPanel)
	{
		char buf[128]; // RRR GGG BBB
		char *ptr = buf;
		m_pRgbTextPanel->GetText(buf, sizeof(buf));

		Color col(0, 0, 0, 255);

		auto fnParseNumber = [&](int colorIdx) {
			// Skip spaces
			while (*ptr == ' ')
				ptr++;

			if (*ptr == '\0')
				return false;

			char *begin = ptr;

			// Find next space or end of string
			while (*ptr != ' ' && *ptr != '\0')
				ptr++;
			char charToSwap = '\0';
			std::swap(*ptr, charToSwap);

			// Check chars of the number
			for (char *i = begin; *i; i++)
				if (!isdigit(*i))
					return false; // Invalid digit

			long val = strtol(begin, nullptr, 10);
			if (val < 0 || val > 255)
				return false; // Invalid for color
			col[colorIdx] = val;

			std::swap(*ptr, charToSwap);

			return true;
		};

		if (fnParseNumber(0) && fnParseNumber(1) && fnParseNumber(2))
		{
			m_SelectedColor = col;

			float h, s, v;
			colorpicker::RGBtoHSV(m_SelectedColor, h, s, v);
			m_pBarPanel->SetHue(h);
			m_pPickerPanel->SetHSV(h, s, v);

			m_pPreviewImage->SetNewColor(m_SelectedColor);
		}
	}
}
