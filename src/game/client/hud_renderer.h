#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H
#include "hud.h"

class CHudRenderer
{
public:
	static CHudRenderer &Get();

	bool IsAvailable();
	void HookFuncs();

	static void SpriteSet(HSPRITE hPic, int r, int g, int b);
	static void SpriteDraw(int frame, int x, int y, const wrect_t *prc);
	static void SpriteDrawAdditive(int frame, int x, int y, const wrect_t *prc);

private:
	enum class SpriteDrawMode
	{
		Normal,
		Additive,
	};

	HSPRITE m_hPic = 0;
	uint8_t m_SpriteColor[4];

	void DrawSprite(int frame, float x, float y, float width, float height, const wrect_t *prc, SpriteDrawMode mode);
};

#endif
