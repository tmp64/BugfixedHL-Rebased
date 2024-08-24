/***
*
*	Copyright (c) 2012, AGHL.RU. All rights reserved.
*
****/
//
// svc_messages.cpp
//
// Engine messages handlers
//

#include <ctime>
#include <pcre.h>
#include <tier1/strtools.h>

#include "hud.h"
#include "cl_util.h"
#include <demo_api.h>
#include "demo.h"
#include "svc_messages.h"
#include "engine_patches.h"
#include "parsemsg.h"
#include "vgui/client_viewport.h"
#include "hud/timer.h"

static_assert(sizeof(SvcClientFuncs) == sizeof(void *) * SVC_MSG_COUNT, "SvcClientFuncs size doesn't match SVC_MSG_COUNT");

extern ConVar r_dynamic_ent_light;

static CSvcMessages s_SvcMessages;
static char s_ComToken[1024];
constexpr size_t MAX_CMD_LINE = 1024;
const char *s_BlockList = "^(exit|quit|bind|unbind|unbindall|kill|exec|alias|clear|"
                          "motdfile|motd_write|writecfg|developer|fps.+|rcon.*)$";
const char *s_BlockListCvar = "^(rcon.*)$";

static ConVar cl_messages_log("cl_messages_log", "0", FCVAR_BHL_ARCHIVE);
static ConVar cl_protect_log("cl_protect_log", "1", FCVAR_BHL_ARCHIVE);
static ConVar cl_protect_block("cl_protect_block", "", FCVAR_BHL_ARCHIVE);
static ConVar cl_protect_allow("cl_protect_allow", "", FCVAR_BHL_ARCHIVE);
static ConVar cl_protect_block_cvar("cl_protect_block_cvar", "", FCVAR_BHL_ARCHIVE);

CON_COMMAND(cl_messages_dump, "Prints all registered user messages to the console.")
{
	if (CEnginePatches::Get().GetUserMsgList())
	{
		// Dump all user messages to console
		UserMessage *current = CEnginePatches::Get().GetUserMsgList();
		while (current != 0)
		{
			gEngfuncs.Con_Printf("User Message: %d, %s, %d\n", current->messageId, current->messageName, current->messageLen);
			current = current->nextMessage;
		}
	}
	else
	{
		gEngfuncs.Con_Printf("Can't dump user messages: engine wasn't hooked properly.\n");
	}
}

CON_COMMAND(cl_protect_help, "Prints info about client proctection configuration.")
{
	ConPrintf("cl_protect_* cvars are used to protect from slowhacking.\n");
	ConPrintf("By default following is blocked:\n");
	ConPrintf("Commands: %s\n", s_BlockList);
	ConPrintf("Cvar requests: %s\n", s_BlockListCvar);
}

CON_COMMAND(dev_send_status, "Sends a status command to update SteamIDs")
{
	CSvcMessages::Get().SendStatusRequest();
}

/**
 * A template function that calls a member function of CSvcMessages.
 */
template <void (CSvcMessages::*FUNC)()>
static inline void CallMember()
{
	(CSvcMessages::Get().*FUNC)();
}

/**
 * And alias to CEnginePatches::Get().GetMsgBuf().
 */
static inline const CEnginePatches::EngineMsgBuf &GetMsgBuf()
{
	return CEnginePatches::Get().GetMsgBuf();
}

CSvcMessages &CSvcMessages::Get()
{
	return s_SvcMessages;
}

CSvcMessages::CSvcMessages()
{
	memset(&m_Handlers, 0, sizeof(m_Handlers));
	memset(m_iMarkedPlayers, 0, sizeof(m_iMarkedPlayers));
}

