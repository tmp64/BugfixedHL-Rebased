/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"
#include "spectator.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "customentity.h"
#include "weapons.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "path.h"
#include <ctype.h>
#include "CBugfixedServer.h"

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL g_fGameOver;
extern DLL_GLOBAL int g_iSkillLevel;
extern DLL_GLOBAL ULONG g_ulFrameCount;

extern int giPrecacheGrunt;
extern int gmsgSayText;

extern int g_teamplay;

void LinkUserMessages(void);

char g_checkedPlayerModels[MAX_PLAYERS][MAX_TEAM_NAME]; // Used to store checked player model name

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t *pev)
{
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

	//	pev->frame		= $deatha11;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->deadflag = DEAD_DEAD;
	pev->nextthink = -1;
}

/*
===========
ClientConnect

called when a player connects to a server
============
*/
BOOL ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
{
	bool ret = g_pGameRules->ClientConnected(pEntity, pszName, pszAddress, szRejectReason);
	serverapi()->ClientConnect(pEntity);
	return ret;

	// a client connecting during an intermission can cause problems
	//	if (intermission_running)
	//		ExitIntermission ();
}
/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
void ClientDisconnect(edict_t *pEntity)
{
	if (g_fGameOver)
		return;

	CSound *pSound;
	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(pEntity));
	{
		// since this client isn't around to think anymore, reset their sound.
		if (pSound)
		{
			pSound->Reset();
		}
	}

	// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO; // don't attract autoaim
	pEntity->v.solid = SOLID_NOT; // nonsolid
	pEntity->v.flags = 0; // clear client flags, because engine doesn't clear them before calling ClientConnect, but only before ClientPutInServer, on next connection to this slot
	UTIL_SetOrigin(&pEntity->v, pEntity->v.origin);

	g_pGameRules->ClientDisconnected(pEntity);

	// Mark player as disconnected
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pl = (CBasePlayer *)CBasePlayer::Instance(pev);
	if (pl)
		pl->Disconnect();
	g_checkedPlayerModels[pl->entindex() - 1][0] = 0;
}

