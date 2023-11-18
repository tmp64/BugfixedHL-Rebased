#include "compat/anniversary_compat.h"
#include "compat/FileSystemCompat.h"
#include "compat/ISchemeCompat.h"
#include "compat/ISurfaceCompat.h"

static IFileSystem *s_pEngineFileSystem = nullptr;
static IFileSystem *s_pFileSystemCompat = nullptr;
static vgui2::ISchemeManager *s_pSchemeManagerCompat = nullptr;
static vgui2::ISurface *s_pSurfaceCompat = nullptr;

template <typename TIface, typename TCompatIface>
using CompatFactoryFn = TIface*(*)(TCompatIface*);

template <typename TIface, typename TCompatIface>
static void FindAndCreateCompat(
	TIface *&compatInstance,
	CreateInterfaceFn factory,
    CompatFactoryFn<TIface, TCompatIface> compatFactory)
{
	if (compatInstance)
		return;

	// Find the interface
	const char *version = TCompatIface::VERSION;
	void *pRawIface = factory(version, nullptr);

	if (!pRawIface)
		return;

	compatInstance = compatFactory(static_cast<TCompatIface *>(pRawIface));
}

static void* CompatFactory(const char* pName, int* pReturnCode)
{
	if (!strcmp(pName, FILESYSTEM_INTERFACE_VERSION_ENGINE))
		return s_pEngineFileSystem;

	if (!strcmp(pName, FILESYSTEM_INTERFACE_VERSION))
		return s_pFileSystemCompat;

	if (!strcmp(pName, VGUI_SCHEME_INTERFACE_VERSION_GS))
		return s_pSchemeManagerCompat;

	if (!strcmp(pName, VGUI_SURFACE_INTERFACE_VERSION_GS))
		return s_pSurfaceCompat;

	return nullptr;
}

CreateInterfaceFn Compat_CreateFactory(CreateInterfaceFn *pFactories, int iNumFactories)
{
	for (int i = 0; i < iNumFactories; i++)
	{
		FindAndCreateCompat(s_pFileSystemCompat, pFactories[i], Compat_CreateFileSystem);
		FindAndCreateCompat(s_pSchemeManagerCompat, pFactories[i], Compat_CreateSchemeManager);
		FindAndCreateCompat(s_pSurfaceCompat, pFactories[i], Compat_CreateSurface);

		if (!s_pEngineFileSystem)
			s_pEngineFileSystem = static_cast<IFileSystem *>(pFactories[i](FILESYSTEM_INTERFACE_VERSION, nullptr));
	}

	if (!s_pFileSystemCompat)
		return nullptr;

	if (!s_pSchemeManagerCompat)
		return nullptr;

	if (!s_pSurfaceCompat)
		return nullptr;

	return CompatFactory;
}