void CSvcMessages::Init()
{
	if (!CEnginePatches::Get().GetSvcArray())
	{
		ConPrintf(ConColor::Red, "SVC: Engine svc array not found, handlers not installed.\n");
		return;
	}

	if (!CEnginePatches::Get().GetMsgBuf().IsValid())
	{
		ConPrintf(ConColor::Red, "SVC: Engine message buffer not found, handlers not installed.\n");
		return;
	}

	memset(&m_Handlers.funcs, 0, sizeof(m_Handlers.funcs));

	m_Handlers.funcs.pfnSvcPrint = CallMember<&CSvcMessages::SvcPrint>;
	m_Handlers.funcs.pfnSvcTempEntity = CallMember<&CSvcMessages::SvcTempEntity>;
	m_Handlers.funcs.pfnSvcNewUserMsg = CallMember<&CSvcMessages::SvcNewUserMsg>;
	m_Handlers.funcs.pfnSvcStuffText = CallMember<&CSvcMessages::SvcStuffText>;
	m_Handlers.funcs.pfnSvcSendCvarValue = CallMember<&CSvcMessages::SvcSendCvarValue>;
	m_Handlers.funcs.pfnSvcSendCvarValue2 = CallMember<&CSvcMessages::SvcSendCvarValue2>;

	CEnginePatches::Get().HookSvcHandlers(m_Handlers.array);
	m_bInitialized = true;
}

void CSvcMessages::VidInit()
{
	m_iStatusRequestState = StatusRequestState::Idle;
	m_iStatusResponseCounter = 0;

	// Only allow sending requests STATUS_REQUEST_CONN_DELAY after connection was established
	m_flStatusRequestLastTime = gEngfuncs.GetAbsoluteTime() + STATUS_REQUEST_CONN_DELAY - STATUS_REQUEST_PERIOD;
}

void CSvcMessages::SendStatusRequest()
{
	// Ignore when playing a demo
	if (gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	// Check if svc_print is hooked
	if (!m_bInitialized)
		return;

	// Only send or delay once per frame
	if (m_iStatusRequestLastFrame == gHUD.GetFrameCount())
		return;

	m_iStatusRequestLastFrame = gHUD.GetFrameCount();
	float absTime = gEngfuncs.GetAbsoluteTime();

	if (m_iStatusRequestState != StatusRequestState::Idle && m_flStatusRequestLastTime + STATUS_REQUEST_TIMEOUT > absTime)
	{
		// Request is in progress, delay it
		m_flStatusRequestNextTime = m_flStatusRequestLastTime + STATUS_REQUEST_PERIOD;
		return;
	}

	if (m_flStatusRequestLastTime + STATUS_REQUEST_PERIOD >= absTime)
	{
		// Delay request if it was called recently (to not spam the server)
		m_flStatusRequestNextTime = m_flStatusRequestLastTime + STATUS_REQUEST_PERIOD;
		return;
	}

	m_iStatusRequestState = StatusRequestState::Sent;

	// Send the command to the server
	ServerCmd("status");
	gEngfuncs.Con_DPrintf("%.3f status request sent (delta %.3f)\n", absTime, absTime - m_flStatusRequestLastTime);
	m_flStatusRequestLastTime = absTime;

	if (gEngfuncs.pDemoAPI->IsRecording())
	{
		// Write into the demo that a status command was sent
		uint8_t buf[sizeof(DEMO_MAGIC)];
		memcpy(buf, &DEMO_MAGIC, sizeof(DEMO_MAGIC));
		Demo_WriteBuffer(TYPE_SVC_STATUS, sizeof(buf), buf);
	}
}

void CSvcMessages::CheckDelayedSendStatusRequest()
{
	if (m_flStatusRequestNextTime > 0 && m_flStatusRequestNextTime < gEngfuncs.GetAbsoluteTime())
	{
		m_flStatusRequestNextTime = 0;
		SendStatusRequest();
	}
}

bool CSvcMessages::SanitizeCommands(char *str)
{
	bool changed = false;
	char *text = str;
	char command[MAX_CMD_LINE];
	int i, quotes;
	int len = strlen(str);

	// Split string into commands and check them separately
	while (text[0] != 0)
	{
		// Find \n or ; splitter
		quotes = 0;
		for (i = 0; i < len; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes & 1) && text[i] == ';')
				break;
			if (text[i] == '\n')
				break;
		}
		if (i >= MAX_CMD_LINE)
			i = MAX_CMD_LINE; // game engine behaviour
		strncpy(command, text, i);
		command[i] = 0;

		// Check command
		bool isGood = IsCommandGood(command);

		// Log command
		int log = cl_protect_log.GetInt();
		if (log > 0)
		{
			/*
			0  - log (1) or not (0) to console
			1  - log to common (1) or to developer (0) console
			2  - log all (1) or only bad (0) to console
			8  - log (1) or not (0) to file
			9  - log all (1) or only bad (0) to file
			15 - log full command (1) or only name (0)
			*/

			// Full command or only command name
			char *c = (log & (1 << 15)) ? command : s_ComToken;

			// Log destination
			if (log & (1 << 0)) // console
			{
				// Log only bad or all
				if (!isGood || log & (1 << 2))
				{
					// Action
					const char *a = isGood ? "Server executed command: %s\n" : "Server tried to execute bad command: %s\n";
					// Common or developer console
					void (*m)(const char *, ...) = (log & (1 << 1)) ? gEngfuncs.Con_Printf : gEngfuncs.Con_DPrintf;
					// Log
					m(a, c);
				}
			}
			if (log & (1 << 8)) // file
			{
				// Log only bad or all
				if (!isGood || log & (1 << 9))
				{
					FILE *f = fopen("svc_protect.log", "a+");
					if (f != NULL)
					{
						// The time
						time_t now;
						time(&now);
						struct tm *current = localtime(&now);
						if (current != NULL)
						{
							fprintf(f, "[%04i-%02i-%02i %02i:%02i:%02i] ",
							    current->tm_year + 1900,
							    current->tm_mon + 1,
							    current->tm_mday,
							    current->tm_hour,
							    current->tm_min,
							    current->tm_sec);
						}
						// Action
						const char *a = isGood ? "[allowed] " : "[blocked] ";
						fputs(a, f);
						// Command
						fputs(c, f);
						fputs("\n", f);
						fclose(f);
					}
				}
			}
		}

		len -= i;
		if (!isGood)
		{
			// Trash command, but leave the splitter
			strncpy(text, text + i, len);
			text[len] = 0;
			text++;
			changed = true;
		}
		else
		{
			text += i + 1;
		}
	}
	return changed;
}