// Checks player model for validity
void CheckPlayerModel(CBasePlayer *pPlayer, char *infobuffer)
{
	char text[256];
	int clientIndex = pPlayer->entindex();
	char *prevModel = g_checkedPlayerModels[clientIndex - 1];

	// Detect model change
	char *mdls = g_engfuncs.pfnInfoKeyValue(infobuffer, "model");
	if (prevModel[0] == 0 || _stricmp(mdls, prevModel))
	{
		// First parse the model and remove any %'s
		bool changed = false;
		for (char *c = mdls; *c != 0; c++)
		{
			if (*c != '%')
				continue;
			// Replace it with a space
			*c = ' ';
			changed = true;
		}

		// Check for incorrect player model
		if (mdls[0] == 0 || strlen(mdls) > MAX_TEAM_NAME - 1 || !IsValidFilename(mdls))
		{
			if (prevModel[0] == 0)
				UTIL_strncpy(prevModel, "gordon", MAX_TEAM_NAME); // default model if empty

			// Set previous model back into info buffer
			g_engfuncs.pfnSetClientKeyValue(clientIndex, infobuffer, "model", prevModel);

			// Inform player
			sprintf(text, "* Model should be non-empty, less then %d characters and can't contain special characters like: <>:\"/\\|?*\n* Your current model remains: \"%s\"\n", MAX_TEAM_NAME - 1, prevModel);
			UTIL_SayText(text, pPlayer);

			return;
		}

		// Set changed model back into info buffer
		if (changed)
			g_engfuncs.pfnSetClientKeyValue(clientIndex, infobuffer, "model", mdls);

		// Remember model player has set
		UTIL_strncpy(prevModel, mdls, MAX_TEAM_NAME);
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill(edict_t *pEntity)
{
	entvars_t *pev = &pEntity->v;

	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;
	// PrivateData is never deleted after it was created on first PutInServer so we will check for IsConnected flag too
	CBasePlayer *pPlayer = (CBasePlayer *)CBasePlayer::Instance(pev);
	if (!pPlayer || !pPlayer->IsConnected())
		return;

	// prevent suiciding too often
	if (pPlayer->m_fNextSuicideTime > gpGlobals->time)
		return;
	pPlayer->m_fNextSuicideTime = gpGlobals->time + 1;

	// prevent death in spectator mode
	if (pev->iuser1 != OBS_NONE)
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Can't suicide while in spectator mode!\n"));
		return;
	}

	// prevent death if already dead
	if (pev->deadflag != DEAD_NO)
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Can't suicide -- already dead!\n"));
		return;
	}

	// prevent death in welcome cam
	if (pPlayer->m_bInWelcomeCam)
	{
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Can't suicide while in welcome cam mode!\n"));
		return;
	}

	// have the player kill themself
	pev->health = 0;
	pPlayer->Killed(pev, GIB_NEVER);
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer(edict_t *pEntity)
{
	CBasePlayer *pPlayer;

	entvars_t *pev = &pEntity->v;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer = GetClassPtr((CBasePlayer *)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// Check player model before spawn
	CheckPlayerModel(pPlayer, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()));

	// Check if bot
	const char *auth;
	if ((pPlayer->pev->flags & FL_FAKECLIENT) == FL_FAKECLIENT || (auth = GETPLAYERAUTHID(pPlayer->edict())) && strcmp(auth, "BOT") == 0)
	{
		pPlayer->m_bIsBot = true;
	}

	pPlayer->Spawn();

	// Setup some fields initially
	pPlayer->m_fNextSuicideTime = 0;
	pPlayer->m_iAutoWepSwitch = 1;

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;

	// Mark as PutInServer
	pPlayer->m_bPutInServer = TRUE;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say(edict_t *pEntity, int teamonly)
{
	CBasePlayer *client;
	int j;
	char *p;
	char *pc;
	char text[128];
	char szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = CMD_ARGV(0);
	const int cmdc = CMD_ARGC();

	if (cmdc == 0)
		return;

	entvars_t *pev = &pEntity->v;
	CBasePlayer *player = GetClassPtr((CBasePlayer *)pev);

	// Flood check
	if (player->m_flNextChatTime > gpGlobals->time)
	{
		if (player->m_iChatFlood >= CHAT_FLOOD)
		{
			player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL + CHAT_PENALTY;
			return;
		}
		player->m_iChatFlood++;
	}
	else if (player->m_iChatFlood)
	{
		player->m_iChatFlood--;
	}
	player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;

	// We can get a raw string now, without the "say " prepended
	if (!_stricmp(pcmd, cpSay) || !_stricmp(pcmd, cpSayTeam))
	{
		if (cmdc > 1)
		{
			_snprintf(szTemp, sizeof(szTemp), "%s", (char *)CMD_ARGS());
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else // Raw text, need to prepend argv[0]
	{
		if (cmdc > 1)
		{
			_snprintf(szTemp, sizeof(szTemp), "%s %s", (char *)pcmd, (char *)CMD_ARGS());
		}
		else
		{
			// Just a one word command, use the first word...sigh
			_snprintf(szTemp, sizeof(szTemp), "%s", (char *)pcmd);
		}
	}
	p = szTemp;

	// remove empty spaces at the end
	for (pc = p + (int)strlen(p) - 1; pc != p; pc--)
	{
		if ((byte)(*pc) >= (byte)0x80 || // UTF-8 symbol
		    !isspace(*pc))
		{
			break;
		}
		*pc = 0;
	}

	// remove surrounding quotes if present
	int len = (int)strlen(p);
	if (*p == '"' && p[len - 1] == '"')
	{
		p[len - 1] = 0;
		p++;
	}

	// remove empty spaces at the end
	for (pc = p + (int)strlen(p) - 1; pc != p; pc--)
	{
		if ((byte)(*pc) >= (byte)0x80 || // UTF-8 symbol
		    !isspace(*pc))
		{
			break;
		}
		*pc = 0;
	}

	// make sure the text has content
	for (pc = p; pc != NULL && *pc != 0; pc++)
	{
		if ((byte)(*pc) >= (byte)0x80 || // UTF-8 symbol
		    isprint(*pc) && !isspace(*pc))
		{
			pc = NULL; // we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if (pc != NULL)
		return; // no character found, so say nothing

	// turn on color set 2  (color on,  no sound)
	if (teamonly)
		_snprintf(text, sizeof(text), "%c(TEAM) %s: ", 2, STRING(pEntity->v.netname));
	else
		_snprintf(text, sizeof(text), "%c%s: ", 2, STRING(pEntity->v.netname));

	j = sizeof(text) - 2 - strlen(text); // -2 for \n and null terminator
	if ((int)strlen(p) > j)
		p[j] = 0;

	strcat(text, p);
	strcat(text, "\n");

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while (((client = (CBasePlayer *)UTIL_FindEntityByClassname(client, "player")) != NULL) && (!FNullEnt(client->edict())))
	{
		if (!client->pev)
			continue;

		if (client->edict() == pEntity)
			continue;

		if (!(client->IsNetClient())) // Not a client ? (should never be true)
			continue;

		// can the receiver hear the sender? or has he muted him?
		if (g_VoiceGameMgr.PlayerHasBlockedPlayer(client, player))
			continue;

		if (teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE)
			continue;

		MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, client->pev);
		WRITE_BYTE(ENTINDEX(pEntity));
		WRITE_STRING(text);
		MESSAGE_END();
	}

	// print to the sending client
	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, &pEntity->v);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint(text);

	char *temp;
	if (teamonly)
		temp = "say_team";
	else
		temp = "say";

	// team match?
	if (g_teamplay)
	{
		UTIL_LogPrintf("\"%s<%i><%s><%s>\" %s \"%s\"\n",
		    STRING(pEntity->v.netname),
		    GETPLAYERUSERID(pEntity),
		    GETPLAYERAUTHID(pEntity),
		    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pEntity), "model"),
		    temp,
		    p);
	}
	else
	{
		UTIL_LogPrintf("\"%s<%i><%s><%i>\" %s \"%s\"\n",
		    STRING(pEntity->v.netname),
		    GETPLAYERUSERID(pEntity),
		    GETPLAYERAUTHID(pEntity),
		    GETPLAYERUSERID(pEntity),
		    temp,
		    p);
	}
}

/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
extern float g_flWeaponCheat;

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand(edict_t *pEntity)
{
	const char *pcmd = CMD_ARGV(0);
	const char *pstr;

	entvars_t *pev = &pEntity->v;

	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;
	// PrivateData is never deleted after it was created on first PutInServer so we will check for IsConnected flag too
	CBasePlayer *pPlayer = (CBasePlayer *)CBasePlayer::Instance(pev);
	if (!pPlayer || !pPlayer->IsConnected())
		return;

	if (FStrEq(pcmd, "say"))
	{
		Host_Say(pEntity, 0);
	}
	else if (FStrEq(pcmd, "say_team"))
	{
		Host_Say(pEntity, 1);
	}
	else if (FStrEq(pcmd, "fullupdate"))
	{
		if (pPlayer->m_flNextFullupdate[0] < gpGlobals->time)
		{
			pPlayer->ForceClientDllUpdate();
		}
		pPlayer->m_flNextFullupdate[0] = pPlayer->m_flNextFullupdate[1];
		pPlayer->m_flNextFullupdate[1] = gpGlobals->time + FULLUPDATE_INTERVAL;
	}
	else if (FStrEq(pcmd, "give"))
	{
		if (g_flWeaponCheat != 0.0)
		{
			int iszItem = ALLOC_STRING(CMD_ARGV(1)); // Make a copy of the classname
			pPlayer->GiveNamedItem(STRING(iszItem));
		}
	}
	else if (FStrEq(pcmd, "drop"))
	{
		// player is dropping an item.
		pPlayer->DropPlayerItem((char *)CMD_ARGV(1));
	}
	else if (FStrEq(pcmd, "fov"))
	{
		if (g_flWeaponCheat && CMD_ARGC() > 1)
		{
			pPlayer->m_iFOV = atoi(CMD_ARGV(1));
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("\"fov\" is \"%d\"\n", (int)pPlayer->m_iFOV));
		}
	}
	else if (FStrEq(pcmd, "use"))
	{
		pPlayer->SelectItem((char *)CMD_ARGV(1));
	}
	else if (((pstr = strstr(pcmd, "weapon_")) != NULL) && (pstr == pcmd))
	{
		pPlayer->SelectItem(pcmd);
	}
	else if (FStrEq(pcmd, "lastinv"))
	{
		pPlayer->SelectLastItem();
	}
	else if (FStrEq(pcmd, "spectate"))
	{
		// Block too offten spectator command usage
		if (pPlayer->m_flNextSpectatorCommand < gpGlobals->time)
		{
			pPlayer->m_flNextSpectatorCommand = gpGlobals->time + (spectator_cmd_delay.value < 1.0 ? 1.0 : spectator_cmd_delay.value);
			if (!pPlayer->IsObserver())
			{
				if ((pev->flags & FL_PROXY) || allow_spectators.value != 0.0)
				{
					pPlayer->StartObserver();

					if (((int)mp_notify_player_status.value & 4) == 4)
					{
						// notify other clients of player switched to spectators
						UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s switched to spectator mode\n", (pPlayer->pev->netname && STRING(pPlayer->pev->netname)[0] != 0) ? STRING(pPlayer->pev->netname) : "unconnected"));
					}

					// team match?
					if (g_teamplay)
					{
						UTIL_LogPrintf("\"%s<%i><%s><%s>\" switched to spectator mode\n",
						    STRING(pPlayer->pev->netname),
						    GETPLAYERUSERID(pPlayer->edict()),
						    GETPLAYERAUTHID(pPlayer->edict()),
						    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model"));
					}
					else
					{
						UTIL_LogPrintf("\"%s<%i><%s><%i>\" switched to spectator mode\n",
						    STRING(pPlayer->pev->netname),
						    GETPLAYERUSERID(pPlayer->edict()),
						    GETPLAYERAUTHID(pPlayer->edict()),
						    GETPLAYERUSERID(pPlayer->edict()));
					}
				}
				else
				{
					ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Spectator mode is disabled.\n"));
				}
			}
			else
			{
				pPlayer->StopObserver();

				if (((int)mp_notify_player_status.value & 4) == 4)
				{
					// notify other clients of player left spectators
					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s has left spectator mode\n", (pPlayer->pev->netname && STRING(pPlayer->pev->netname)[0] != 0) ? STRING(pPlayer->pev->netname) : "unconnected"));
				}

				// team match?
				if (g_teamplay)
				{
					UTIL_LogPrintf("\"%s<%i><%s><%s>\" has left spectator mode\n",
					    STRING(pPlayer->pev->netname),
					    GETPLAYERUSERID(pPlayer->edict()),
					    GETPLAYERAUTHID(pPlayer->edict()),
					    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model"));
				}
				else
				{
					UTIL_LogPrintf("\"%s<%i><%s><%i>\" has left spectator mode\n",
					    STRING(pPlayer->pev->netname),
					    GETPLAYERUSERID(pPlayer->edict()),
					    GETPLAYERAUTHID(pPlayer->edict()),
					    GETPLAYERUSERID(pPlayer->edict()));
				}
			}
		}
	}
	else if (FStrEq(pcmd, "specmode"))
	{
		if (pPlayer->IsObserver())
		{
			pPlayer->Observer_SetMode(atoi(CMD_ARGV(1)));
		}
	}
	else if (FStrEq(pcmd, "follownext"))
	{
		// No switching of view point in Free Overview
		if (pev->iuser1 == OBS_MAP_FREE)
			return;
		if (pPlayer->IsObserver())
		{
			if (pev->iuser1 == OBS_ROAMING)
				pPlayer->Observer_FindNextSpot(atoi(CMD_ARGV(1)) != 0);
			else
				pPlayer->Observer_FindNextPlayer(atoi(CMD_ARGV(1)) != 0, false);
		}
	}
	else if (g_pGameRules->ClientCommand(pPlayer, pcmd))
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy(command, pcmd, 127);
		command[127] = '\0';

		// First parse the name and remove any %'s
		for (char *pApersand = command; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// tell the user they entered an unknown command
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Unknown command: %s\n", command));
	}
}

/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged(edict_t *pEntity, char *infobuffer)
{
	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;
	// PrivateData is never deleted after it was created on first PutInServer so we will check for IsConnected flag too
	CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)&pEntity->v);
	if (!pPlayer->IsConnected())
		return;

	CheckPlayerModel(pPlayer, infobuffer);

	char text[256];

	// msg everyone if someone changes their name, and it isn't the first time (changing no name to current name)
	if (pEntity->v.netname && STRING(pEntity->v.netname)[0] != 0 && !FStrEq(STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name")))
	{
		char sName[256];
		char *pName = g_engfuncs.pfnInfoKeyValue(infobuffer, "name");
		strncpy(sName, pName, sizeof(sName) - 1);
		sName[sizeof(sName) - 1] = 0;

		// First parse the name and remove any %'s
		for (char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pEntity), infobuffer, "name", sName);

		_snprintf(text, sizeof(text), "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		UTIL_SayTextAll(text, pPlayer);

		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" changed name to \"%s\"\n",
			    STRING(pEntity->v.netname),
			    GETPLAYERUSERID(pEntity),
			    GETPLAYERAUTHID(pEntity),
			    g_engfuncs.pfnInfoKeyValue(infobuffer, "model"),
			    g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" changed name to \"%s\"\n",
			    STRING(pEntity->v.netname),
			    GETPLAYERUSERID(pEntity),
			    GETPLAYERAUTHID(pEntity),
			    GETPLAYERUSERID(pEntity),
			    g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		}
	}

	pPlayer->SetPrefsFromUserinfo(infobuffer);

	g_pGameRules->ClientUserInfoChanged(pPlayer, infobuffer);
}

static int g_serveractive = 0;

void ServerDeactivate(void)
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate
	if (g_serveractive != 1)
	{
		return;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	//
}

void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	int i;
	CBaseEntity *pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	ASSERT(g_serveractive == 0);
	g_serveractive = 1;

	// Clients have not been initialized yet
	for (i = 0; i < edictCount; i++)
	{
		if (pEdictList[i].free)
			continue;

		// Clients aren't necessarily initialized until ClientPutInServer()
		if (i < clientMax || !pEdictList[i].pvPrivateData)
			continue;

		pClass = CBaseEntity::Instance(&pEdictList[i]);
		// Activate this entity if it's got a class & isn't dormant
		if (pClass && !(pClass->pev->flags & FL_DORMANT))
		{
			pClass->Activate();
		}
		else
		{
			ALERT(at_console, "Can't instance %s\n", STRING(pEdictList[i].v.classname));
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
}

/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink(edict_t *pEntity)
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PreThink();
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink(edict_t *pEntity)
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink();

	serverapi()->PlayerPostThink(pEntity);
}

void ParmsNewLevel(void)
{
}

void ParmsChangeLevel(void)
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if (pSaveData)
		pSaveData->connectionCount = BuildChangeList(pSaveData->levelList, MAX_LEVEL_CONNECTIONS);
}

//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame(void)
{
	if (g_pGameRules)
		g_pGameRules->Think();

	if (g_fGameOver)
		return;

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;
}

void ClientPrecache(void)
{
	// setup precaches always needed
	PRECACHE_SOUND("player/sprayer.wav"); // spray paint sound for PreAlpha

	// PRECACHE_SOUND("player/pl_jumpland2.wav");		// UNDONE: play 2x step sound

	PRECACHE_SOUND("player/pl_fallpain2.wav");
	PRECACHE_SOUND("player/pl_fallpain3.wav");

	PRECACHE_SOUND("player/pl_step1.wav"); // walk on concrete
	PRECACHE_SOUND("player/pl_step2.wav");
	PRECACHE_SOUND("player/pl_step3.wav");
	PRECACHE_SOUND("player/pl_step4.wav");

	PRECACHE_SOUND("common/npc_step1.wav"); // NPC walk on concrete
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	PRECACHE_SOUND("common/npc_step4.wav");

	PRECACHE_SOUND("player/pl_metal1.wav"); // walk on metal
	PRECACHE_SOUND("player/pl_metal2.wav");
	PRECACHE_SOUND("player/pl_metal3.wav");
	PRECACHE_SOUND("player/pl_metal4.wav");

	PRECACHE_SOUND("player/pl_dirt1.wav"); // walk on dirt
	PRECACHE_SOUND("player/pl_dirt2.wav");
	PRECACHE_SOUND("player/pl_dirt3.wav");
	PRECACHE_SOUND("player/pl_dirt4.wav");

	PRECACHE_SOUND("player/pl_duct1.wav"); // walk in duct
	PRECACHE_SOUND("player/pl_duct2.wav");
	PRECACHE_SOUND("player/pl_duct3.wav");
	PRECACHE_SOUND("player/pl_duct4.wav");

	PRECACHE_SOUND("player/pl_grate1.wav"); // walk on grate
	PRECACHE_SOUND("player/pl_grate2.wav");
	PRECACHE_SOUND("player/pl_grate3.wav");
	PRECACHE_SOUND("player/pl_grate4.wav");

	PRECACHE_SOUND("player/pl_slosh1.wav"); // walk in shallow water
	PRECACHE_SOUND("player/pl_slosh2.wav");
	PRECACHE_SOUND("player/pl_slosh3.wav");
	PRECACHE_SOUND("player/pl_slosh4.wav");

	PRECACHE_SOUND("player/pl_tile1.wav"); // walk on tile
	PRECACHE_SOUND("player/pl_tile2.wav");
	PRECACHE_SOUND("player/pl_tile3.wav");
	PRECACHE_SOUND("player/pl_tile4.wav");
	PRECACHE_SOUND("player/pl_tile5.wav");

	PRECACHE_SOUND("player/pl_swim1.wav"); // breathe bubbles
	PRECACHE_SOUND("player/pl_swim2.wav");
	PRECACHE_SOUND("player/pl_swim3.wav");
	PRECACHE_SOUND("player/pl_swim4.wav");

	PRECACHE_SOUND("player/pl_ladder1.wav"); // climb ladder rung
	PRECACHE_SOUND("player/pl_ladder2.wav");
	PRECACHE_SOUND("player/pl_ladder3.wav");
	PRECACHE_SOUND("player/pl_ladder4.wav");

	PRECACHE_SOUND("player/pl_wade1.wav"); // wade in water
	PRECACHE_SOUND("player/pl_wade2.wav");
	PRECACHE_SOUND("player/pl_wade3.wav");
	PRECACHE_SOUND("player/pl_wade4.wav");

	PRECACHE_SOUND("debris/wood1.wav"); // hit wood texture
	PRECACHE_SOUND("debris/wood2.wav");
	PRECACHE_SOUND("debris/wood3.wav");

	PRECACHE_SOUND("plats/train_use1.wav"); // use a train
	PRECACHE_SOUND("plats/vehicle_ignition.wav");

	PRECACHE_SOUND("buttons/spark5.wav"); // hit computer texture
	PRECACHE_SOUND("buttons/spark6.wav");
	PRECACHE_SOUND("debris/glass1.wav");
	PRECACHE_SOUND("debris/glass2.wav");
	PRECACHE_SOUND("debris/glass3.wav");

	PRECACHE_SOUND(SOUND_FLASHLIGHT_ON);
	PRECACHE_SOUND(SOUND_FLASHLIGHT_OFF);

	// player gib sounds
	PRECACHE_SOUND("common/bodysplat.wav");

	// player pain sounds
	PRECACHE_SOUND("player/pl_pain2.wav");
	PRECACHE_SOUND("player/pl_pain4.wav");
	PRECACHE_SOUND("player/pl_pain5.wav");
	PRECACHE_SOUND("player/pl_pain6.wav");
	PRECACHE_SOUND("player/pl_pain7.wav");

	PRECACHE_MODEL("models/player.mdl");

	// hud sounds

	PRECACHE_SOUND("common/wpn_hudoff.wav");
	PRECACHE_SOUND("common/wpn_hudon.wav");
	PRECACHE_SOUND("common/wpn_moveselect.wav");
	PRECACHE_SOUND("common/wpn_select.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");

	// geiger sounds

	PRECACHE_SOUND("player/geiger6.wav");
	PRECACHE_SOUND("player/geiger5.wav");
	PRECACHE_SOUND("player/geiger4.wav");
	PRECACHE_SOUND("player/geiger3.wav");
	PRECACHE_SOUND("player/geiger2.wav");
	PRECACHE_SOUND("player/geiger1.wav");

	if (giPrecacheGrunt)
		UTIL_PrecacheOther("monster_human_grunt");
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if (g_pGameRules) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Half-Life";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
void Sys_Error(const char *error_string)
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization(edict_t *pEntity, customization_t *pCust)
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_console, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_console, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_console, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect(edict_t *pEntity)
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect();
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect(edict_t *pEntity)
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect();
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink(edict_t *pEntity)
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink();
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility(edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas)
{
	Vector org;
	edict_t *pView = pClient;

	// Find the client's PVS
	if (pViewEntity)
	{
		pView = pViewEntity;
	}

	// Observers use the visibility of their target
	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pClient);
	if (pPlayer->pev->iuser1 == OBS_IN_EYE && pPlayer->pev->iuser2 != 0 && pPlayer->m_hObserverTarget != NULL)
	{
		ASSERT(pPlayer->pev->iuser2 == ENTINDEX(pPlayer->m_hObserverTarget->edict()));
		pView = pPlayer->m_hObserverTarget->edict();
	}

	if (pClient->v.flags & FL_PROXY)
	{
		*pvs = NULL; // the spectator proxy sees
		*pas = NULL; // and hears everything
		return;
	}

	org = pView->v.origin + pView->v.view_ofs;
	if (pView->v.flags & FL_DUCKING)
	{
		org = org + (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	*pvs = ENGINE_SET_PVS((float *)&org);
	*pas = ENGINE_SET_PAS((float *)&org);
}

#include "entity_state.h"

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
int AddToFullPack(struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet)
{
	int i;

	// don't send if flagged for NODRAW and it's not the host getting the message
	if ((ent->v.effects == EF_NODRAW) && (ent != host))
		return 0;

	// Ignore ents without valid / visible models
	if (!ent->v.modelindex || !STRING(ent->v.model))
		return 0;

	// Don't send spectators to other players
	if ((ent->v.flags & FL_SPECTATOR) && (ent != host))
	{
		return 0;
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	if (ent != host)
	{
		if (!ENGINE_CHECK_VISIBILITY((const struct edict_s *)ent, pSet))
		{
			return 0;
		}
	}

	// Don't send entity to local client if the client says it's predicting the entity itself.
	if (ent->v.flags & FL_SKIPLOCALHOST)
	{
		if ((hostflags & 1) && (ent->v.owner == host))
			return 0;
	}

	if (host->v.groupinfo)
	{
		UTIL_SetGroupTrace(host->v.groupinfo, GROUP_OP_AND);

		// Should always be set, of course
		if (ent->v.groupinfo)
		{
			if (g_groupop == GROUP_OP_AND)
			{
				if (!(ent->v.groupinfo & host->v.groupinfo))
					return 0;
			}
			else if (g_groupop == GROUP_OP_NAND)
			{
				if (ent->v.groupinfo & host->v.groupinfo)
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset(state, 0, sizeof(*state));

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number = e;
	state->entityType = ENTITY_NORMAL;

	// Flag custom entities.
	if (ent->v.flags & FL_CUSTOMENTITY)
	{
		state->entityType = ENTITY_BEAM;
	}

	//
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime = (int)(1000.0 * ent->v.animtime) / 1000.0;

	memcpy(state->origin, ent->v.origin, 3 * sizeof(float));
	memcpy(state->angles, ent->v.angles, 3 * sizeof(float));
	memcpy(state->mins, ent->v.mins, 3 * sizeof(float));
	memcpy(state->maxs, ent->v.maxs, 3 * sizeof(float));

	memcpy(state->startpos, ent->v.startpos, 3 * sizeof(float));
	memcpy(state->endpos, ent->v.endpos, 3 * sizeof(float));

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;

	state->frame = ent->v.frame;

	state->skin = ent->v.skin;
	state->effects = ent->v.effects;

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	if (!player && ent->v.animtime && ent->v.velocity[0] == 0 && ent->v.velocity[1] == 0 && ent->v.velocity[2] == 0)
	{
		state->eflags |= EFLAG_SLERP;
	}

	state->scale = ent->v.scale;
	state->solid = ent->v.solid;
	state->colormap = ent->v.colormap;

	state->movetype = ent->v.movetype;
	state->sequence = ent->v.sequence;
	state->framerate = ent->v.framerate;
	state->body = ent->v.body;

	for (i = 0; i < 4; i++)
	{
		state->controller[i] = ent->v.controller[i];
	}

	for (i = 0; i < 2; i++)
	{
		state->blending[i] = ent->v.blending[i];
	}

	state->rendermode = ent->v.rendermode;
	state->renderamt = ent->v.renderamt;
	state->renderfx = ent->v.renderfx;
	state->rendercolor.r = ent->v.rendercolor.x;
	state->rendercolor.g = ent->v.rendercolor.y;
	state->rendercolor.b = ent->v.rendercolor.z;

	state->aiment = 0;
	if (ent->v.aiment)
	{
		state->aiment = ENTINDEX(ent->v.aiment);
		// Change ent for egon beam for spectators to look like it goes from the client weapon if in first person mode
		if (state->entityType == ENTITY_BEAM && host->v.iuser1 == OBS_IN_EYE && host->v.iuser2 == state->aiment)
		{
			state->aiment = ENTINDEX(host);
			state->skin = (state->aiment & 0x0FFF) | (state->skin & 0xF000);
		}
	}

	state->owner = 0;
	if (ent->v.owner)
	{
		int owner = ENTINDEX(ent->v.owner);

		// Only care if owned by a player
		if (owner >= 1 && owner <= gpGlobals->maxClients)
		{
			state->owner = owner;
		}
	}

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	if (!player)
	{
		state->playerclass = ent->v.playerclass;
	}

	// Special stuff for players only
	if (player)
	{
		memcpy(state->basevelocity, ent->v.basevelocity, 3 * sizeof(float));

		state->weaponmodel = MODEL_INDEX(STRING(ent->v.weaponmodel));
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction = ent->v.friction;

		state->gravity = ent->v.gravity;

		if (ent->v.iuser1)
			state->team = -1; // Set team if player is spectator. This will enable "Cancel" button in team menu.

		state->usehull = (ent->v.flags & FL_DUCKING) ? 1 : 0;
		state->health = ent->v.health;
	}

	if (ent->v.renderfx == kRenderFxDeadPlayer)
	{
		state->movetype = MOVETYPE_NONE;
		state->solid = SOLID_NOT;
	}

	CBaseEntity *pEntity = static_cast<CBaseEntity *>(GET_PRIVATE(ent));
	if (pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
		state->eflags |= EFLAG_FLESH_SOUND;
	else
		state->eflags &= ~EFLAG_FLESH_SOUND;

	return 1;
}

// defaults for clientinfo messages
#define DEFAULT_VIEWHEIGHT 28

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
void CreateBaseline(int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, Vector player_mins, Vector player_maxs)
{
	baseline->origin = entity->v.origin;
	baseline->angles = entity->v.angles;
	baseline->frame = entity->v.frame;
	baseline->skin = (short)entity->v.skin;

	// render information
	baseline->rendermode = (byte)entity->v.rendermode;
	baseline->renderamt = (byte)entity->v.renderamt;
	baseline->rendercolor.r = (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g = (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b = (byte)entity->v.rendercolor.z;
	baseline->renderfx = (byte)entity->v.renderfx;

	if (player)
	{
		baseline->mins = player_mins;
		baseline->maxs = player_maxs;

		baseline->colormap = eindex;
		baseline->modelindex = playermodelindex;
		baseline->friction = 1.0;
		baseline->movetype = MOVETYPE_WALK;

		baseline->scale = entity->v.scale;
		baseline->solid = SOLID_SLIDEBOX;
		baseline->framerate = 1.0;
		baseline->gravity = 1.0;
	}
	else
	{
		baseline->mins = entity->v.mins;
		baseline->maxs = entity->v.maxs;

		baseline->colormap = 0;
		baseline->modelindex = entity->v.modelindex; //SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype = entity->v.movetype;

		baseline->scale = entity->v.scale;
		baseline->solid = entity->v.solid;
		baseline->framerate = entity->v.framerate;
		baseline->gravity = entity->v.gravity;
	}
}

typedef struct
{
	char name[32];
	int field;
} entity_field_alias_t;

#define FIELD_ORIGIN0 0
#define FIELD_ORIGIN1 1
#define FIELD_ORIGIN2 2
#define FIELD_ANGLES0 3
#define FIELD_ANGLES1 4
#define FIELD_ANGLES2 5

static entity_field_alias_t entity_field_alias[] = {
	{ "origin[0]", 0 },
	{ "origin[1]", 0 },
	{ "origin[2]", 0 },
	{ "angles[0]", 0 },
	{ "angles[1]", 0 },
	{ "angles[2]", 0 },
};

void Entity_FieldInit(struct delta_s *pFields)
{
	entity_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN0].name);
	entity_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN1].name);
	entity_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN2].name);
	entity_field_alias[FIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES0].name);
	entity_field_alias[FIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES1].name);
	entity_field_alias[FIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES2].name);
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
void Entity_Encode(struct delta_s *pFields, const unsigned char *from, const unsigned char *to)
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if (!initialized)
	{
		Entity_FieldInit(pFields);
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->impacttime != 0) && (t->starttime != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);

		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) && (t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
}

static entity_field_alias_t player_field_alias[] = {
	{ "origin[0]", 0 },
	{ "origin[1]", 0 },
	{ "origin[2]", 0 },
};

void Player_FieldInit(struct delta_s *pFields)
{
	player_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN0].name);
	player_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN1].name);
	player_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN2].name);
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
void Player_Encode(struct delta_s *pFields, const unsigned char *from, const unsigned char *to)
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if (!initialized)
	{
		Player_FieldInit(pFields);
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) && (t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, player_field_alias[FIELD_ORIGIN2].field);
	}
}

