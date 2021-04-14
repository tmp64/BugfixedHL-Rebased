#ifndef HUD_SPEEDOMETER_H
#define HUD_SPEEDOMETER_H
#include "base.h"

class CHudSpeedometer : public CHudElemBase<CHudSpeedometer>
{
public:
	virtual void Init();
	virtual void VidInit();
	virtual void Draw(float time);
	void UpdateSpeed(const float velocity[2]);

private:
	int m_iOldSpeed;
	int m_iSpeed;
	float m_fFade;
};

#endif