void CSvcMessages::ReadDemoBuffer(int type, const uint8_t *buffer)
{
	switch (type)
	{
	case TYPE_SVC_STATUS:
	{
		// Check the magic
		// It prevents the game from interpreting messages recorded in other mods
		unsigned magic;
		memcpy(&magic, buffer, sizeof(magic));
		if (magic != DEMO_MAGIC)
		{
			gEngfuncs.Con_DPrintf("CSvcMessages::ReadDemoBuffer: Invalid magic %u\n", magic);
			return;
		}

		// Simulate SendStatusRequest
		float absTime = gEngfuncs.GetAbsoluteTime();
		m_iStatusRequestState = StatusRequestState::Sent;
		gEngfuncs.Con_DPrintf("%.3f status request sent in demo\n", absTime, absTime - m_flStatusRequestLastTime);
		m_flStatusRequestLastTime = absTime;
		return;
	}
	default:
	{
		Assert(!("Invalid type sent from Demo_ReadBuffer"));
	}
	}
}

bool CSvcMessages::RegexMatch(const char *str, const char *regex)
{
	if (regex[0] == 0)
		return false;

	const char *error;
	int erroffset;
	int ovector[30];

	// Prepare regex
	pcre *re = pcre_compile(regex, PCRE_CASELESS, &error, &erroffset, NULL);
	if (!re)
	{
		ConPrintf(ConColor::Red, "PCRE compilation failed at offset %d: %s\n", erroffset, error);
		ConPrintf(ConColor::Red, "in regex: %s\n", regex);
		return false;
	}

	// Try to match
	int rc = pcre_exec(re, NULL, str, strlen(str), 0, 0, ovector, sizeof(ovector) / sizeof(ovector[0]));

	// Free up the regular expression
	pcre_free(re);

	// Test the result
	if (rc < 0)
		return false; // No match
	return true;
}

