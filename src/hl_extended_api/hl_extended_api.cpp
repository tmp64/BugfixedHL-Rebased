#include "amxxmodule.h"

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"
#include "enginecallback.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef USE_METAMOD
extern gamedll_funcs_t *gpGamedllFuncs;
#endif

namespace
{
constexpr int MaxGoldSrcClients = 32;
constexpr float LaserTraceDistance = 8192.0f;
constexpr float NpcInfoTraceDistance = 2048.0f;
constexpr float NpcInfoInterval = 0.12f;
constexpr int LaserSpriteScale = 6;
constexpr int LaserSpriteBrightness = 255;

bool g_weaponLaserEnabled[MaxGoldSrcClients + 1]{};
float g_nextNpcInfoTime[MaxGoldSrcClients + 1]{};
int g_laserDotSprite = 0;
int g_msgStatusText = 0;
bool g_precachedForMap = false;

const char *const EssentialNpcModels[] = {
	"models/scientist.mdl",
	"models/barney.mdl",
	"models/hgrunt.mdl",
	"models/hassassin.mdl",
	"models/zombie.mdl",
	"models/headcrab.mdl",
	"models/bullsquid.mdl",
	"models/islave.mdl",
	"models/agrunt.mdl",
	"models/houndeye.mdl",
	"models/barnacle.mdl",
	"models/controller.mdl",
	"models/bigmom.mdl",
	"models/baby_headcrab.mdl",
	"models/garg.mdl",
	"models/apache.mdl"
};

bool IsValidPlayerIndex(const int id)
{
	return gpGlobals && id >= 1 && id <= gpGlobals->maxClients && id <= MaxGoldSrcClients;
}

bool IsUsablePlayer(const edict_t *player)
{
	return player && !player->free && (player->v.flags & FL_CLIENT) != 0;
}

Vector CellArrayToVector(AMX *amx, const cell amxAddress)
{
	cell *data = MF_GetAmxAddr(amx, amxAddress);
	return Vector(amx_ctof(data[0]), amx_ctof(data[1]), amx_ctof(data[2]));
}

void EnsureUserMessages()
{
	if (g_msgStatusText == 0)
		g_msgStatusText = REG_USER_MSG("StatusText", -1);
}

void PrecacheExtendedAssets()
{
	if (g_precachedForMap)
		return;

	g_laserDotSprite = PRECACHE_MODEL("sprites/laserdot.spr");

	for (const char *model : EssentialNpcModels)
		PRECACHE_MODEL(model);

	EnsureUserMessages();
	g_precachedForMap = true;
}

void ClearNpcInfo(const edict_t *player)
{
	if (g_msgStatusText <= 0 || !player)
		return;

	MESSAGE_BEGIN(MSG_ONE, g_msgStatusText, nullptr, const_cast<entvars_t *>(&player->v));
	WRITE_BYTE(0);
	WRITE_STRING("");
	MESSAGE_END();
}

void SendNpcInfo(edict_t *player, CBaseEntity *entity)
{
	if (!player || !entity || !entity->pev || g_msgStatusText <= 0)
		return;

	const float health = std::max(0.0f, entity->pev->health);
	const float maxHealth = entity->pev->max_health > 0.0f ? entity->pev->max_health : health;
	const char *classname = STRING(entity->pev->classname);

	char message[160];
	if (maxHealth > 0.0f)
		std::snprintf(message, sizeof(message), "NPC: %s  |  HP: %.0f / %.0f", classname, health, maxHealth);
	else
		std::snprintf(message, sizeof(message), "NPC: %s  |  HP: %.0f", classname, health);

	MESSAGE_BEGIN(MSG_ONE, g_msgStatusText, nullptr, &player->v);
	WRITE_BYTE(0);
	WRITE_STRING(message);
	MESSAGE_END();
}

void RenderLaserDot(edict_t *player)
{
	if (!player || g_laserDotSprite <= 0)
		return;

	MAKE_VECTORS(player->v.v_angle);

	const Vector start = player->v.origin + player->v.view_ofs;
	const Vector end = start + gpGlobals->v_forward * LaserTraceDistance;

	TraceResult trace;
	TRACE_LINE(start, end, dont_ignore_monsters, player, &trace);

	if (trace.flFraction >= 1.0f)
		return;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, trace.vecEndPos);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(trace.vecEndPos.x);
	WRITE_COORD(trace.vecEndPos.y);
	WRITE_COORD(trace.vecEndPos.z);
	WRITE_SHORT(g_laserDotSprite);
	WRITE_BYTE(LaserSpriteScale);
	WRITE_BYTE(LaserSpriteBrightness);
	MESSAGE_END();
}

