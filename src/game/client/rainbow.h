#ifndef RAINBOW_H
#define RAINBOW_H
#include <functional>
#include <hud.h>

class CRainbow
{
public:
	void Think();

	/**
	 * Returns whether Rainbow HUD is enabled.
	 */
	bool IsEnabled();

	/**
	 * Converts input color to rainbow.
	 */
	void GetRainbowColor(int x, int y, int &r, int &g, int &b);

	/**
	 * Overwrites some gEngfuncs members with color-changing wrappers.
	 * Makes the player happier.
	 */
	void HookFuncs();

private:
	/**
	 * Function that draws an input string at input position with input color.
	 * @returns Width the string
	 */
	using DrawStringFn = std::function<int(int x, int y, const char *buf, int r, int g, int b)>;

	float m_flSat = 100;
	float m_flVal = 100;

	HSPRITE m_hSprite = 0;
	int m_iSpriteColor[3] = { 0, 0, 0 };

	decltype(gEngfuncs.pfnSPR_Set) m_pfnSPR_Set = nullptr;
	decltype(gEngfuncs.pfnSPR_DrawAdditive) m_pfnSPR_DrawAdditive = nullptr;
	decltype(gEngfuncs.pfnDrawString) m_pfnDrawString = nullptr;
	decltype(gEngfuncs.pfnDrawStringReverse) m_pfnDrawStringReverse = nullptr;
	decltype(gEngfuncs.pfnDrawConsoleString) m_pfnDrawConsoleString = nullptr;
	decltype(gEngfuncs.pfnFillRGBA) m_pfnFillRGBA = nullptr;

	static void SPR_SetRainbow(HSPRITE hPic, int r, int g, int b);
	static void SPR_DrawAdditiveRainbow(int frame, int x, int y, const struct rect_s *prc);
	static int DrawString(int x, int y, const char *str, int r, int g, int b);
	static int DrawStringReverse(int x, int y, const char *str, int r, int g, int b);
	static int DrawConsoleString(int x, int y, const char *string);
	static void FillRGBARainbow(int x, int y, int width, int height, int r, int g, int b, int a);

	/**
	 * Draws a string using specified drawing func.
	 * func will be called for every character with a new color.
	 */
	static int DrawRainbowString(int x, int y, const char *str, const DrawStringFn &func);

	/**
	 * Converts color from HSV color space to RGB
	 * @param	H	Hue, [0, 360]
	 * @param	S	Saturation, [0, 100]
	 * @param	V	Value, [0, 100]
	 * @param	R	Red output, [0, 255]
	 * @param	G	Green output, [0, 255]
	 * @param	B	Blue output,  [0, 255]
	 */
	static void HSVtoRGB(float H, float S, float V, int &R, int &G, int &B);
};

#endif