bool CSvcMessages::IsCommandGood(const char *str)
{
	// Parse command into token
	char *ret = gEngfuncs.COM_ParseFile((char *)str, s_ComToken);
	if (ret == NULL || s_ComToken[0] == '\0')
		return true; // no tokens

	// Block our filter from hacking
	if (!Q_stricmp(s_ComToken, cl_protect_log.GetName()))
		return false;
	if (!Q_stricmp(s_ComToken, cl_protect_block.GetName()))
		return false;
	if (!Q_stricmp(s_ComToken, cl_protect_allow.GetName()))
		return false;
	if (!Q_stricmp(s_ComToken, cl_protect_block_cvar.GetName()))
		return false;

	// Check command name against block lists and whole command line against allow list
	if ((RegexMatch(s_ComToken, s_BlockList) || RegexMatch(s_ComToken, cl_protect_block.GetString())) && !RegexMatch(str, cl_protect_allow.GetString()))
		return false;

	return true;
}

bool CSvcMessages::IsCvarGood(const char *str)
{
	if (str[0] == '\0')
		return true; // no cvar

	// Block our filter from getting
	if (!Q_stricmp(s_ComToken, cl_protect_log.GetName()))
		return false;
	if (!Q_stricmp(s_ComToken, cl_protect_block.GetName()))
		return false;
	if (!Q_stricmp(s_ComToken, cl_protect_allow.GetName()))
		return false;
	if (!Q_stricmp(s_ComToken, cl_protect_block_cvar.GetName()))
		return false;

	// Check cvar name against block lists
	if (RegexMatch(str, s_BlockListCvar) || RegexMatch(str, cl_protect_block_cvar.GetString()))
		return false;

	return true;
}

