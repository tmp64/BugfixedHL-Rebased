#if defined _hl_extended_api_included
	#endinput
#endif
#define _hl_extended_api_included

#pragma reqlib hl_extended_api
#if !defined AMXMODX_NOAUTOLOAD
	#pragma loadlib hl_extended_api
#endif

/**
 * Enables or disables the per-player weapon laser dot.
 *
 * The dot is rendered server-side as sprites/laserdot.spr along the player's
 * current aim trace, independent from the RPG laser state.
 *
 * @param id      Player index, 1..MaxClients.
 * @param enable  true to enable, false to disable.
 *
 * @return        1 on success, 0 if the player index is invalid/not in-game.
 */
native native_set_weapon_laser(id, bool:enable);

/**
 * Spawns a Half-Life NPC by classname.
 *
 * Essential stock NPC models are precached by the module during worldspawn so
 * this can be used on deathmatch maps that do not already reference monsters.
 *
 * @param classname  Entity classname, for example "monster_scientist".
 * @param origin     World origin.
 * @param angles     Entity angles.
 *
 * @return           Entindex of the spawned NPC, or 0 on failure.
 */
native native_spawn_npc(const classname[], const Float:origin[3], const Float:angles[3]);
