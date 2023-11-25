#ifndef ENGINE_BUILD_H
#define ENGINE_BUILD_H

// Some build numbers are larger on Linux
#ifdef PLATFORM_LINUX
#define LINUX_OFFSET 1
#else
#define LINUX_OFFSET 0
#endif

//! Protocol 48 builds.
constexpr int ENGINE_BUILD_P48 = 4000;

//! Probably the last version before SteamPipe.
constexpr int ENGINE_BUILD_PRE_STEAMPIPE = 4617;

//! First builds of SteamPipe.
constexpr int ENGINE_BUILD_STEAMPIPE = 5940;

//! First build of the Anniversary Update (2023-11-16).
//! Changes a bunch of interfaces (see https://github.com/ValveSoftware/halflife/issues/3442)
constexpr int ENGINE_BUILD_ANNIVERSARY_FIRST = 9884 + LINUX_OFFSET;

//! First build of the Anniversary Update (2023-11-22).
//! Reverts the interface changes (see https://github.com/ValveSoftware/halflife/issues/3442).
//! Adds new interface changes.
constexpr int ENGINE_BUILD_ANNIVERSARY_FIXED_INTERFACES = 9890 + LINUX_OFFSET;

#undef LINUX_OFFSET

#endif
