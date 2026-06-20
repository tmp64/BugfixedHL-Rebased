// vim: set ts=4 sw=4 tw=99 noet:

#ifndef HL_EXTENDED_API_MODULECONFIG_H
#define HL_EXTENDED_API_MODULECONFIG_H

#include "appversion.h"

#define MODULE_NAME     "HL Extended API"
#define MODULE_VERSION  APP_VERSION
#define MODULE_AUTHOR   "Pablo / Codex"
#define MODULE_URL      "https://github.com/tmp64/BugfixedHL"
#define MODULE_LOGTAG   "HL_EXT_API"
#define MODULE_LIBRARY  "hl_extended_api"
#define MODULE_LIBCLASS ""

#ifdef __DATE__
#define MODULE_DATE __DATE__
#else
#define MODULE_DATE "Unknown"
#endif

#define NO_ALLOC_OVERRIDES

#define FN_AMXX_ATTACH OnAmxxAttach
#define FN_AMXX_DETACH OnAmxxDetach

#ifdef USE_METAMOD
#define FN_DispatchSpawn      DispatchSpawn
#define FN_PlayerPreThink_Post PlayerPreThink_Post
#endif

#endif
