#ifndef HUD_JUMPSPEED_H
#define HUD_JUMPSPEED_H
#include "base.h"

#define FADE_DURATION_JUMPSPEED 0.7f

class CHudJumpspeed : public CHudElemBase<CHudJumpspeed>
{
public:
	virtual void Init();
	virtual void VidInit();
	virtual void Draw(float time);
	void UpdateSpeed(const float velocity[3]);

private:
	int m_iOldSpeed;
	int m_iSpeed;
	float m_fFade;

	int fadingFrom[3];
	float prevVel[3] = { 0.0f, 0.0f, 0.0f };
	float lastTime;
	double passedTime;
};

#endif
