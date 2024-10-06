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
//
// teamplay_gamerules.cpp
//
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

#include "skill.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"
#include "hltv.h"
#include "trains.h"

#include <ctype.h>

#include <CBugfixedServer.h>

extern DLL_GLOBAL CGameRules *g_pGameRules;
extern DLL_GLOBAL BOOL g_fGameOver;
extern int gmsgDeathMsg; // client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgHtmlMOTD;
extern int gmsgServerName;

extern int g_teamplay;

extern ConVar sv_bhl_defer_motd;

#define ITEM_RESPAWN_TIME   30
#define WEAPON_RESPAWN_TIME 20
#define AMMO_RESPAWN_TIME   20

float g_flIntermissionStartTime = 0;

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME 120

extern cvar_t timeleft, fragsleft, sv_busters;

extern cvar_t mp_chattime;

CVoiceGameMgr g_VoiceGameMgr;

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker)
	{
		if (g_teamplay)
		{
			if (g_pGameRules->PlayerRelationship(pListener, pTalker) != GR_TEAMMATE)
			{
				return false;
			}
		}

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay ::CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	RefreshSkillData();
	m_flIntermissionEndTime = 0;
	g_flIntermissionStartTime = 0;

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)

	// 2/16/2014
	// Removed execution of servercfgfile, because server is now do it on its own.
	if (!IS_DEDICATED_SERVER())
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING("lservercfgfile");

		if (lservercfgfile && lservercfgfile[0])
		{
			char szCommand[256];

			ALERT(at_console, "Executing listen server config file\n");
			sprintf(szCommand, "exec %s\n", lservercfgfile);
			SERVER_COMMAND(szCommand);
		}
	}
}