void CSvcMessages::SvcPrint()
{
	static bool processingUserRow = false;

	BEGIN_READ(GetMsgBuf().GetBuf(), GetMsgBuf().GetSize(), GetMsgBuf().GetReadPos());
	char *str = READ_STRING();

	if (!strncmp(str, "\"mp_timelimit\" changed to \"", 27) || !strncmp(str, "\"amx_nextmap\" changed to \"", 26))
	{
		CHudTimer::Get()->DoResync();
	}
	else if (m_iStatusRequestState != StatusRequestState::Idle)
	{
		switch (m_iStatusRequestState)
		{
		case StatusRequestState::Sent:
		{
			// Detect answer
			if (!strncmp(str, "hostname:  ", 11))
			{
				m_iStatusRequestState = StatusRequestState::AnswerReceived;
				m_iStatusResponseCounter++;
				// Suppress status output
				GetMsgBuf().GetReadPos() += strlen(str) + 1;
				return;
			}
			else if (m_flStatusRequestLastTime + STATUS_REQUEST_TIMEOUT > gEngfuncs.GetAbsoluteTime())
			{
				// No answer
				m_iStatusRequestState = StatusRequestState::Idle;
			}
			break;
		}
		case StatusRequestState::AnswerReceived:
		{
			// Search for start of table header
			if (str[0] == '#' && str[1] != 0 && str[2] != 0 && str[3] == ' ')
			{
				m_iStatusRequestState = StatusRequestState::Processing;
			}
			if (strchr(str, '\n') == NULL)
			{
				// if header row is not finished within received string, pend it
				processingUserRow = true;
			}

			// Suppress status output
			GetMsgBuf().GetReadPos() += strlen(str) + 1;
			return;
		}
		case StatusRequestState::Processing:
		{
			if (str[0] == '#' && str[1] != 0 && str[2] != 0 && str[3] == ' ')
			{
				// start of new player info row
				int idx = atoi(str + 1); // Index in 'status' doesn't always match with player slot
				if (idx > 0)
				{
					char *name = strchr(strchr(str + 2, ' '), '"');
					if (name != NULL)
					{
						name++; // space
						char *userid = strchr(name, '"');
						if (userid != NULL)
						{
							userid += 2; // quote and space
							char *steamid = strchr(userid, ' ');
							if (steamid != NULL)
							{
								// Find actual slot
								int slot = 0;

								// Replace '\"' in the string with a null-terminator
								//   to later be used in strcmp
								char stringTerm = '\0';
								std::swap(name[userid - name - 2], stringTerm);

								for (int i = 1; i <= MAX_PLAYERS; i++)
								{
									CPlayerInfo *pi = GetPlayerInfo(i)->Update();
									if (pi->IsConnected() && !strcmp(pi->GetName(), name))
									{
										slot = i;
										break;
									}
								}

								std::swap(name[userid - name - 2], stringTerm);

								if (slot > 0)
								{
									steamid++; // space
									char *steamidend = strchr(steamid, ' ');
									if (steamidend != NULL)
										*steamidend = 0;

									CPlayerInfo *pi = GetPlayerInfo(slot);

									if (!strncmp(steamid, "STEAM_", 6) || !strncmp(steamid, "VALVE_", 6))
										strncpy(pi->m_szSteamID, steamid + 6, MAX_STEAMID); // cutout "STEAM_" or "VALVE_" start of the string
									else
										strncpy(pi->m_szSteamID, steamid, MAX_STEAMID);
									pi->m_szSteamID[MAX_STEAMID] = 0;

									m_iMarkedPlayers[slot] = m_iStatusResponseCounter;
								}
								else
								{
									ConPrintf(ConColor::Red, "[BUG] SvcPrint: Unable to find player's slot\n");
									ConPrintf(ConColor::Red, "[BUG] Status string:\n");
									ConPrintf(ConColor::Red, "[BUG] %s\n", str);
									assert(false);
								}
							}
						}
					}
				}
				if (strchr(str, '\n') == NULL) // user row is not finished within received string, pend it
					processingUserRow = true;
			}
			else if (processingUserRow)
			{
				// continuation of user or header row
				if (strchr(str, '\n') != NULL) // skip till the end of the row (new line)
					processingUserRow = false;
			}
			else
			{
				// end of the table
				m_iStatusRequestState = StatusRequestState::Idle;
				gEngfuncs.Con_DPrintf("%.3f status request received\n", gEngfuncs.GetAbsoluteTime());

				for (int idx = 1; idx <= MAX_PLAYERS; idx++)
				{
					CPlayerInfo *pi = GetPlayerInfo(idx);

					if (pi->IsConnected() && m_iMarkedPlayers[idx] != m_iStatusResponseCounter)
					{
						pi->m_iStatusPenalty++;
					}
				}
			}
			// Suppress status output
			GetMsgBuf().GetReadPos() += strlen(str) + 1;
			return;
		}
		}
	}
	else
	{
		// Clear cached steam id for left player
		int len = strlen(str);
		if (!strcmp(str + len - 9, " dropped\n"))
		{
			str[len - 9] = 0;
			for (int i = 1; i <= MAX_PLAYERS; i++)
			{
				CPlayerInfo *pi = GetPlayerInfo(i)->Update();
				if (pi->IsConnected() && !strcmp(pi->GetName(), str))
				{
					pi->m_szSteamID[0] = 0;
					break;
				}
			}
		}
	}

	CEnginePatches::Get().GetEngineSvcHandlers().pfnSvcPrint();
}

void CSvcMessages::SvcTempEntity()
{
	BEGIN_READ(GetMsgBuf().GetBuf(), GetMsgBuf().GetSize(), GetMsgBuf().GetReadPos());
	const int type = READ_BYTE();
	switch (type)
	{
	case TE_EXPLOSION:
		if (!r_dynamic_ent_light.GetBool())
		{
			*((byte *)GetMsgBuf().GetBuf() + GetMsgBuf().GetReadPos() + 1 + 6 + 2 + 2) |= TE_EXPLFLAG_NODLIGHTS;
		}
		break;
	default:
		break;
	}

	CEnginePatches::Get().GetEngineSvcHandlers().pfnSvcTempEntity();
}

