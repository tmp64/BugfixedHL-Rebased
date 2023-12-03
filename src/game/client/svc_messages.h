/***
*
*	Copyright (c) 2012, AGHL.RU. All rights reserved.
*
****/
//
// svc_messages.h
//
// Engine messages handlers
//

#ifndef SVC_MESSAGES_H
#define SVC_MESSAGES_H
#include <cstddef>

using SvcParseFunc = void (*)();

struct SvcHandler
{
	unsigned char nOpcode;
	const char *pszName;
	SvcParseFunc pfnParse;
};

struct UserMessage
{
	int messageId;
	int messageLen;
	char messageName[16];
	UserMessage *nextMessage;
};

struct SvcClientFuncs
{
	void (*pfnSvcBad)(void);
	void (*pfnSvcNop)(void);
	void (*pfnSvcDisconnect)(void);
	void (*pfnSvcEvent)(void);
	void (*pfnSvcVersion)(void);
	void (*pfnSvcSetView)(void);
	void (*pfnSvcSound)(void);
	void (*pfnSvcTime)(void);
	void (*pfnSvcPrint)(void);
	void (*pfnSvcStuffText)(void);
	void (*pfnSvcSetAngle)(void);
	void (*pfnSvcServerInfo)(void);
	void (*pfnSvcLightStyle)(void);
	void (*pfnSvcUpdateUserInfo)(void);
	void (*pfnSvcDeltaDescription)(void);
	void (*pfnSvcClientData)(void);
	void (*pfnSvcStopSound)(void);
	void (*pfnSvcPings)(void);
	void (*pfnSvcParticle)(void);
	void (*pfnSvcDamage)(void);
	void (*pfnSvcSpawnStatic)(void);
	void (*pfnSvcEventReliable)(void);
	void (*pfnSvcSpawnBaseline)(void);
	void (*pfnSvcTempEntity)(void);
	void (*pfnSvcSetPause)(void);
	void (*pfnSvcSignonNum)(void);
	void (*pfnSvcCenterPrint)(void);
	void (*pfnSvcKilledMonster)(void);
	void (*pfnSvcFoundSecret)(void);
	void (*pfnSvcSpawnStaticSound)(void);
	void (*pfnSvcIntermission)(void);
	void (*pfnSvcFinale)(void);
	void (*pfnSvcCdTrack)(void);
	void (*pfnSvcRestore)(void);
	void (*pfnSvcCutscene)(void);
	void (*pfnSvcWeaponAnim)(void);
	void (*pfnSvcDecalName)(void);
	void (*pfnSvcRoomType)(void);
	void (*pfnSvcAddAngle)(void);
	void (*pfnSvcNewUserMsg)(void);
	void (*pfnSvcPacketEntites)(void);
	void (*pfnSvcDeltaPacketEntites)(void);
	void (*pfnSvcChoke)(void);
	void (*pfnSvcResourceList)(void);
	void (*pfnSvcNewMoveVars)(void);
	void (*pfnSvcResourceRequest)(void);
	void (*pfnSvcCustomization)(void);
	void (*pfnSvcCrosshairAngle)(void);
	void (*pfnSvcSoundFade)(void);
	void (*pfnSvcFileTxferFailed)(void);
	void (*pfnSvcHltv)(void);
	void (*pfnSvcDirector)(void);
	void (*pfnSvcVoiceInit)(void);
	void (*pfnSvcVoiceData)(void);
	void (*pfnSvcSendExtraInfo)(void);
	void (*pfnSvcTimeScale)(void);
	void (*pfnSvcResourceLocation)(void);
	void (*pfnSvcSendCvarValue)(void);
	void (*pfnSvcSendCvarValue2)(void);
};

constexpr size_t SVC_MSG_COUNT = 59;

class CSvcMessages
{
public:
	/**
	 * Returns singleton instance.
	 */
	static CSvcMessages &Get();

	CSvcMessages();
	CSvcMessages(const CSvcMessages &) = delete;
	CSvcMessages &operator=(const CSvcMessages &) = delete;

	/**
	 * Installs SVC message hooks.
	 */
	void Init();

	/**
	 * Call on every level start.
	 */
	void VidInit();

	/**
	 * Sends 'status' command to the server to update SteamIDs.
	 */
	void SendStatusRequest();

	/**
	 * Sends 'status' command if it was delayed by previous call to SendStatusRequest.
	 */
	void CheckDelayedSendStatusRequest();

	/**
	 * Removes unsafe commands from specified buffer.
	 * @return	True if string was modified.
	 */
	bool SanitizeCommands(char *str);

	/**
	 * Reads a message from the demo.
	 */
	void ReadDemoBuffer(int type, const uint8_t *buffer);

private:
	enum class StatusRequestState
	{
		Idle = 0,
		Sent,
		AnswerReceived,
		Processing,
	};

	union
	{
		SvcClientFuncs funcs;
		SvcParseFunc array[SVC_MSG_COUNT];
	} m_Handlers;

	static constexpr float STATUS_REQUEST_TIMEOUT = 1.0f; //<! Maximum waiting time for response
	static constexpr float STATUS_REQUEST_PERIOD = 2.0f; //<! Minimum time between status requests
	static constexpr float STATUS_REQUEST_CONN_DELAY = 3.f; //<! Only begin sending requests some time after connection established

	static constexpr unsigned DEMO_MAGIC = 2498416793; //!< A random magic number

	bool m_bInitialized = false;

	StatusRequestState m_iStatusRequestState = StatusRequestState::Idle;
	float m_flStatusRequestLastTime = 0.0f;
	float m_flStatusRequestNextTime = 0.0f;
	int m_iStatusRequestLastFrame = 0;
	int m_iStatusResponseCounter = 0; //<! Incremented at the start of each response
	int m_iMarkedPlayers[MAX_PLAYERS + 1]; //<! [i] is set to counter if i-th player was found in the response

	/**
	 * Returns whether string matches a regular expression.
	 */
	bool RegexMatch(const char *str, const char *regex);

	/**
	 * Returns whether a command is safe to execute from the server.
	 */
	bool IsCommandGood(const char *str);

	/**
	 * Returns whether cvar can be queried by the server.
	 */
	bool IsCvarGood(const char *str);

	/**
	 * svc_print: Prints text to the console.
	 * Message contents:
	 *   string: Text to print to the console.
	 */
	void SvcPrint();

	/**
	 * svc_temp_entity: Create a tempentity on the client.
	 * Message contents:
	 *   byte: Type
	 */
	void SvcTempEntity();

	/**
	 * svc_newusermsg: Register new UserMessage.
	 * Message contents:
	 *   byte: id
	 *   byte: len
	 *   long * 4: name 
	 */
	void SvcNewUserMsg();

	/**
	 * svc_stufftext: Execute a console command.
	 * Message contents:
	 *   string: The command
	 */
	void SvcStuffText();

	/**
	 * svc_sendcvarvalue: Send a cvar value to the server.
	 * Message contents:
	 *   string: Cvar name
	 */
	void SvcSendCvarValue();

	/**
	 * svc_sendcvarvalue2: Send a cvar value to the server.
	 * Message contents:
	 *   string: Cvar name
	 *   long: Request ID
	 */
	void SvcSendCvarValue2();
};

#endif SVC_MESSAGES_H
