#ifndef HUD_STRAFEGUIDE_H
#define HUD_STRAFEGUIDE_H
#include "base.h"
#include <complex>

class CHudStrafeGuide : public CHudElemBase<CHudStrafeGuide>
{
public:
	virtual void Init();
	virtual void VidInit();
	virtual void Draw(float time);

	void Update(struct ref_params_s *ppmove);

private:
	double angles[6] = { 0. };

	std::complex<double> lastSimvel = 0.;
};

#endif