#define CUSTOMFIELD_ORIGIN0  0
#define CUSTOMFIELD_ORIGIN1  1
#define CUSTOMFIELD_ORIGIN2  2
#define CUSTOMFIELD_ANGLES0  3
#define CUSTOMFIELD_ANGLES1  4
#define CUSTOMFIELD_ANGLES2  5
#define CUSTOMFIELD_SKIN     6
#define CUSTOMFIELD_SEQUENCE 7
#define CUSTOMFIELD_ANIMTIME 8

entity_field_alias_t custom_entity_field_alias[] = {
	{ "origin[0]", 0 },
	{ "origin[1]", 0 },
	{ "origin[2]", 0 },
	{ "angles[0]", 0 },
	{ "angles[1]", 0 },
	{ "angles[2]", 0 },
	{ "skin", 0 },
	{ "sequence", 0 },
	{ "animtime", 0 },
};

void Custom_Entity_FieldInit(struct delta_s *pFields)
{
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].name);
	custom_entity_field_alias[CUSTOMFIELD_SKIN].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].name);
	custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].name);
	custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].name);
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode(struct delta_s *pFields, const unsigned char *from, const unsigned char *to)
{
	entity_state_t *f, *t;
	int beamType;
	static int initialized = 0;

	if (!initialized)
	{
		Custom_Entity_FieldInit(pFields);
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	beamType = t->rendermode & 0x0f;

	if (beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field);
	}

	if (beamType != BEAM_POINTS)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field);
	}

	if (beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field);
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ((int)f->animtime == (int)t->animtime)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field);
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders(void)
{
	DELTA_ADDENCODER("Entity_Encode", Entity_Encode);
	DELTA_ADDENCODER("Custom_Encode", Custom_Encode);
	DELTA_ADDENCODER("Player_Encode", Player_Encode);
}

