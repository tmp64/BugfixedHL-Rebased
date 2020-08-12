#ifndef HUD_CROSSHAIR_H
#define HUD_CROSSHAIR_H
#include "base.h"
#include "vgui/crosshair_image.h"

class CHudCrosshair : public CHudElemBase<CHudCrosshair>
{
public:
	CHudCrosshair();
	virtual void Init();
	virtual void Draw(float flTime);

	bool IsEnabled();

private:
	CCrosshairImage m_Img;
};

#endif
