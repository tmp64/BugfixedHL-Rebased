#ifndef ANNIVERSARY_COMPAT_H
#define ANNIVERSARY_COMPAT_H
#include <tier1/interface.h>

//! Provides compatibility with pre-Anniversary-Update game versions.
//! Works by creating wrappers for new (hl1_source_sdk) interfaces that call old ones.
//! This factory will provide those wrappers.
CreateInterfaceFn Compat_CreateFactory(CreateInterfaceFn *pFactories, int iNumFactories);

#endif