void CSvcMessages::SvcNewUserMsg()
{
	BEGIN_READ(GetMsgBuf().GetBuf(), GetMsgBuf().GetSize(), GetMsgBuf().GetReadPos());

	int id = READ_BYTE();
	int len = READ_BYTE();
	char name[16];
	uint32_t value = READ_LONG();
	memcpy(name, &(value), 4);
	value = READ_LONG();
	memcpy(name + 4, &(value), 4);
	value = READ_LONG();
	memcpy(name + 8, &(value), 4);
	value = READ_LONG();
	memcpy(name + 12, &(value), 4);
	name[15] = 0;

	CEnginePatches::Get().GetEngineSvcHandlers().pfnSvcNewUserMsg();

	// Log user message to console
	if (cl_messages_log.GetBool())
		gEngfuncs.Con_Printf("User Message: %d, %s, %d\n", id, name, len == 255 ? -1 : len);

	// Fix engine bug that leads to duplicate user message ids in user messages chain
	if (CEnginePatches::Get().GetUserMsgList())
	{
		UserMessage *current = CEnginePatches::Get().GetUserMsgList();
		while (current != 0)
		{
			if (current->messageId == id && strcmp(current->messageName, name))
				current->messageId = 0;
			current = current->nextMessage;
		}
	}
}

void CSvcMessages::SvcStuffText()
{
	BEGIN_READ(GetMsgBuf().GetBuf(), GetMsgBuf().GetSize(), GetMsgBuf().GetReadPos());
	char *commands = READ_STRING();

	char str[MAX_CMD_LINE];
	V_strcpy_safe(str, commands);

	if (SanitizeCommands(str))
	{
		// Some commands were removed, put cleaned command line back to stream
		int l1 = strlen(commands);
		int l2 = strlen(str);

		Assert(l2 <= l1);

		if (l2 == 0 || l2 > l1)
		{
			// Suppress commands if they are all removed
			// Or if SanitizeCommands made the string bigger (failsafe in case of a bug)
			GetMsgBuf().GetReadPos() += l1 + 1;
			return;
		}

		// strcpy is safe since strlen(str) <= strlen(commands)
		// so use memcpy instead
		// Modified command is put to the end of the string and read position is incremented
		int diff = l1 - l2;
		memcpy(commands + diff, str, l2); // Don't copy null-terminator: already exists in the string
		GetMsgBuf().GetReadPos() += diff;
	}

	CEnginePatches::Get().GetEngineSvcHandlers().pfnSvcStuffText();
}

void CSvcMessages::SvcSendCvarValue()
{
	BEGIN_READ(GetMsgBuf().GetBuf(), GetMsgBuf().GetSize(), GetMsgBuf().GetReadPos());
	char *cvar = READ_STRING();

	// Check cvar
	bool isGood = IsCvarGood(cvar);
	if (!isGood)
	{
		gEngfuncs.Con_DPrintf("Server tried to request blocked cvar: %s\n", cvar);
		GetMsgBuf().GetReadPos() += strlen(cvar) + 1;
		// At now we will silently block the request
		return;
	}

	CEnginePatches::Get().GetEngineSvcHandlers().pfnSvcSendCvarValue();
}

void CSvcMessages::SvcSendCvarValue2()
{
	BEGIN_READ(GetMsgBuf().GetBuf(), GetMsgBuf().GetSize(), GetMsgBuf().GetReadPos());
	long l = READ_LONG();
	char *cvar = READ_STRING();

	// Check cvar
	bool isGood = IsCvarGood(cvar);
	if (!isGood)
	{
		gEngfuncs.Con_DPrintf("Server tried to request blocked cvar: %s\n", cvar);
		GetMsgBuf().GetReadPos() += 4 + strlen(cvar) + 1;
		// At now we will silently block the request
		return;
	}

	CEnginePatches::Get().GetEngineSvcHandlers().pfnSvcSendCvarValue2();
}
