#include "hud.h"
#include "fog.h"
#include "triangleapi.h"

CFog gFog;

CFog::CFog()
{
	ClearFog();
}

void CFog::SetWaterLevel(int waterLevel)
{
	m_iWaterLevel = waterLevel;
}

void CFog::SetFogParameters(const FogParams &params)
{
	m_FogParams = params;
}

FogParams CFog::GetFogParameters() const
{
	return m_FogParams;
}

void CFog::RenderFog()
{
	bool bFog;

	if (m_iWaterLevel <= 2) // checking if player is not underwater
		bFog = (m_FogParams.density > 0.0f) ? true : false;
	else
		bFog = false;

	gEngfuncs.pTriAPI->FogParams(m_FogParams.density, m_FogParams.fogSkybox);
	gEngfuncs.pTriAPI->Fog(m_FogParams.color, FOG_START_DISTANCE, FOG_END_DISTANCE, bFog);
}

void CFog::ClearFog()
{
	m_FogParams.fogSkybox = false;
	m_FogParams.color[0] = m_FogParams.color[1] = m_FogParams.color[2] = 0.0f;
	m_FogParams.density = 0.0f;
}