int GetWeaponData(struct edict_s *player, struct weapon_data_s *info)
{
#if defined(CLIENT_WEAPONS)
	int i;
	weapon_data_t *item;
	entvars_t *pev = &player->v;
	CBasePlayer *pl = (CBasePlayer *)CBasePlayer::Instance(pev);
	CBasePlayerWeapon *gun;

	ItemInfo II;

	memset(info, 0, 32 * sizeof(weapon_data_t));

	if (!pl)
		return 1;

	// go through all of the weapons and make a list of the ones to pack
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (pl->m_rgpPlayerItems[i])
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = pl->m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();
				if (gun && gun->UseDecrement())
				{
					// Get The ID.
					memset(&II, 0, sizeof(II));
					gun->GetItemInfo(&II);

					if (II.iId >= 0 && II.iId < 32)
					{
						item = &info[II.iId];

						item->m_iId = II.iId;
						item->m_iClip = gun->m_iClip;

						item->m_flTimeWeaponIdle = max(gun->m_flTimeWeaponIdle, -0.001f);
						item->m_flNextPrimaryAttack = max(gun->m_flNextPrimaryAttack, -0.001f);
						item->m_flNextSecondaryAttack = max(gun->m_flNextSecondaryAttack, -0.001f);
						item->m_fInReload = gun->m_fInReload;
						item->m_fInSpecialReload = gun->m_fInSpecialReload;
						item->fuser1 = max(gun->pev->fuser1, -0.001f);
						item->fuser2 = gun->m_flStartThrow;
						item->fuser3 = gun->m_flReleaseThrow;
						item->iuser1 = gun->m_chargeReady;
						item->iuser2 = gun->m_fInAttack;
						item->iuser3 = gun->m_fireState;

						//						item->m_flPumpTime				= max( gun->m_flPumpTime, -0.001 );
					}
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
#else
	memset(info, 0, 32 * sizeof(weapon_data_t));
#endif
	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
void UpdateClientData(const struct edict_s *ent, int sendweapons, struct clientdata_s *cd)
{
	entvars_t *pev = (entvars_t *)&ent->v;

	if (pev->iuser1 == OBS_IN_EYE)
	{
		CBasePlayer *pl = (CBasePlayer *)CBasePlayer::Instance(pev);
		if (pl && pl->m_hObserverTarget)
		{
			pev = &(pl->m_hObserverTarget->edict()->v);
		}
	}

	cd->flags = pev->flags;

	// Clamp value for delta compression
	if (pev->health <= 0.0)
		cd->health = 0.0;
	else if (pev->health <= 1.0)
		cd->health = 1.0;
	else if ((int)pev->health < 0)
		cd->health = 0x7FFFFF00; // (int)(float)0x7FFFFF00 == 0x7FFFFF00, (int)(float)0x7FFFFFF0 != 0x7FFFFFF0
	else
		cd->health = pev->health;

	cd->viewmodel = MODEL_INDEX(STRING(pev->viewmodel));

	cd->waterlevel = pev->waterlevel;
	cd->watertype = pev->watertype;
	cd->weapons = pev->weapons;

	// Vectors
	cd->origin = pev->origin;
	cd->velocity = pev->velocity;
	cd->view_ofs = pev->view_ofs;
	cd->punchangle = pev->punchangle;

	cd->bInDuck = pev->bInDuck;
	cd->flTimeStepSound = pev->flTimeStepSound;
	cd->flDuckTime = pev->flDuckTime;
	cd->flSwimTime = pev->flSwimTime;
	cd->waterjumptime = pev->teleport_time;

	UTIL_strcpy(cd->physinfo, ENGINE_GETPHYSINFO(ent));

	cd->maxspeed = pev->maxspeed;
	cd->fov = pev->fov;
	cd->weaponanim = pev->weaponanim;

	cd->pushmsec = pev->pushmsec;

	// Observer
	cd->iuser1 = ent->v.iuser1;
	cd->iuser2 = ent->v.iuser2;

#if defined(CLIENT_WEAPONS)
	if (sendweapons)
	{
		entvars_t *pev = (entvars_t *)&ent->v;
		CBasePlayer *pl = (CBasePlayer *)CBasePlayer::Instance(pev);

		if (pl)
		{
			cd->m_flNextAttack = pl->m_flNextAttack;
			cd->fuser2 = pl->m_flNextAmmoBurn;
			cd->fuser3 = pl->m_flAmmoStartCharge;
			cd->vuser1.x = pl->ammo_9mm;
			cd->vuser1.y = pl->ammo_357;
			cd->vuser1.z = pl->ammo_argrens;
			cd->ammo_nails = pl->ammo_bolts;
			cd->ammo_shells = pl->ammo_buckshot;
			cd->ammo_rockets = pl->ammo_rockets;
			cd->ammo_cells = pl->ammo_uranium;
			cd->vuser2.x = pl->ammo_hornets;

			if (pl->m_pActiveItem)
			{
				CBasePlayerWeapon *gun;
				gun = (CBasePlayerWeapon *)pl->m_pActiveItem->GetWeaponPtr();
				if (gun && gun->UseDecrement())
				{
					ItemInfo II;
					memset(&II, 0, sizeof(II));
					gun->GetItemInfo(&II);

					cd->m_iId = II.iId;

					cd->vuser3.z = gun->m_iSecondaryAmmoType;
					cd->vuser4.x = gun->m_iPrimaryAmmoType;

					if (gun->m_iPrimaryAmmoType > 0)
						cd->vuser4.y = pl->m_rgAmmo[gun->m_iPrimaryAmmoType];

					if (gun->m_iSecondaryAmmoType > 0)
						cd->vuser4.z = pl->m_rgAmmo[gun->m_iSecondaryAmmoType];

					if (pl->m_pActiveItem->m_iId == WEAPON_RPG)
					{
						cd->vuser2.y = ((CRpg *)pl->m_pActiveItem)->m_fSpotActive;
						cd->vuser2.z = ((CRpg *)pl->m_pActiveItem)->m_cActiveRockets;
					}
				}
			}
		}
	}
#endif
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart(const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed)
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = (CBasePlayer *)CBasePlayer::Instance(pev);

	if (!pl)
		return;

	if (pl->pev->groupinfo != 0)
	{
		UTIL_SetGroupTrace(pl->pev->groupinfo, GROUP_OP_AND);
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd(const edict_t *player)
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = (CBasePlayer *)CBasePlayer::Instance(pev);

	if (!pl)
		return;
	if (pl->pev->groupinfo != 0)
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int ConnectionlessPacket(const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size)
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds(int hullnumber, float *mins, float *maxs)
{
	int iret = 0;

	switch (hullnumber)
	{
	case 0: // Normal player
		mins = VEC_HULL_MIN;
		maxs = VEC_HULL_MAX;
		iret = 1;
		break;
	case 1: // Crouched player
		mins = VEC_DUCK_HULL_MIN;
		maxs = VEC_DUCK_HULL_MAX;
		iret = 1;
		break;
	case 2: // Point based hull
		mins = Vector(0, 0, 0);
		maxs = Vector(0, 0, 0);
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
void CreateInstancedBaselines(void)
{
	int iret = 0;
	entity_state_t state;

	memset(&state, 0, sizeof(state));

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int InconsistentFile(const edict_t *player, const char *filename, char *disconnect_message)
{
	// Server doesn't care?
	if (CVAR_GET_FLOAT("mp_consistency") != 1)
		return 0;

	// Default behavior is to kick the player
	sprintf(disconnect_message, "Server is enforcing file consistency for %s\n", filename);

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
int AllowLagCompensation(void)
{
	return 1;
}

// When the pfnQueryClientCvarValue2() completes it will call pfnCvarValue2() with the
// request ID you supplied earlier, the name of the cvar you requested and the value of that cvar.
void CvarValue2(const edict_t *pEnt, int requestID, const char *cvarName, const char *value)
{
	serverapi()->CvarValueCallback(pEnt, requestID, cvarName, value);
}
