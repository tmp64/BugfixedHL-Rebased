#ifndef BHL_API_H
#define BHL_API_H
#include <vinterface/interface.h>
#include <IBugfixedServer.h>
#include <IGameVersion.h>

namespace bhl
{

enum class E_ApiInitResult
{
	OK = 0,
	ModuleNotFound, // Sys_LoadModule returned nullptr
	FactoryNotFound, // Sys_GetFactory returned nullptr
	InterfaceNotFound, // Factory returned nullptr
	VersionMismatch // GetInterfaceVersion returned incompatible version
};

//! Server API minor version of the IScoreboardExt layout the natives call: older draft builds have another one and would crash (docs/scoreboard-protocol.md, 9.1)
constexpr int SCOREBOARD_API_MINOR = 4;

/**
 * @returns pointer to server API or nullptr
 */
bhl::IBugfixedServer *serverapi();

/**
 * Return true if API is available.
 */
bool IsServerApiReady();

/**
 * Return true if API is available and has every method of IScoreboardExt (v1.4 or newer).
 */
bool IsScoreboardApiReady();

/**
 * Initializes server API.
 * Uses functions from interface.h to get API pointer and check the version.
 *
 * If result is VersionMismatch, use GetServerApiVersion to get the server version.
 */
E_ApiInitResult InitServerApi();

/**
 * Shutdowns server API.
 */
void ShutdownServerApi();

/**
 * Sets major and minor to the oversion of the server.
 * Only works if IsServerApiReady() == true or InitServerApi() returned VersionMismatch.
 * Othervise variables are in an undefined state.
 */
void GetServerApiVersion(int &major, int &minor);

}
#endif