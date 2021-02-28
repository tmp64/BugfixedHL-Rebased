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

/**
 * @returns pointer to server API or nullptr
 */
bhl::IBugfixedServer *serverapi();

/**
 * Return true if API is available.
 */
bool IsServerApiReady();

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