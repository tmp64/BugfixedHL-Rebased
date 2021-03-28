#ifndef IBUGFIXEDAPI_H
#define IBUGFIXEDAPI_H
#include <vinterface/interface.h>
#include <ClientSupportsFlags.h>
#include <enum_utils.h>

#define IBUGFIXEDSERVER_NAME  "IBugfixedServer"
#define IBUGFIXEDSERVER_MAJOR 1
#define IBUGFIXEDSERVER_MINOR 0

class CGameRules;
class IGameVersion;

namespace bhl
{
enum class E_MotdType : unsigned int
{
	None = 0,
	Plain = 1 << 0,
	Unicode = 1 << 1,
	Html = 1 << 2,
	All = Plain | Unicode | Html
};

class IBugfixedServer : public IBaseInterface
{
public:
	/**
	 * @brief Sets major and minor to the version of implemented interface.
	 *
	 * Major is incremented every time API-breaking changes occur and MUST equal to IBUGFIXEDSERVER_MAJOR.
	 * Minor is incremented every time a new feature is added that doesn't break compatibility.
	 * It should be >= IBUGFIXEDSERVER_MINOR.
	 *
	 * If major is zero, minor version should match exactly.
	 *
	 * This method must be the first one in this class.
	 * New methods must be added to the end and should increment minor each time new update with them is released.
	 * Any changes to existing methods may/will break compatibility.
	 * 
	 * Added in version v1.0
	 */
	virtual void GetInterfaceVersion(int &major, int &minor) = 0;

	/**
	 * Returns a pointer to g_pGameRules that contains a pointer to CGameRules.
	 * 
	 * Added in version v1.0
	 */
	virtual CGameRules **GetGameRulesPtr() = 0;

	/**
	 * Returns server DLL version.
	 *
	 * This pointer is only valid immediately after the call.
	 * Make a copy if you need to use it later.
	 * 
	 * Added in version v1.0
	 */
	virtual const IGameVersion *GetServerVersion() = 0;

	/**
	 * Returns bitfield with BugfixedHL features the client's game supports.
	 * Use bitwise AND to check:
	 * if (server()->GetClientSupports(idx) & AGHL_SUPPORTS_UNICODE_MOTD) { ... }
	 * 
	 * Added in version v1.0
	 */
	virtual bhl::E_ClientSupports GetClientSupports(int idx) = 0;

	/**
	 * Returns true if client has color code parsing (^1, ^2, ...) enabled (BugfixedHL or Rofi's client DLL).
	 * 
	 * Added in version v1.0
	 */
	virtual bool GetColorSupport(int idx) = 0;

	/**
	 * Returns true if server knows client's game DLL version.
	 * 
	 * Added in version v1.0
	 */
	virtual bool IsClientVersionValid(int idx) = 0;

	/**
	 * If IsClientVersionValid(idx) == true then returns pointer to the version.
	 * Otherwise returns nullptr.
	 *
	 * This pointer is only valid immediately after the call.
	 * Make a copy if you need to use it later.
	 * 
	 * Added in version v1.0
	 */
	virtual const IGameVersion *GetClientVersion(int idx) = 0;

	/**
	 * @see SetAutomaticMotd
	 */
	virtual bool GetAutomaticMotd(bhl::E_MotdType type) = 0;

	/**
	 * Enables or disables automatic send of MOTD to connecting clients.
	 *
	 * The server checks support for HTML MOTD, then Unicode MOTD.
	 * If HTML MOTD is supported, the server will read it from file int `motdfile_html`.
	 * If not, if Unicode MOTD is supported, the server will read it from file in `motdfile_unicode`.
	 * Otherwise, file `motdfile` is sent.
	 *
	 * If a MOTD file is supported but it is disabled with SetAutomaticMotd, no MOTD is sent.
	 * 
	 * Added in version v1.0
	 *
	 * @see CHalfLifeMultiplay::InitHUD for details
	 */
	virtual void SetAutomaticMotd(bhl::E_MotdType type, bool state) = 0;

	/**
	 * Sends a MOTD to the client from a string
	 * 
	 * Added in version v1.0
	 * 
	 * @param type MOTD type
	 * @param idx Client index [1; (maxplayers)]
	 * @param str The string
	 */
	virtual void ShowMotdFromString(bhl::E_MotdType type, int idx, const char *str) = 0;

	/**
	 * Sends a MOTD to the client from a file
	 * 
	 * Added in version v1.0
	 * 
	 * @param type MOTD type
	 * @param idx Client index [1; (maxplayers)]
	 * @param file Path to the file relative from gamedir
	 */
	virtual void ShowMotdFromFile(bhl::E_MotdType type, int idx, const char *file) = 0;

	/**
	 * Sets player's score and sends score update message to all players.
	 * 
	 * Added in version v1.1
	 * 
	 * @param	idx		Client index [1; (maxplayers)]
	 * @param	frags	Number of kills (score)
	 * @param	deaths	Number of deaths
	 */
	virtual void SetPlayerScore(int idx, int frags, int deaths) = 0;
};
}
#endif