void UpdateNpcInfo(edict_t *player, const int id)
{
	if (!player || gpGlobals->time < g_nextNpcInfoTime[id])
		return;

	g_nextNpcInfoTime[id] = gpGlobals->time + NpcInfoInterval;

	MAKE_VECTORS(player->v.v_angle);

	const Vector start = player->v.origin + player->v.view_ofs;
	const Vector end = start + gpGlobals->v_forward * NpcInfoTraceDistance;

	TraceResult trace;
	TRACE_LINE(start, end, dont_ignore_monsters, player, &trace);

	if (trace.flFraction >= 1.0f || FNullEnt(trace.pHit))
	{
		ClearNpcInfo(player);
		return;
	}

	CBaseEntity *hit = CBaseEntity::Instance(trace.pHit);
	if (!hit || !hit->pev || !hit->MyMonsterPointer() || !hit->IsAlive())
	{
		ClearNpcInfo(player);
		return;
	}

	SendNpcInfo(player, hit);
}

bool IsAllowedNpcClassname(const char *classname)
{
	if (!classname || std::strncmp(classname, "monster_", 8) != 0)
		return false;

	constexpr const char *BlockedClassnames[] = {
		"monster_generic",
		"monster_furniture"
	};

	for (const char *blocked : BlockedClassnames)
	{
		if (std::strcmp(classname, blocked) == 0)
			return false;
	}

	return true;
}

edict_t *SpawnGameEntity(const char *classname, const Vector &origin, const Vector &angles)
{
	if (!IsAllowedNpcClassname(classname))
		return nullptr;

	edict_t *entity = CREATE_NAMED_ENTITY(ALLOC_STRING(classname));
	if (FNullEnt(entity))
		return nullptr;

	entity->v.origin = origin;
	entity->v.angles = angles;
	entity->v.spawnflags &= ~SF_NORESPAWN;

#ifdef MDLL_Spawn
	const int spawnResult = MDLL_Spawn(entity);
#else
	if (!gpGamedllFuncs || !gpGamedllFuncs->dllapi_table || !gpGamedllFuncs->dllapi_table->pfnSpawn)
	{
		REMOVE_ENTITY(entity);
		return nullptr;
	}

	const int spawnResult = gpGamedllFuncs->dllapi_table->pfnSpawn(entity);
#endif

	if (spawnResult < 0 || FNullEnt(entity) || entity->free)
		return nullptr;

	CBaseEntity *baseEntity = CBaseEntity::Instance(entity);
	if (!baseEntity || !baseEntity->MyMonsterPointer())
	{
		REMOVE_ENTITY(entity);
		return nullptr;
	}

	return entity;
}
}

// native native_set_weapon_laser(id, bool:enable)
static cell AMX_NATIVE_CALL native_set_weapon_laser(AMX *amx, cell *params)
{
	const int id = params[1];
	if (!IsValidPlayerIndex(id) || !MF_IsPlayerIngame(id))
		return 0;

	g_weaponLaserEnabled[id] = params[2] != 0;
	return 1;
}

// native native_spawn_npc(const classname[], const Float:origin[3], const Float:angles[3])
static cell AMX_NATIVE_CALL native_spawn_npc(AMX *amx, cell *params)
{
	int length = 0;
	const char *classname = MF_GetAmxString(amx, params[1], 0, &length);
	if (!classname || length <= 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "classname must not be empty");
		return 0;
	}

	PrecacheExtendedAssets();

	const Vector origin = CellArrayToVector(amx, params[2]);
	const Vector angles = CellArrayToVector(amx, params[3]);

	edict_t *entity = SpawnGameEntity(classname, origin, angles);
	if (!entity)
		return 0;

	return ENTINDEX(entity);
}

AMX_NATIVE_INFO hl_extended_api_Exports[] = {
	{ "native_set_weapon_laser", native_set_weapon_laser },
	{ "native_spawn_npc", native_spawn_npc },
	{ nullptr, nullptr }
};

void OnAmxxAttach()
{
	MF_AddNatives(hl_extended_api_Exports);
	LOG_MESSAGE(PLID, "HL Extended API AMXX module loaded.");
}

void OnAmxxDetach()
{
	for (bool &enabled : g_weaponLaserEnabled)
		enabled = false;

	for (float &nextTime : g_nextNpcInfoTime)
		nextTime = 0.0f;

	g_laserDotSprite = 0;
	g_msgStatusText = 0;
	g_precachedForMap = false;
}

int DispatchSpawn(edict_t *entity)
{
	if (entity && !FNullEnt(entity))
	{
		const char *classname = STRING(entity->v.classname);
		if (classname && std::strcmp(classname, "worldspawn") == 0)
		{
			g_precachedForMap = false;
			PrecacheExtendedAssets();
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void PlayerPreThink_Post(edict_t *player)
{
	if (!IsUsablePlayer(player))
		RETURN_META(MRES_IGNORED);

	const int id = ENTINDEX(player);
	if (!IsValidPlayerIndex(id) || !MF_IsPlayerIngame(id) || !MF_IsPlayerAlive(id))
		RETURN_META(MRES_IGNORED);

	if (g_weaponLaserEnabled[id])
		RenderLaserDot(player);

	UpdateNpcInfo(player, id);

	RETURN_META(MRES_IGNORED);
}
