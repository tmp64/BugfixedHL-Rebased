#include "texture_manager.h"
#include "../color_picker.h"
#include <vgui/ISurface.h>
#include "hud.h"
#include "opengl.h"

colorpicker::CTextureManager colorpicker::gTexMgr(colorpicker::PICKER_WIDE, colorpicker::PICKER_TALL);

//----------------------------------------------------------
// colorpicker::CTextureManager
//----------------------------------------------------------
colorpicker::CTextureManager::CTextureManager(int wide, int tall)
    : m_iWide(wide)
    , m_iTall(tall)
    , m_Worker(this)
{
}

void colorpicker::CTextureManager::Init()
{
	if (CClientOpenGL::Get().IsAvailable())
	{
		// The color picker will be drawn via OpenGL, textures are not required.
		return;
	}

	m_Worker.StartThread();

	// Print VRAM usage
	int texwide = GetTextureWide();
	int textall = GetTextureTall();
	int barPixels = texwide * 1;
	int pickerPixels = texwide * textall;
	int totalPixels = barPixels + pickerPixels * texwide;
	gEngfuncs.Con_DPrintf("Color Picker textures: %.2f MB\n", totalPixels * 4 / 1024.0 / 1024.0);
}

void colorpicker::CTextureManager::RunFrame()
{
	if (!m_bIsReady && !m_bIsError && m_bIsThreadReady)
	{
		m_Worker.StopThread();
		gEngfuncs.Con_DPrintf("ColorPicker: Textures ready at %.3f s\n", gEngfuncs.GetAbsoluteTime());

		int texwide = GetTextureWide();
		int textall = GetTextureTall();

		if (m_Worker.GetPickerRgba().size() == 0)
		{
			m_bIsError = true;
			return;
		}

		m_iBarTexture = vgui2::surface()->CreateNewTextureID(true);
		vgui2::surface()->DrawSetTextureRGBA(
		    m_iBarTexture,
		    m_Worker.GetBarRgba().data(),
		    texwide,
		    1, true, false);

		auto picker = m_Worker.GetPickerRgba();
		m_PickerTextures.resize(picker.size());
		for (size_t i = 0; i < picker.size(); i++)
		{
			m_PickerTextures[i] = vgui2::surface()->CreateNewTextureID(true);
			vgui2::surface()->DrawSetTextureRGBA(
			    m_PickerTextures[i],
			    picker[i].data(),
			    texwide,
			    textall,
			    true, false);
		}

		m_Worker.ClearRgba();

		m_bIsReady = true;
	}
}

void colorpicker::CTextureManager::Shutdown()
{
	m_Worker.StopThread();
}

int colorpicker::CTextureManager::GetWide()
{
	return m_iWide;
}

int colorpicker::CTextureManager::GetTall()
{
	return m_iTall;
}

int colorpicker::CTextureManager::GetBarTextureId()
{
	return m_iBarTexture;
}

int colorpicker::CTextureManager::GetPickerTexture(int idx)
{
	Assert(idx >= 0 && idx < (int)m_PickerTextures.size());
	return m_PickerTextures[idx];
}

int colorpicker::CTextureManager::GetPickerTextureIndex(float hue)
{
	return static_cast<int>(hue / 360.f * (float)GetTextureWide());
}

Color colorpicker::CTextureManager::GetColorForPickerPixel(int idx, int x, int y)
{
	int wide = GetWide(), tall = GetTall();
	float hue = 360.f * static_cast<float>(idx) / static_cast<float>(GetTextureWide() + 1);
	float sat = 100.f * static_cast<float>(x) / static_cast<float>(wide - 1);
	float val = 100.f * static_cast<float>(tall - 1 - y) / static_cast<float>(tall - 1);
	Color c;
	HSVtoRGB(hue, sat, val, c);
	return c;
}

int colorpicker::CTextureManager::GetTextureWide()
{
	return round((double)m_iWide * SCALE);
}

int colorpicker::CTextureManager::GetTextureTall()
{
	return round((double)m_iTall * SCALE);
}

//----------------------------------------------------------
// colorpicker::CTextureManager::CWorker
//----------------------------------------------------------
colorpicker::CTextureManager::CWorker::CWorker(colorpicker::CTextureManager *pParent)
{
	m_pParent = pParent;
}

void colorpicker::CTextureManager::CWorker::StartThread()
{
	m_Thread = std::thread([this]() { (*this)(); });
}

void colorpicker::CTextureManager::CWorker::StopThread()
{
	if (m_Thread.joinable())
		m_Thread.join();
}

std::vector<unsigned char> &colorpicker::CTextureManager::CWorker::GetBarRgba()
{
	return m_BarRgba;
}

std::vector<std::vector<unsigned char>> &colorpicker::CTextureManager::CWorker::GetPickerRgba()
{
	return m_PickerRgba;
}

void colorpicker::CTextureManager::CWorker::ClearRgba()
{
	std::vector<unsigned char>().swap(m_BarRgba);
	std::vector<std::vector<unsigned char>>().swap(m_PickerRgba);
}

void colorpicker::CTextureManager::CWorker::operator()()
{
	try
	{
		GenerateBarTexture();
		GeneratePickerTexture();
	}
	catch (...)
	{
		// Ran out of memory or something. Main thread will set m_bError to true.
		ClearRgba();
	}

	m_pParent->m_bIsThreadReady = true;
}

void colorpicker::CTextureManager::CWorker::GenerateBarTexture()
{
	int size = m_pParent->GetTextureWide();
	m_BarRgba.resize(size * 1 * PIXEL_SIZE); // wide * tall * (4 bytes RGBA)

	for (int i = 0; i < size; i++)
	{
		float h = 360.f * static_cast<float>(i) / static_cast<float>(size + 1);
		Color rgb;
		HSVtoRGB(h, 100, 100, rgb);
		m_BarRgba[i * PIXEL_SIZE + 0] = rgb[0];
		m_BarRgba[i * PIXEL_SIZE + 1] = rgb[1];
		m_BarRgba[i * PIXEL_SIZE + 2] = rgb[2];
		m_BarRgba[i * PIXEL_SIZE + 3] = 255;
	}
}

void colorpicker::CTextureManager::CWorker::GeneratePickerTexture()
{
	int xsize = m_pParent->GetTextureWide();
	int ysize = m_pParent->GetTextureTall();

	m_PickerRgba.resize(xsize);

	for (int i = 0; i < xsize; i++)
	{
		std::vector<unsigned char> &data = m_PickerRgba[i];
		data.resize(xsize * ysize * PIXEL_SIZE); // wide * tall * (4 bytes RGBA)
		float h = 360.f * static_cast<float>(i) / static_cast<float>(xsize + 1);

		for (int y = 0; y < ysize; y++)
		{
			float v = 100.f * static_cast<float>(ysize - 1 - y) / static_cast<float>(ysize - 1);
			for (int x = 0; x < xsize; x++)
			{
				float s = 100.f * static_cast<float>(x) / static_cast<float>(xsize - 1);

				Color rgb;
				HSVtoRGB(h, s, v, rgb);
				int idx = y * xsize * PIXEL_SIZE + x * PIXEL_SIZE;

				data[idx + 0] = rgb[0];
				data[idx + 1] = rgb[1];
				data[idx + 2] = rgb[2];
				data[idx + 3] = 255;
			}
		}
	}
}
