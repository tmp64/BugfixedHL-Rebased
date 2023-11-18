#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include "compat/ISchemeCompat.h"

using namespace vgui2;

class CSchemeManagerCompat final : public vgui2::ISchemeManager
{
public:
	CSchemeManagerCompat(vgui2::ISchemeManagerCompat* pIface)
	{
		m_pIface = pIface;
	}

	virtual HScheme LoadSchemeFromFile(const char *fileName, const char *tag) override
	{
		return m_pIface->LoadSchemeFromFile(fileName, tag);
	}

	virtual void ReloadSchemes() override
	{
		m_pIface->ReloadSchemes();
	}

	virtual HScheme GetDefaultScheme() override
	{
		return m_pIface->GetDefaultScheme();
	}

	virtual HScheme GetScheme(const char *tag) override
	{
		return m_pIface->GetScheme(tag);
	}

	virtual IImage *GetImage(const char *imageName, bool hardwareFiltered) override
	{
		return m_pIface->GetImage(imageName, hardwareFiltered);
	}

	virtual HTexture GetImageID(const char *imageName, bool hardwareFiltered) override
	{
		return m_pIface->GetImageID(imageName, hardwareFiltered);
	}

	virtual IScheme *GetIScheme(HScheme scheme) override
	{
		return m_pIface->GetIScheme(scheme);
	}

	virtual void Shutdown(bool full) override
	{
		m_pIface->Shutdown(full);
	}

	virtual float GetProportionalScale() override
	{
		constexpr int BASE_HEIGHT = 480;
		int wide, tall;
		surface()->GetScreenSize(wide, tall);
		return (float)tall / BASE_HEIGHT;
	}

	virtual int GetProportionalScaledValue(int normalizedValue) override
	{
		return m_pIface->GetProportionalScaledValue(normalizedValue);
	}

	virtual int GetProportionalNormalizedValue(int scaledValue) override
	{
		return m_pIface->GetProportionalNormalizedValue(scaledValue);
	}

private:
	vgui2::ISchemeManagerCompat *m_pIface = nullptr;
};

vgui2::ISchemeManager *Compat_CreateSchemeManager(vgui2::ISchemeManagerCompat *pIface)
{
	return new CSchemeManagerCompat(pIface);
}
