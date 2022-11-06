#ifndef VGUI_CROSSHAIR_IMAGE_H
#define VGUI_CROSSHAIR_IMAGE_H
#include <Color.h>
#include <vgui/IImage.h>

struct CrosshairSettings
{
	Color color = Color(255, 255, 255, 255);
	int gap = 0;
	int thickness = 1;
	int outline = 0;
	int size = 1;
	bool dot = false;
	bool t = false;
};

class CCrosshairImage : public vgui2::IImage
{
public:
	CCrosshairImage();

	void SetSettings(const CrosshairSettings &settings);

	// Image will draw within the current panel context at the specified position
	virtual void Paint() override;

	// Set the position of the image
	virtual void SetPos(int x, int y) override;

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall) override;

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall) override;

	// Sets the size of the image
	virtual void SetSize(int wide, int tall) override;

	// Set the draw color
	virtual void SetColor(Color col) override;

private:
	CrosshairSettings m_Settings;
	int m_nX = 0, m_nY = 0;
	int m_wide = 0, m_tall = 0;

	void DrawVLine(int x0, int y0, int len, int thick);
	void DrawHLine(int x0, int y0, int len, int thick);
	void DrawDot(int x, int y, int size);
};

#endif
