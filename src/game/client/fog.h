#ifndef FOG_H
#define FOG_H

#define FOG_START_DISTANCE	1500.0f
#define FOG_END_DISTANCE	2000.0f

struct FogParams
{
	bool fogSkybox;
	float color[3];
	float density;
};

class CFog
{
public:
	CFog();
	void SetFogParameters(const FogParams &params);
	FogParams GetFogParameters() const;
	void RenderFog();
	void ClearFog();
private:
	FogParams m_FogParams;
};

extern CFog gFog;

#endif // FOG_H