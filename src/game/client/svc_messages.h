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

	CSvcMessages() = default;
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
	 * Sends 'status' commend to the server to update SteamIDs.
	 */
	void SendStatusRequest();

	/**
	 * Removes unsafe commands from specified buffer.
	 * @return	True if string was modified.
	 */
	bool SanitizeCommands(char *str);

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

	StatusRequestState m_iStatusRequestState = StatusRequestState::Idle;
	float m_flStatusRequestLastTime;
	float m_flStatusRequestNextTime;

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