//=========================================================
// ClientCommand
// the user has typed a command which is unrecognized by everything else;
// this check to see if the gamerules knows anything about the command
//=========================================================
BOOL CHalfLifeMultiplay::ClientCommand(CBasePlayer *pPlayer, const char *pcmd)
{
	if (g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return TRUE;

	if (FStrEq(pcmd, "menuselect"))
	{
		if (CMD_ARGC() < 2)
			return TRUE;

		int slot = atoi(CMD_ARGV(1));

		// There is no menu usages in server dll.

		return TRUE;
	}

	return CGameRules::ClientCommand(pPlayer, pcmd);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData(void)
{
	// load all default values
	CGameRules::RefreshSkillData();

	// override some values for multiplay.

	// suitcharger
	gSkillData.suitchargerCapacity = 30;

	// Crowbar whack
	gSkillData.plrDmgCrowbar = mp_dmg_crowbar.value;

	// Glock Round
	gSkillData.plrDmg9MM = mp_dmg_glock.value;

	// 357 Round
	gSkillData.plrDmg357 = mp_dmg_357.value;

	// MP5 Round
	gSkillData.plrDmgMP5 = mp_dmg_mp5.value;

	// M203 grenade
	gSkillData.plrDmgM203Grenade = mp_dmg_m203.value;

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = mp_dmg_shotgun.value;

	// Crossbow
	gSkillData.plrDmgCrossbowScope = mp_dmg_xbow_scope.value;
	gSkillData.plrDmgCrossbowNoScope = mp_dmg_xbow_noscope.value;

	// RPG
	gSkillData.plrDmgRPG = mp_dmg_rpg.value;

	// Egon
	gSkillData.plrDmgEgonWide = mp_dmg_egon.value;

	// Hand Grenade
	gSkillData.plrDmgHandGrenade = mp_dmg_hgrenade.value;

	// Satchel Charge
	gSkillData.plrDmgSatchel = mp_dmg_satchel.value;

	// Tripmine
	gSkillData.plrDmgTripmine = mp_dmg_tripmine.value;

	// hornet
	gSkillData.plrDmgHornet = mp_dmg_hornet.value;

	// gauss
	gSkillData.plrDmgGauss = mp_dmg_gauss_primary.value;
	gSkillData.plrDmgGaussSecondary = mp_dmg_gauss_secondary.value;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::Think(void)
{
	CGameRules::Think();
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	if (g_fGameOver) // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT("mp_chattime");
		if (time < 1)
			CVAR_SET_STRING("mp_chattime", "1");
		else if (time > MAX_INTERMISSION_TIME)
			CVAR_SET_STRING("mp_chattime", UTIL_dtos1(MAX_INTERMISSION_TIME));

		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime.value;

		// check to see if we should change levels now
		if (m_flIntermissionEndTime < gpGlobals->time)
		{
			if (m_iEndIntermissionButtonHit // check that someone has pressed a key, or the max intermission time is over
			    || ((g_flIntermissionStartTime + MAX_INTERMISSION_TIME) < gpGlobals->time))
				ChangeLevel(); // intermission is over
		}

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;

	time_remaining = (int)(flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);

	if (flTimeLimit != 0 && gpGlobals->time >= flTimeLimit)
	{
		GoToIntermission();
		return;
	}

	if (flFragLimit)
	{
		bool first = true;
		int bestfrags = flFragLimit;
		int remain;

		// check if any player is over the frag limit
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex(i);
			if (pPlayer == NULL)
				continue;

			if (pPlayer->pev->frags >= flFragLimit)
			{
				GoToIntermission();
				return;
			}

			remain = flFragLimit - pPlayer->pev->frags;
			if (first)
			{
				bestfrags = remain;
				first = false;
				continue;
			}
			if (remain < bestfrags)
			{
				bestfrags = remain;
			}
		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if (frags_remaining != last_frags)
	{
		g_engfuncs.pfnCvar_DirectSet(&fragsleft, UTIL_VarArgs("%i", frags_remaining));
	}

	// Updates once per second
	if (timeleft.value != last_time)
	{
		g_engfuncs.pfnCvar_DirectSet(&timeleft, UTIL_VarArgs("%i", time_remaining));
	}

	last_frags = frags_remaining;
	last_time = time_remaining;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer(void)
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch(void)
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp(void)
{
	return gpGlobals->coop;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon)
{
	if (!pWeapon->CanDeploy())
	{
		// that weapon can't deploy anyway.
		return FALSE;
	}

	if (!pPlayer->m_pActiveItem)
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if (pPlayer->m_iAutoWepSwitch == 0)
	{
		// player disabled auto weapon switching
		return FALSE;
	}
	else if (pPlayer->m_iAutoWepSwitch == 2)
	{
		// player disabled auto weapon switching when firing
		if (pPlayer->m_afButtonLast & (IN_ATTACK | IN_ATTACK2))
			return FALSE;
	}

	if (!pPlayer->m_pActiveItem->CanHolster())
	{
		// can't put away the active item.
		return FALSE;
	}

	if (pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight())
	{
		return TRUE;
	}

	return FALSE;
}

extern BOOL HLGetNextBestWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon);

BOOL CHalfLifeMultiplay ::GetNextBestWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon)
{
	return HLGetNextBestWeapon(pPlayer, pCurrentWeapon);
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay ::ClientConnected(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
{
	g_VoiceGameMgr.ClientConnected(pEntity);
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;
extern int gmsgTeamInfo;
extern int gmsgTeamNames;
extern int gmsgSpectator;
extern int gmsgAllowSpec;

void CHalfLifeMultiplay ::UpdateGameMode(CBasePlayer *pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgGameMode, NULL, pPlayer->edict());
	// There is plugin (su-27) for scoreboard manipulation under AMXX, it should receive 0 for correct behaviour
	// So in any case (AMXX installed and have or not su-27) we should send 0 down
	if (g_amxmodx_version)
		WRITE_BYTE(0); // game mode none
	else
		WRITE_BYTE(1); // game mode teamplay
	MESSAGE_END();
}

void CHalfLifeMultiplay ::InitHUD(CBasePlayer *pl)
{
	// Send allow_spectators status
	MESSAGE_BEGIN(MSG_ONE, gmsgAllowSpec, NULL, pl->edict());
	WRITE_BYTE(allow_spectators.value);
	MESSAGE_END();

	// notify other clients of player joining the game
	if (((int)mp_notify_player_status.value & 2) == 2)
	{
		const char *name = pl->pev->netname ? STRING(pl->pev->netname) : "";
		if (name[0] == 0)
			name = "unconnected";
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("+ %s has joined the game\n", name));
	}

	// team match?
	if (g_teamplay)
	{
		UTIL_LogPrintf("\"%s<%i><%s><%s>\" entered the game\n",
		    STRING(pl->pev->netname),
		    GETPLAYERUSERID(pl->edict()),
		    GETPLAYERAUTHID(pl->edict()),
		    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pl->edict()), "model"));
	}
	else
	{
		UTIL_LogPrintf("\"%s<%i><%s><%i>\" entered the game\n",
		    STRING(pl->pev->netname),
		    GETPLAYERUSERID(pl->edict()),
		    GETPLAYERAUTHID(pl->edict()),
		    GETPLAYERUSERID(pl->edict()));
	}

	UpdateGameMode(pl);

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	// Let all know that new player have zero score
	pl->SendScoreInfo();

	if (!g_teamplay)
	{
		// Send this player team info to all
		MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
		WRITE_BYTE(pl->entindex());
		if (g_amxmodx_version)
			WRITE_STRING(pl->pev->iuser1 ? "" : pl->TeamID());
		else
			WRITE_STRING(pl->pev->iuser1 ? "" : "Players");
		MESSAGE_END();
	}

	// Send player spectator status (it is not used in client dll)
	MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
	WRITE_BYTE(pl->entindex());
	WRITE_BYTE(pl->IsObserver());
	MESSAGE_END();

	// Send server name
	SendServerNameToClient(pl->edict());

	if (!sv_bhl_defer_motd.GetBool() || serverapi()->IsClientSupportsReceived(pl->entindex()))
		SendDefaultMOTDToClient(pl->edict());

	// loop through all active players and send their score and team info to the new client
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);

		if (plr)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgScoreInfo, NULL, pl->edict());
			WRITE_BYTE(i); // client number
			WRITE_SHORT(plr->pev->frags);
			WRITE_SHORT(plr->m_iDeaths);
			WRITE_SHORT(0);
			WRITE_SHORT(GetTeamIndex(plr->m_szTeamName) + 1);
			MESSAGE_END();

			if (!g_teamplay)
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgTeamInfo, NULL, pl->edict());
				WRITE_BYTE(plr->entindex());
				if (g_amxmodx_version)
					WRITE_STRING(plr->pev->iuser1 ? "" : plr->TeamID());
				else
					WRITE_STRING(plr->pev->iuser1 ? "" : "Players");
				MESSAGE_END();
			}
		}
	}

	if (g_fGameOver)
	{
		MESSAGE_BEGIN(MSG_ONE, SVC_INTERMISSION, NULL, pl->edict());
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::ClientDisconnected(edict_t *pClient)
{
	if (!pClient)
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pClient);
	if (!pPlayer)
		return;

	// notify other clients of player leaving the game
	if (((int)mp_notify_player_status.value & 1) == 1)
	{
		const char *name = pPlayer->pev->netname ? STRING(pPlayer->pev->netname) : "";
		if (name[0] == 0)
			name = "unconnected";
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("- %s has left the game\n", name));
	}

	FireTargets("game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0);

	// team match?
	if (g_teamplay)
	{
		UTIL_LogPrintf("\"%s<%i><%s><%s>\" disconnected\n",
		    STRING(pPlayer->pev->netname),
		    GETPLAYERUSERID(pPlayer->edict()),
		    GETPLAYERAUTHID(pPlayer->edict()),
		    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model"));
	}
	else
	{
		UTIL_LogPrintf("\"%s<%i><%s><%i>\" disconnected\n",
		    STRING(pPlayer->pev->netname),
		    GETPLAYERUSERID(pPlayer->edict()),
		    GETPLAYERAUTHID(pPlayer->edict()),
		    GETPLAYERUSERID(pPlayer->edict()));
	}

	if (pPlayer->m_pTank != NULL)
	{
		// Stop controlling the tank
		pPlayer->m_pTank->Use(pPlayer, pPlayer, USE_OFF, 0);
	}

	pPlayer->RemoveAllItems(TRUE); // destroy all of the players weapons and items

	// Tell all clients this player isn't a spectator anymore
	MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
	WRITE_BYTE(ENTINDEX(pClient));
	WRITE_BYTE(0);
	MESSAGE_END();
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay ::FlPlayerFallDamage(CBasePlayer *pPlayer)
{
	int iFallDamage = (int)falldamage.value;

	switch (iFallDamage)
	{
	case 1: // progressive
		pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		break;
	case 2: // no damage
		return 0;
	default:
	case 0: // fixed
		return 10;
		break;
	}
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage(CBasePlayer *pPlayer, CBaseEntity *pAttacker)
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::PlayerThink(CBasePlayer *pPlayer)
{
	if (g_fGameOver)
	{
		// check for button presses
		if (pPlayer->m_afButtonPressed & (IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP))
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::PlayerSpawn(CBasePlayer *pPlayer)
{
	BOOL addDefault;
	CBaseEntity *pWeaponEntity = NULL;

	// Start welcome cam for new players
	if (!pPlayer->m_bPutInServer && mp_welcomecam.value != 0)
	{
		// don't let him spawn as soon as he enters the server
		// give enough time to plugins to send the player to spectator mode
		pPlayer->m_flNextAttack = 0.2;

		pPlayer->StartWelcomeCam();
		return;
	}

	int aws = pPlayer->m_iAutoWepSwitch;
	pPlayer->m_iAutoWepSwitch = 1;

	pPlayer->pev->weapons |= (1 << WEAPON_SUIT);

	addDefault = TRUE;

	while (pWeaponEntity = UTIL_FindEntityByClassname(pWeaponEntity, "game_player_equip"))
	{
		pWeaponEntity->Touch(pPlayer);
		addDefault = FALSE;
	}

	if (addDefault)
	{
		pPlayer->GiveNamedItem("weapon_crowbar");
		pPlayer->GiveNamedItem("weapon_9mmhandgun");
		pPlayer->GiveAmmo(68, "9mm", _9MM_MAX_CARRY); // 4 full reloads
	}

	FireTargets("game_playerspawn", pPlayer, pPlayer, USE_TOGGLE, 0);

	pPlayer->m_iAutoWepSwitch = aws;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay ::FPlayerCanRespawn(CBasePlayer *pPlayer)
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay ::FlPlayerSpawnTime(CBasePlayer *pPlayer)
{
	return gpGlobals->time; //now!
}

BOOL CHalfLifeMultiplay ::AllowAutoTargetCrosshair(void)
{
	return (aimcrosshair.value != 0);
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay ::IPointsForKill(CBasePlayer *pAttacker, CBasePlayer *pKilled)
{
	return 1;
}

//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeMultiplay ::PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor)
{
	CBasePlayer *peKiller = nullptr;
	CBaseEntity *ktmp = CBaseEntity::Instance(pKiller);
	if (ktmp && ktmp->Classify() == CLASS_PLAYER)
	{
		peKiller = static_cast<CBasePlayer *>(ktmp);
	}
	else if (ktmp && ktmp->Classify() == CLASS_VEHICLE)
	{
		CBasePlayer *pDriver = static_cast<CFuncVehicle *>(ktmp)->m_pDriver;
		if (pDriver != nullptr)
		{
			peKiller = pDriver;
			ktmp = pDriver;
			pKiller = pDriver->pev;
		}
	}

	DeathNotice(pVictim, pKiller, pInflictor);

	pVictim->m_iDeaths += 1;

	FireTargets("game_playerdie", pVictim, pVictim, USE_TOGGLE, 0);

	if (pVictim->pev == pKiller)
	{ // killed self
		pKiller->frags -= 1;
	}
	else if (ktmp && ktmp->IsPlayer())
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill(peKiller, pVictim);

		FireTargets("game_playerkill", ktmp, ktmp, USE_TOGGLE, 0);
	}
	else
	{ // killed by the world
		pKiller->frags -= 1;
	}

	// update the scores
	// killed scores
	pVictim->SendScoreInfo();

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance(pKiller);
	if (ep && ep->Classify() == CLASS_PLAYER)
	{
		CBasePlayer *PK = (CBasePlayer *)ep;

		PK->SendScoreInfo();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
}

//=========================================================
// Deathnotice.
//=========================================================
void CHalfLifeMultiplay::DeathNotice(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor)
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance(pKiller);

	const char *killer_weapon_name = "world"; // by default, the player is killed by the world
	int killer_index = 0;

	// Hack to fix name change
	char *tau = "tau_cannon";
	char *gluon = "gluon gun";

	if (pKiller->flags & FL_CLIENT)
	{
		killer_index = ENTINDEX(ENT(pKiller));

		if (pevInflictor)
		{
			if (pevInflictor == pKiller)
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pKiller);

				if (pPlayer->m_pActiveItem)
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING(pevInflictor->classname); // it's just that easy
			}
		}
	}
	else
	{
		if (pevInflictor)
		{
			killer_weapon_name = STRING(pevInflictor->classname);
		}
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if (strncmp(killer_weapon_name, "weapon_", 7) == 0)
		killer_weapon_name += 7;
	else if (strncmp(killer_weapon_name, "monster_", 8) == 0)
		killer_weapon_name += 8;
	else if (strncmp(killer_weapon_name, "func_", 5) == 0)
		killer_weapon_name += 5;

	MESSAGE_BEGIN(MSG_ALL, gmsgDeathMsg);
	WRITE_BYTE(killer_index); // the killer
	WRITE_BYTE(ENTINDEX(pVictim->edict())); // the victim
	WRITE_STRING(killer_weapon_name); // what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if (!strcmp(killer_weapon_name, "egon"))
		killer_weapon_name = gluon;
	else if (!strcmp(killer_weapon_name, "gauss"))
		killer_weapon_name = tau;

	if (pVictim->pev == pKiller)
	{
		// killed self

		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",
			    STRING(pVictim->pev->netname),
			    GETPLAYERUSERID(pVictim->edict()),
			    GETPLAYERAUTHID(pVictim->edict()),
			    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pVictim->edict()), "model"),
			    killer_weapon_name);
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",
			    STRING(pVictim->pev->netname),
			    GETPLAYERUSERID(pVictim->edict()),
			    GETPLAYERAUTHID(pVictim->edict()),
			    GETPLAYERUSERID(pVictim->edict()),
			    killer_weapon_name);
		}
	}
	else if (pKiller->flags & FL_CLIENT)
	{
		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",
			    STRING(pKiller->netname),
			    GETPLAYERUSERID(ENT(pKiller)),
			    GETPLAYERAUTHID(ENT(pKiller)),
			    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(ENT(pKiller)), "model"),
			    STRING(pVictim->pev->netname),
			    GETPLAYERUSERID(pVictim->edict()),
			    GETPLAYERAUTHID(pVictim->edict()),
			    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pVictim->edict()), "model"),
			    killer_weapon_name);
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",
			    STRING(pKiller->netname),
			    GETPLAYERUSERID(ENT(pKiller)),
			    GETPLAYERAUTHID(ENT(pKiller)),
			    GETPLAYERUSERID(ENT(pKiller)),
			    STRING(pVictim->pev->netname),
			    GETPLAYERUSERID(pVictim->edict()),
			    GETPLAYERAUTHID(pVictim->edict()),
			    GETPLAYERUSERID(pVictim->edict()),
			    killer_weapon_name);
		}
	}
	else
	{
		// killed by the world

		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
			    STRING(pVictim->pev->netname),
			    GETPLAYERUSERID(pVictim->edict()),
			    GETPLAYERAUTHID(pVictim->edict()),
			    g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pVictim->edict()), "model"),
			    killer_weapon_name);
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
			    STRING(pVictim->pev->netname),
			    GETPLAYERUSERID(pVictim->edict()),
			    GETPLAYERAUTHID(pVictim->edict()),
			    GETPLAYERUSERID(pVictim->edict()),
			    killer_weapon_name);
		}
	}

	MESSAGE_BEGIN(MSG_SPEC, SVC_DIRECTOR);
	WRITE_BYTE(9); // command length in bytes
	WRITE_BYTE(DRC_CMD_EVENT); // player killed
	WRITE_SHORT(ENTINDEX(pVictim->edict())); // index number of primary entity
	if (pevInflictor)
		WRITE_SHORT(ENTINDEX(ENT(pevInflictor))); // index number of secondary entity
	else
		WRITE_SHORT(ENTINDEX(ENT(pKiller))); // index number of secondary entity
	WRITE_LONG(7 | DRC_FLAG_DRAMATIC); // eventflags (priority and flags)
	MESSAGE_END();

	//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
	/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		UTIL_strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		UTIL_strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		UTIL_strcpy ( szText, STRING( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	{
		UTIL_strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		UTIL_strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		UTIL_strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay ::PlayerGotWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon)
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay ::FlWeaponRespawnTime(CBasePlayerItem *pWeapon)
{
	if (weaponstay.value > 0)
	{
		// make sure it's only certain weapons
		if (!(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD))
		{
			return gpGlobals->time + 0; // weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE 100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay ::FlWeaponTryRespawn(CBasePlayerItem *pWeapon)
{
	if (pWeapon && pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD))
	{
		if (NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE))
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime(pWeapon);
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay ::VecWeaponRespawnSpot(CBasePlayerItem *pWeapon)
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay ::WeaponShouldRespawn(CBasePlayerItem *pWeapon)
{
	if (pWeapon->pev->spawnflags & SF_NORESPAWN)
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem(CBasePlayer *pPlayer, CBasePlayerItem *pItem)
{
	if (weaponstay.value > 0)
	{
		if (pItem->iFlags() & ITEM_FLAG_LIMITINWORLD)
			return CGameRules::CanHavePlayerItem(pPlayer, pItem);

		// check if the player already has this weapon
		for (int i = 0; i < MAX_ITEM_TYPES; i++)
		{
			CBasePlayerItem *it = pPlayer->m_rgpPlayerItems[i];

			while (it != NULL)
			{
				if (it->m_iId == pItem->m_iId)
				{
					return FALSE;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem(pPlayer, pItem);
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem(CBasePlayer *pPlayer, CItem *pItem)
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem(CBasePlayer *pPlayer, CItem *pItem)
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn(CItem *pItem)
{
	if (pItem->pev->spawnflags & SF_NORESPAWN)
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;
}

//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime(CItem *pItem)
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot(CItem *pItem)
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo(CBasePlayer *pPlayer, char *szName, int iCount)
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn(CBaseEntity *pEntity)
{
	//	if ( pEntity->pev->flags & FL_MONSTER )
	//		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn(CBasePlayerAmmo *pAmmo)
{
	if (pAmmo->pev->spawnflags & SF_NORESPAWN)
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime(CBasePlayerAmmo *pAmmo)
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot(CBasePlayerAmmo *pAmmo)
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime(void)
{
	return 60;
}

float CHalfLifeMultiplay::FlHEVChargerRechargeTime(void)
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons(CBasePlayer *pPlayer)
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo(CBasePlayer *pPlayer)
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot(CBasePlayer *pPlayer)
{
	edict_t *pentSpawnSpot = CGameRules::GetPlayerSpawnSpot(pPlayer);
	if (IsMultiplayer() && pentSpawnSpot->v.target)
	{
		FireTargets(STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0);
	}

	return pentSpawnSpot;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship(CBaseEntity *pPlayer, CBaseEntity *pTarget)
{
	if (!pPlayer || !pTarget || !pPlayer->IsPlayer() || !pTarget->IsPlayer())
		return GR_NOTTEAMMATE;
	// Spectators are teammates, but not players in welcomecam mode
	if (((CBasePlayer *)pPlayer)->IsObserver() && !((CBasePlayer *)pPlayer)->m_bInWelcomeCam && ((CBasePlayer *)pTarget)->IsObserver() && !((CBasePlayer *)pTarget)->m_bInWelcomeCam)
		return GR_TEAMMATE;
	// half life deathmatch has only enemies and spectators
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay ::PlayFootstepSounds(CBasePlayer *pl, float fvol)
{
	if (g_footsteps && g_footsteps->value == 0)
		return FALSE;

	if (pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220)
		return TRUE; // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay ::FAllowFlashlight(void)
{
	return flashlight.value != 0;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay ::FAllowMonsters(void)
{
	return (allowmonsters.value != 0);
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME 6

void CHalfLifeMultiplay ::GoToIntermission(void)
{
	if (g_fGameOver)
		return; // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT("mp_chattime");
	if (time < 1)
		CVAR_SET_STRING("mp_chattime", "1");
	else if (time > MAX_INTERMISSION_TIME)
		CVAR_SET_STRING("mp_chattime", UTIL_dtos1(MAX_INTERMISSION_TIME));

	m_flIntermissionEndTime = gpGlobals->time + ((int)mp_chattime.value);
	g_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[32];
	int minplayers, maxplayers;
	char rulebuffer[MAX_RULE_BUFFER];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle(mapcycle_t *cycle)
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if (p)
	{
		start = p;
		p = p->next;
		while (p != start)
		{
			n = p->next;
			delete p;
			p = n;
		}

		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

static char com_token[1500];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse(char *data)
{
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL; // end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',')
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting(char *buffer)
{
	char *p;

	p = buffer;
	while (*p && *p != '\n')
	{
		if (!isspace(*p) || isalnum(*p))
			return 1;

		p++;
	}

	return 0;
}

/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile(char *filename, mapcycle_t *cycle)
{
	char szBuffer[MAX_RULE_BUFFER];
	char szMap[32];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char *)LOAD_FILE_FOR_ME(filename, &length);
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if (pFileList && length)
	{
		// the first map name in the file becomes the default
		while (1)
		{
			hasbuffer = 0;
			memset(szBuffer, 0, MAX_RULE_BUFFER);

			pFileList = COM_Parse(pFileList);
			if (strlen(com_token) <= 0)
				break;

			strncpy(szMap, com_token, sizeof(szMap));
			szMap[sizeof(szMap) - 1] = '\0';

			// Any more tokens on this line?
			if (COM_TokenWaiting(pFileList))
			{
				pFileList = COM_Parse(pFileList);
				if (strlen(com_token) > 0)
				{
					hasbuffer = 1;
					strncpy(szBuffer, com_token, sizeof(szBuffer));
					szBuffer[sizeof(szBuffer) - 1] = '\0';
				}
			}

			// Check map
			if (IS_MAP_VALID(szMap))
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				UTIL_strcpy(item->mapname, szMap);

				item->minplayers = 0;
				item->maxplayers = 0;

				memset(item->rulebuffer, 0, MAX_RULE_BUFFER);

				if (hasbuffer)
				{
					s = g_engfuncs.pfnInfoKeyValue(szBuffer, "minplayers");
					if (s && s[0])
					{
						item->minplayers = atoi(s);
						item->minplayers = max(item->minplayers, 0);
						item->minplayers = min(item->minplayers, gpGlobals->maxClients);
					}
					s = g_engfuncs.pfnInfoKeyValue(szBuffer, "maxplayers");
					if (s && s[0])
					{
						item->maxplayers = atoi(s);
						item->maxplayers = max(item->maxplayers, 0);
						item->maxplayers = min(item->maxplayers, gpGlobals->maxClients);
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey(szBuffer, "minplayers");
					g_engfuncs.pfnInfo_RemoveKey(szBuffer, "maxplayers");

					UTIL_strcpy(item->rulebuffer, szBuffer);
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT(at_console, "Skipping %s from mapcycle, not a valid map\n", szMap);
			}
		}

		FREE_FILE(aFileList);
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while (item)
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if (!item)
	{
		return 0;
	}

	while (item->next)
	{
		item = item->next;
	}
	item->next = cycle->items;

	cycle->next_item = item->next;

	return 1;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers(void)
{
	int num = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex(i);

		if (pEnt)
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString(char *s, char *szCommand)
{
	// Now make rules happen
	char pkey[512];
	char value[512]; // use two buffers so compares
	    // work without stomping on each other
	char *o;

	if (*s == '\\')
		s++;

	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat(szCommand, pkey);
		if (strlen(value) > 0)
		{
			strcat(szCommand, " ");
			strcat(szCommand, value);
		}
		strcat(szCommand, "\n");

		if (!*s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay ::ChangeLevel(void)
{
	static char szPreviousMapCycleFile[256];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[1500];
	char szRules[1500];
	int minplayers = 0, maxplayers = 0;
	UTIL_strcpy(szFirstMapInList, "hldm1"); // the absolute default level is hldm1

	int curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char *)CVAR_GET_STRING("mapcyclefile");
	ASSERT(mapcfile != NULL);

	szCommands[0] = '\0';
	szRules[0] = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if (_stricmp(mapcfile, szPreviousMapCycleFile))
	{
		UTIL_strcpy(szPreviousMapCycleFile, mapcfile);

		DestroyMapCycle(&mapcycle);

		if (!ReloadMapCycleFile(mapcfile, &mapcycle) || (!mapcycle.items))
		{
			ALERT(at_console, "Unable to load map cycle file %s\n", mapcfile);
			do_cycle = FALSE;
		}
	}

	if (do_cycle && mapcycle.items)
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		UTIL_strcpy(szNextMap, STRING(gpGlobals->mapname));
		UTIL_strcpy(szFirstMapInList, STRING(gpGlobals->mapname));

		// Traverse list
		for (item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next)
		{
			keeplooking = FALSE;

			ASSERT(item != NULL);

			if (item->minplayers != 0)
			{
				if (curplayers >= item->minplayers)
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if (item->maxplayers != 0)
			{
				if (curplayers <= item->maxplayers)
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if (keeplooking)
				continue;

			found = TRUE;
			break;
		}

		if (!found)
		{
			item = mapcycle.next_item;
		}

		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		UTIL_strcpy(szNextMap, item->mapname);

		ExtractCommandString(item->rulebuffer, szCommands);
		UTIL_strcpy(szRules, item->rulebuffer);
	}

	if (!IS_MAP_VALID(szNextMap))
	{
		UTIL_strcpy(szNextMap, szFirstMapInList);
	}

	g_fGameOver = TRUE;

	ALERT(at_console, "CHANGE LEVEL: %s\n", szNextMap);
	if (minplayers || maxplayers)
	{
		ALERT(at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers);
	}
	if (strlen(szRules) > 0)
	{
		ALERT(at_console, "RULES:  %s\n", szRules);
	}

	CHANGE_LEVEL(szNextMap, NULL);
	if (strlen(szCommands) > 0)
	{
		SERVER_COMMAND(szCommands);
	}
}

void CHalfLifeMultiplay::SendServerNameToClient(edict_t *client)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgServerName, NULL, client);
	WRITE_STRING(CVAR_GET_STRING("hostname"));
	MESSAGE_END();
}

void CHalfLifeMultiplay::SendDefaultMOTDToClient(edict_t *client)
{
	// Send MOTD
	// 1. Check if motd TYPE is supported by the client
	// 2. If it is, then check if it is enabled in API. If not, do nothing.
	// 3. Try to send MOTD type TYPE. If failed, send the MOTD of next type.
	// 4. Repeat
	bool motdret = false;
	bhl::E_ClientSupports supports = serverapi()->GetClientSupports(ENTINDEX(client));

	if (IsEnumFlagSet(supports, bhl::E_ClientSupports::HtmlMotd))
	{
		if (serverapi()->GetAutomaticMotd(bhl::E_MotdType::Html))
			motdret = SendHtmlMOTDFileToClient(client);
		else
			motdret = true; // Handled by Metamod/AMXX/etc, do not send anything
	}

	if (!motdret && IsEnumFlagSet(supports, bhl::E_ClientSupports::UnicodeMotd))
	{
		if (serverapi()->GetAutomaticMotd(bhl::E_MotdType::Unicode))
			motdret = SendUnicodeMOTDFileToClient(client);
		else
			motdret = true; // Handled by Metamod/AMXX/etc, do not send anything
	}

	if (!motdret)
	{
		if (serverapi()->GetAutomaticMotd(bhl::E_MotdType::Plain))
			motdret = SendMOTDFileToClient(client);
		else
			motdret = true; // Handled by Metamod/AMXX/etc, do not send anything
	}
}

#define MAX_MOTD_CHUNK  60
#define MAX_MOTD_LENGTH 1536 // (MAX_MOTD_CHUNK * 4)

bool CHalfLifeMultiplay::SendMOTDFileToClient(edict_t *client, const char *file /*= nullptr*/)
{
	if (!file)
		file = (char *)CVAR_GET_STRING("motdfile");

	int length;
	char *aFileList = (char *)LOAD_FILE_FOR_ME(const_cast<char *>(file), &length);
	if (!aFileList)
		return false;
	SendMOTDToClient(client, aFileList);
	FREE_FILE(aFileList);
	return true;
}

void CHalfLifeMultiplay::SendMOTDToClient(edict_t *client, char *string)
{
	int char_count = 0;
	char *pFileList = string;
	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts
	while (pFileList && *pFileList && char_count < MAX_MOTD_LENGTH)
	{
		char chunk[MAX_MOTD_CHUNK + 1];
		UTIL_strcpy(chunk, pFileList);

		char_count += strlen(chunk);
		if (char_count < MAX_MOTD_LENGTH)
			pFileList = string + char_count;
		else
			*pFileList = 0;

		MESSAGE_BEGIN(MSG_ONE, gmsgMOTD, NULL, client);
		WRITE_BYTE(*pFileList ? FALSE : TRUE); // FALSE means there is still more message to come
		WRITE_STRING(chunk);
		MESSAGE_END();
	}
}

#define MAX_UNICODE_MOTD_LENGTH (MAX_MOTD_LENGTH * 2) // Some Unicode charachters take two or more bytes in UTF8

bool CHalfLifeMultiplay::SendUnicodeMOTDFileToClient(edict_t *client, const char *file /*= nullptr*/)
{
	if (!file)
		file = (char *)CVAR_GET_STRING("motdfile_unicode");

	int length;
	char *aFileList = (char *)LOAD_FILE_FOR_ME(const_cast<char *>(file), &length);
	if (!aFileList)
		return false;
	SendUnicodeMOTDToClient(client, aFileList);
	FREE_FILE(aFileList);
	return true;
}

void CHalfLifeMultiplay::SendUnicodeMOTDToClient(edict_t *client, char *string)
{
	int char_count = 0;
	char *pFileList = string;
	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts
	while (pFileList && *pFileList && char_count < MAX_UNICODE_MOTD_LENGTH)
	{
		char chunk[MAX_MOTD_CHUNK + 1];
		UTIL_strcpy(chunk, pFileList);

		char_count += strlen(chunk);
		if (char_count < MAX_UNICODE_MOTD_LENGTH)
			pFileList = string + char_count;
		else
			*pFileList = 0;

		MESSAGE_BEGIN(MSG_ONE, gmsgMOTD, NULL, client);
		WRITE_BYTE(*pFileList ? FALSE : TRUE); // FALSE means there is still more message to come
		WRITE_STRING(chunk);
		MESSAGE_END();
	}
}

bool CHalfLifeMultiplay::SendHtmlMOTDFileToClient(edict_t *client, const char *file /*= nullptr*/)
{
	if (!file)
		file = (char *)CVAR_GET_STRING("motdfile_html");

	int length;
	char *aFileList = (char *)LOAD_FILE_FOR_ME(const_cast<char *>(file), &length);
	if (!aFileList)
		return false;
	SendHtmlMOTDToClient(client, aFileList);
	FREE_FILE(aFileList);
	return true;
}

void CHalfLifeMultiplay::SendHtmlMOTDToClient(edict_t *client, char *string)
{
	int char_count = 0;
	char *pFileList = string;
	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts
	while (pFileList && *pFileList && char_count < MAX_UNICODE_MOTD_LENGTH)
	{
		char chunk[MAX_MOTD_CHUNK + 1];
		UTIL_strcpy(chunk, pFileList);

		char_count += strlen(chunk);
		if (char_count < MAX_UNICODE_MOTD_LENGTH)
			pFileList = string + char_count;
		else
			*pFileList = 0;

		MESSAGE_BEGIN(MSG_ONE, gmsgHtmlMOTD, NULL, client);
		WRITE_BYTE(*pFileList ? FALSE : TRUE); // FALSE means there is still more message to come
		WRITE_STRING(chunk);
		MESSAGE_END();
	}
}

void CHalfLifeMultiplay ::ClientUserInfoChanged(CBasePlayer *pPlayer, char *infobuffer)
{
}

//=========================================================
//=========================================================
// Busters Gamerules
//=========================================================
//=========================================================

#define EGON_BUSTING_TIME 10

bool IsBustingGame()
{
	return sv_busters.value == 1;
}

bool IsPlayerBusting(CBaseEntity *pPlayer)
{
	if (!pPlayer || !pPlayer->IsPlayer() || !IsBustingGame())
		return FALSE;

	return ((CBasePlayer *)pPlayer)->HasPlayerItemFromID(WEAPON_EGON);
}

BOOL BustingCanHaveItem(CBasePlayer *pPlayer, CBaseEntity *pItem)
{
	BOOL bIsWeaponOrAmmo = FALSE;

	if (strstr(STRING(pItem->pev->classname), "weapon_") || strstr(STRING(pItem->pev->classname), "ammo_"))
	{
		bIsWeaponOrAmmo = TRUE;
	}

	//Busting players can't have ammo nor weapons
	if (IsPlayerBusting(pPlayer) && bIsWeaponOrAmmo)
		return FALSE;

	return TRUE;
}

//=========================================================
CMultiplayBusters::CMultiplayBusters()
{
	m_flEgonBustingCheckTime = -1;
}

//=========================================================
void CMultiplayBusters::Think()
{
	CheckForEgons();

	CHalfLifeMultiplay::Think();
}

//=========================================================
int CMultiplayBusters::IPointsForKill(CBasePlayer *pAttacker, CBasePlayer *pKilled)
{
	//If the attacker is busting, they get a point per kill
	if (IsPlayerBusting(pAttacker))
		return 1;

	//If the victim is busting, then the attacker gets a point
	if (IsPlayerBusting(pKilled))
		return 2;

	return 0;
}

//=========================================================
void CMultiplayBusters::PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor)
{
	if (IsPlayerBusting(pVictim))
	{
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "The Buster is dead!!");

		//Reset egon check time
		m_flEgonBustingCheckTime = -1;

		CBasePlayer *peKiller = NULL;
		CBaseEntity *ktmp = CBaseEntity::Instance(pKiller);

		if (ktmp && (ktmp->Classify() == CLASS_PLAYER))
		{
			peKiller = (CBasePlayer *)ktmp;
		}
		else if (ktmp && (ktmp->Classify() == CLASS_VEHICLE))
		{
			CBasePlayer *pDriver = ((CFuncVehicle *)ktmp)->m_pDriver;

			if (pDriver != NULL)
			{
				peKiller = pDriver;
				ktmp = pDriver;
				pKiller = pDriver->pev;
			}
		}

		if (peKiller)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s has has killed the Buster!\n", STRING((CBasePlayer *)peKiller->pev->netname)));
		}

		pVictim->pev->renderfx = kRenderFxNone;
		pVictim->pev->rendercolor = g_vecZero;
		//pVictim->pev->effects &= ~EF_BRIGHTFIELD;
	}

	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
}

//=========================================================
void CMultiplayBusters::DeathNotice(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor)
{
	//Only death notices that the Buster was involved in in Busting game mode
	if (!IsPlayerBusting(pVictim) && !IsPlayerBusting(CBaseEntity::Instance(pKiller)))
		return;

	CHalfLifeMultiplay::DeathNotice(pVictim, pKiller, pevInflictor);
}

//=========================================================
int CMultiplayBusters::WeaponShouldRespawn(CBasePlayerItem *pWeapon)
{
	if (pWeapon->m_iId == WEAPON_EGON)
		return GR_WEAPON_RESPAWN_NO;

	return CHalfLifeMultiplay::WeaponShouldRespawn(pWeapon);
}

//=========================================================
// CheckForEgons:
// Check to see if any player has an egon
// If they don't then get the lowest player on the scoreboard and give them one
// Then check to see if any weapon boxes out there has an egon, and delete it
//=========================================================
void CMultiplayBusters::CheckForEgons()
{
	if (m_flEgonBustingCheckTime <= 0.0f)
	{
		m_flEgonBustingCheckTime = gpGlobals->time + EGON_BUSTING_TIME;
		return;
	}

	if (m_flEgonBustingCheckTime <= gpGlobals->time)
	{
		m_flEgonBustingCheckTime = -1.0f;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);

			//Someone is busting, no need to continue
			if (IsPlayerBusting(pPlayer))
				return;
		}

		int bBestFrags = 9999;
		CBasePlayer *pBestPlayer = NULL;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);

			if (pPlayer && pPlayer->pev->frags <= bBestFrags)
			{
				bBestFrags = pPlayer->pev->frags;
				pBestPlayer = pPlayer;
			}
		}

		if (pBestPlayer)
		{
			pBestPlayer->GiveNamedItem("weapon_egon");

			CBaseEntity *pEntity = NULL;

			//Find a weaponbox that includes an Egon, then destroy it
			while ((pEntity = UTIL_FindEntityByClassname(pEntity, "weaponbox")) != NULL)
			{
				CWeaponBox *pWeaponBox = (CWeaponBox *)pEntity;

				if (pWeaponBox)
				{
					CBasePlayerItem *pWeapon;

					for (int i = 0; i < MAX_ITEM_TYPES; i++)
					{
						pWeapon = pWeaponBox->m_rgpPlayerItems[i];

						while (pWeapon)
						{
							//There you are, bye box
							if (pWeapon->m_iId == WEAPON_EGON)
							{
								pWeaponBox->Kill();
								break;
							}

							pWeapon = pWeapon->m_pNext;
						}
					}
				}
			}
		}
	}
}

//=========================================================
BOOL CMultiplayBusters::CanHavePlayerItem(CBasePlayer *pPlayer, CBasePlayerItem *pItem)
{
	//Buster cannot have more weapons nor ammo
	if (BustingCanHaveItem(pPlayer, pItem) == FALSE)
	{
		return FALSE;
	}

	return CHalfLifeMultiplay::CanHavePlayerItem(pPlayer, pItem);
}

//=========================================================
BOOL CMultiplayBusters::CanHaveItem(CBasePlayer *pPlayer, CItem *pItem)
{
	//Buster cannot have more weapons nor ammo
	if (BustingCanHaveItem(pPlayer, pItem) == FALSE)
	{
		return FALSE;
	}

	return CHalfLifeMultiplay::CanHaveItem(pPlayer, pItem);
}

//=========================================================
void CMultiplayBusters::PlayerGotWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon)
{
	if (pWeapon->m_iId == WEAPON_EGON)
	{
		pPlayer->RemoveAllItems(false);

		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Long live the new Buster!");
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s is busting!\n", STRING((CBasePlayer *)pPlayer->pev->netname)));

		SetPlayerModel(pPlayer);

		pPlayer->pev->health = pPlayer->pev->max_health;
		pPlayer->pev->armorvalue = 100;

		pPlayer->pev->renderfx = kRenderFxGlowShell;
		pPlayer->pev->renderamt = 25;
		pPlayer->pev->rendercolor = Vector(0, 75, 250);

		CBasePlayerWeapon *pEgon = (CBasePlayerWeapon *)pWeapon;

		pEgon->m_iDefaultAmmo = 100;
		pPlayer->m_rgAmmo[pEgon->m_iPrimaryAmmoType] = pEgon->m_iDefaultAmmo;

		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "ivan");
	}
}

void CMultiplayBusters::ClientUserInfoChanged(CBasePlayer *pPlayer, char *infobuffer)
{
	CHalfLifeMultiplay::ClientUserInfoChanged(pPlayer, infobuffer);

	SetPlayerModel(pPlayer);
}

void CMultiplayBusters::PlayerSpawn(CBasePlayer *pPlayer)
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);
	SetPlayerModel(pPlayer);
}

void CMultiplayBusters::SetPlayerModel(CBasePlayer *pPlayer)
{
	if (IsPlayerBusting(pPlayer))
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "ivan");
	}
	else
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "skeleton");
	}
}
