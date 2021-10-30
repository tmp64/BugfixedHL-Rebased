#ifndef CCOLORPICKER_H
#define CCOLORPICKER_H
#include <vgui_controls/Frame.h>

namespace vgui2
{
class Label;
class TextEntry;
class ImagePanel;
class Button;
}

namespace colorpicker
{
int constexpr PICKER_WIDE = 300;
int constexpr PICKER_TALL = 160;
int constexpr BAR_TALL = 8;
int constexpr CIRCLE_SIZE = 20;

class CPickerPanel;
class CBarPanel;
class CColorPreviewImage;

// hue 0 - 360
// sat and val 0 - 100
void HSVtoRGB(float h, float s, float v, Color &output);
void RGBtoHSV(Color rgb, float &hue, float &sat, float &val);
}

//-------------------------------------------------------------
// A pop-up that, when activated, will show a color picker dialog.
// When OK clicked, a "ColorPicked" message will be sent.
// Use like this:
//		MESSAGE_FUNC_PARAMS(OnColorPicked, "ColorPicked", kv);
//		void OnColorPicked(KeyValues *kv)
//			Color col;
//			col.SetRawColor(kv->GetInt("color"));
//-------------------------------------------------------------
class CColorPicker : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CColorPicker, vgui2::Frame);

public:
	CColorPicker(vgui2::Panel *parent, const char *panelName, const char *title);
	virtual ~CColorPicker();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void Activate();
	virtual void OnCommand(const char *pMsg);

	void SetColor(Color color);
	void SetInitialColor(Color color);

private:
	Color m_SelectedColor;
	colorpicker::CPickerPanel *m_pPickerPanel = nullptr;
	colorpicker::CBarPanel *m_pBarPanel = nullptr;
	colorpicker::CColorPreviewImage *m_pPreviewImage = nullptr;
	vgui2::ImagePanel *m_pPreviewPanel = nullptr;
	vgui2::TextEntry *m_pRgbTextPanel = nullptr;
	vgui2::Button *m_pOkButton = nullptr;
	vgui2::Button *m_pCancelButton = nullptr;

	void OnColorHSVChanged(); // Called from SetColor(), CPickerPanel, CBarPanel
	MESSAGE_FUNC_PARAMS(OnTextChanged, "TextChanged", kv);

	friend class colorpicker::CPickerPanel;
	friend class colorpicker::CBarPanel;
};

#endif
