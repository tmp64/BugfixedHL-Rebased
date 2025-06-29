#pragma once
#include <vgui/IScheme.h>

class CVGuiSchemeManagerWrap : public vgui2::ISchemeManager
{
public:
    virtual vgui2::HScheme LoadSchemeFromFile( const char *fileName, const char *tag ) override { return m_pBase->LoadSchemeFromFile(fileName, tag); }
    virtual void ReloadSchemes() override { m_pBase->ReloadSchemes(); }
    virtual vgui2::HScheme GetDefaultScheme() override { return m_pBase->GetDefaultScheme(); }
    virtual vgui2::HScheme GetScheme( const char *tag ) override { return m_pBase->GetScheme(tag); }
    virtual vgui2::IImage *GetImage( const char *imageName, bool hardwareFiltered ) override { return m_pBase->GetImage(imageName, hardwareFiltered); }
    virtual vgui2::HTexture GetImageID( const char *imageName, bool hardwareFiltered ) override { return m_pBase->GetImageID(imageName, hardwareFiltered); }
    virtual vgui2::IScheme *GetIScheme( vgui2::HScheme scheme ) override { return m_pBase->GetIScheme(scheme); }
    virtual void Shutdown( bool full = true ) override { m_pBase->Shutdown(true); }
    virtual int GetProportionalScaledValue( int normalizedValue ) override { return m_pBase->GetProportionalScaledValue(normalizedValue); }
    virtual int GetProportionalNormalizedValue( int scaledValue ) override { return m_pBase->GetProportionalNormalizedValue(scaledValue); }
    virtual float GetProportionalScale() override { return m_pBase->GetProportionalScale(); }
    virtual int GetHDProportionalScaledValue(int normalizedValue) override { return m_pBase->GetHDProportionalScaledValue(normalizedValue); }
	virtual int GetHDProportionalNormalizedValue(int scaledValue) override { return m_pBase->GetHDProportionalNormalizedValue(scaledValue); }

protected:
	vgui2::ISchemeManager *m_pBase = nullptr;
};
