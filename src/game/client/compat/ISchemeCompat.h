#ifndef ISCHEMECOMPAT_H
#define ISCHEMECOMPAT_H
#include <vgui/IScheme.h>

namespace vgui2
{

class ISchemeManagerCompat : public IBaseInterface
{
public:
	static constexpr char VERSION[] = "VGUI_Scheme009";

	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	virtual HScheme LoadSchemeFromFile( const char *fileName, const char *tag ) = 0;

	// reloads the scheme from the file - should only be used during development
	virtual void ReloadSchemes() = 0;

	// reloads scheme fonts
	//virtual void ReloadFonts() = 0;

	// returns a handle to the default (first loaded) scheme
	virtual HScheme GetDefaultScheme() = 0;

	// returns a handle to the scheme identified by "tag"
	virtual HScheme GetScheme( const char *tag ) = 0;

	// returns a pointer to an image
	virtual IImage *GetImage( const char *imageName, bool hardwareFiltered ) = 0;
	virtual HTexture GetImageID( const char *imageName, bool hardwareFiltered ) = 0;

	// This can only be called at certain times, like during paint()
	// It will assert-fail if you call it at the wrong time...

	// FIXME: This interface should go away!!! It's an icky back-door
	// If you're using this interface, try instead to cache off the information
	// in ApplySchemeSettings
	virtual IScheme *GetIScheme( HScheme scheme ) = 0;

	// unload all schemes
	virtual void Shutdown( bool full = true ) = 0;

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValue( int normalizedValue ) = 0;
	virtual int GetProportionalNormalizedValue( int scaledValue ) = 0;
};

} // namespace vgui2

//! Creates the interface adapter.
vgui2::ISchemeManager *Compat_CreateSchemeManager(vgui2::ISchemeManagerCompat *pIface);

#endif // ISCHEME_H
