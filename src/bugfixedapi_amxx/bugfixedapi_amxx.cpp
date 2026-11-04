#include <cstdarg>
#include <cstdio>
#include <type_traits>
#include <utility>
#include "amxxmodule.h"
#include "bhl_api.h"
#include <ScoreboardProtocol.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

// native bhl_is_api_ready()
static cell AMX_NATIVE_CALL bhl_is_api_ready(AMX *amx, cell *params)
{
	return bhl::IsServerApiReady();
}

// native bhl_get_server_version(&major, &minor, &patch);
static cell AMX_NATIVE_CALL bhl_get_server_version(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;

	cell *addr[3];
	addr[0] = MF_GetAmxAddr(amx, params[1]);
	addr[1] = MF_GetAmxAddr(amx, params[2]);
	addr[2] = MF_GetAmxAddr(amx, params[3]);

	const IGameVersion *ver = bhl::serverapi()->GetServerVersion();
	ver->GetVersion(*addr[0], *addr[1], *addr[2]);
	return 1;
}

// native bhl_get_client_supports(idx)
static cell AMX_NATIVE_CALL bhl_get_client_supports(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	return (cell)bhl::serverapi()->GetClientSupports(idx);
}

// native bhl_get_color_support(idx)
static cell AMX_NATIVE_CALL bhl_get_color_support(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	return (cell)bhl::serverapi()->GetColorSupport(idx);
}

// native bhl_get_client_version(idx, &major, &minor, &patch)
static cell AMX_NATIVE_CALL bhl_get_client_version(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;

	cell *major = MF_GetAmxAddr(amx, params[2]);
	cell *minor = MF_GetAmxAddr(amx, params[3]);
	cell *patch = MF_GetAmxAddr(amx, params[4]);

	const IGameVersion *ver = bhl::serverapi()->GetClientVersion(idx);
	if (!ver)
		return 0;
	ver->GetVersion(*major, *minor, *patch);
	return 1;
}

// native bhl_is_client_modified(idx)
static cell AMX_NATIVE_CALL bhl_is_client_dirty(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	const IGameVersion *ver = bhl::serverapi()->GetClientVersion(idx);
	if (!ver)
		return 0;
	return ver->IsDirtyBuild();
}

// native bhl_get_client_version_commit(idx, buf[], size)
static cell AMX_NATIVE_CALL bhl_get_client_version_commit(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	int size = params[3];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	const IGameVersion *ver = bhl::serverapi()->GetClientVersion(idx);
	if (!ver)
		return 0;
	char buf[128];
	ver->GetCommitHash(buf, sizeof(buf));
	MF_SetAmxString(amx, params[2], buf, size);
	return 1;
}

// native bhl_is_client_version_valid(idx)
static cell AMX_NATIVE_CALL bhl_is_client_version_valid(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	return (cell)bhl::serverapi()->IsClientVersionValid(idx);
}

// native bhl_get_automatic_motd(E_MotdType:type)
static cell AMX_NATIVE_CALL bhl_get_automatic_motd(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	return (cell)bhl::serverapi()->GetAutomaticMotd(type);
}

// native bhl_set_automatic_motd(E_MotdType:type, state)
static cell AMX_NATIVE_CALL bhl_set_automatic_motd(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	bool state = params[2];
	bhl::serverapi()->SetAutomaticMotd(type, state);
	return 1;
}

// native bhl_show_motd_from_string(E_MotdType:type, idx, const str[])
static cell AMX_NATIVE_CALL bhl_show_motd_from_string(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	int idx = params[2];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;

	int length;
	const char *str = MF_GetAmxString(amx, params[3], 0, &length);
	bhl::serverapi()->ShowMotdFromString(type, idx, str);
	return 1;
}

// native bhl_show_motd_from_file(E_MotdType:type, idx, const file[])
static cell AMX_NATIVE_CALL bhl_show_motd_from_file(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	bhl::E_MotdType type = static_cast<bhl::E_MotdType>(params[1]);
	int idx = params[2];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;

	int length;
	const char *str = MF_GetAmxString(amx, params[3], 0, &length);
	bhl::serverapi()->ShowMotdFromFile(type, idx, str);
	return 1;
}

// native bhl_set_player_score(idx, frags, deaths);
static cell AMX_NATIVE_CALL bhl_set_player_score(AMX *amx, cell *params)
{
	if (!bhl::IsServerApiReady())
		return 0;
	int idx = params[1];
	if (idx < 0 || idx > gpGlobals->maxClients)
		return 0;
	bhl::serverapi()->SetPlayerScore(idx, params[2], params[3]);
	return 1;
}

static int g_iClientCapsChangedFwd = -1;
static bhl::E_ClientCaps g_ClientCaps[32 + 1];
static bool g_bScoreboardErrorLogged = false;

extern AMX_NATIVE_INFO bugfixedapi_Exports[];

//! Returns the file name of the plugin that owns amx, for log messages
static const char *GetPluginName(AMX *amx)
{
	const char *path = MF_GetScriptName(MF_FindScriptByAmx(amx));

	if (!path)
		return "unknown";

	const char *name = path;

	for (const char *c = path; *c; c++)
	{
		if (*c == '/' || *c == '\\')
			name = c + 1;
	}

	return name;
}

/**
 * Returns true if the server has the scoreboard and HUD extension.
 * Otherwise logs once per map, with the name native is registered under.
 * Errors are logged with MF_Log: MF_LogError would abort the plugin's callback.
 */
static bool CheckScoreboardApi(AMX *amx, AMX_NATIVE native)
{
	if (bhl::IsScoreboardApiReady())
		return true;

	if (!g_bScoreboardErrorLogged)
	{
		const AMX_NATIVE_INFO *info = bugfixedapi_Exports;

		while (info->func != native)
			info++;

		if (bhl::IsServerApiReady())
		{
			int major = 0, minor = 0;
			bhl::GetServerApiVersion(major, minor);
			MF_Log("%s: server API %d.%d doesn't support the scoreboard and HUD extension (requires %d.%d). bhl_sb_* and bhl_hud_* natives will not function (plugin \"%s\").", info->name, major, minor, IBUGFIXEDSERVER_MAJOR, bhl::SCOREBOARD_API_MINOR, GetPluginName(amx));
		}
		else
		{
			MF_Log("%s: server API is not available. bhl_sb_* and bhl_hud_* natives will not function (plugin \"%s\").", info->name, GetPluginName(amx));
		}

		g_bScoreboardErrorLogged = true;
	}

	return false;
}

//! Logs one line for a bhl_hud_* call that failed. Returns 0, the result of the failed native.
static cell LogHudError(AMX *amx, const char *native, const char *key, const char *fmt, ...)
{
	char reason[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(reason, sizeof(reason), fmt, args);
	va_end(args);

	MF_Log("%s: %s (element \"%s\", plugin \"%s\").", native, reason, key, GetPluginName(amx));
	return 0;
}

//! Converts param Pos + 1 of a native to argument Pos of an IScoreboardExt method; a string uses the AMXX buffer Pos
template <typename T, int Pos>
static T GetMethodArg(AMX *amx, cell *params)
{
	if constexpr (std::is_same_v<T, const char *>)
	{
		static_assert(Pos < 4, "AMXX has 4 string buffers");
		return MF_GetAmxString(amx, params[Pos + 1], Pos, nullptr);
	}
	else
	{
		static_assert(std::is_integral_v<T>, "only strings and integers are forwarded, a float needs amx_ctof");
		return static_cast<T>(params[Pos + 1]);
	}
}

//! Calls the method with the params of the native in order; a void method returns 1
template <typename R, typename... Args, int... Pos>
static cell CallMethod(AMX *amx, cell *params, bhl::IScoreboardExt *sb, R (bhl::IScoreboardExt::*method)(Args...), std::integer_sequence<int, Pos...>)
{
	if constexpr (std::is_void_v<R>)
	{
		(sb->*method)(GetMethodArg<Args, Pos>(amx, params)...);
		return 1;
	}
	else
	{
		return (cell)(sb->*method)(GetMethodArg<Args, Pos>(amx, params)...);
	}
}

template <typename R, typename... Args>
static cell CallMethod(AMX *amx, cell *params, bhl::IScoreboardExt *sb, R (bhl::IScoreboardExt::*method)(Args...))
{
	return CallMethod(amx, params, sb, method, std::make_integer_sequence<int, sizeof...(Args)>());
}

/**
 * Calls a bhl_sb_* or bhl_hud_* native, or returns FailValue if CheckScoreboardApi fails.
 * Native is a function (AMX *, cell *, bhl::IScoreboardExt *) or an IScoreboardExt method that takes the params in order.
 */
template <auto Native, cell FailValue = 0>
static cell AMX_NATIVE_CALL ScoreboardNative(AMX *amx, cell *params)
{
	if (!CheckScoreboardApi(amx, ScoreboardNative<Native, FailValue>))
		return FailValue;

	bhl::IScoreboardExt *sb = bhl::serverapi()->GetScoreboard();

	if constexpr (std::is_member_function_pointer_v<decltype(Native)>)
		return CallMethod(amx, params, sb, Native);
	else
		return Native(amx, params, sb);
}

// native BhlClientCaps:bhl_get_client_caps(idx)
static cell AMX_NATIVE_CALL bhl_get_client_caps(AMX *amx, cell *params)
{
	if (!bhl::IsScoreboardApiReady())
		return 0;
	int idx = params[1];
	if (idx < 1 || idx > gpGlobals->maxClients)
		return 0;
	return (cell)bhl::serverapi()->GetClientCaps(idx);
}

// native bool:bhl_sb_available()
static cell AMX_NATIVE_CALL bhl_sb_available(AMX *amx, cell *params)
{
	return bhl::IsScoreboardApiReady();
}

// native bool:bhl_hud_add(const key[], BhlHudKind:kind, Float:x = -1.0, Float:y = -1.0, BhlHudFont:font = BHL_HUD_FONT_TEXT, BhlHudAlign:align = BHL_HUD_AUTO, BhlHudFlags:flags = BHL_HUD_DEFAULT)
static cell bhl_hud_add(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	const char *key = MF_GetAmxString(amx, params[1], 0, nullptr);
	int kind = params[2];

	if (!sb->HudAdd(key, kind, amx_ctof(params[3]), amx_ctof(params[4]), params[5], params[6], static_cast<unsigned int>(params[7])))
		return LogHudError(amx, __func__, key, "invalid key or kind %d, or all %d elements are used", kind, bhl::sb::MAX_ELEMENTS);

	return 1;
}

// native bool:bhl_hud_place(const key[], BhlHudPlace:place, const parent[] = "", gap = 0)
static cell bhl_hud_place(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	const char *key = MF_GetAmxString(amx, params[1], 0, nullptr);
	const char *parent = MF_GetAmxString(amx, params[3], 1, nullptr);
	int place = params[2];

	if (!sb->HudSetPlace(key, place, parent, params[4]))
		return LogHudError(amx, __func__, key, "unknown element or place %d, or invalid parent key \"%s\"", place, parent);

	return 1;
}

// native bool:bhl_hud_color(const key[], r = BHL_HUD_COLOR, g = 0, b = 0, a = 255)
static cell bhl_hud_color(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	const char *key = MF_GetAmxString(amx, params[1], 0, nullptr);

	if (!sb->HudSetColor(key, params[2], params[3], params[4], params[5]))
		return LogHudError(amx, __func__, key, "unknown element");

	return 1;
}

// native bool:bhl_hud_bind(const key[], const value[] = "", const list[] = "", const select[] = "")
static cell bhl_hud_bind(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	const char *key = MF_GetAmxString(amx, params[1], 0, nullptr);
	const char *value = MF_GetAmxString(amx, params[2], 1, nullptr);
	const char *list = MF_GetAmxString(amx, params[3], 2, nullptr);
	const char *select = MF_GetAmxString(amx, params[4], 3, nullptr);

	if (!sb->HudBind(key, value, list, select))
		return LogHudError(amx, __func__, key, "unknown element, invalid value \"%s\", list \"%s\" or select \"%s\", or the element would exceed %d bytes", value, list, select, bhl::sb::MAX_MSG_SIZE);

	return 1;
}

// native bool:bhl_hud_param(const key[], BhlHudParam:param, value)
static cell bhl_hud_param(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	const char *key = MF_GetAmxString(amx, params[1], 0, nullptr);
	int param = params[2];

	if (!sb->HudSetParam(key, param, params[3]))
		return LogHudError(amx, __func__, key, "unknown element, its kind has no param %d, or the element would exceed %d bytes", param, bhl::sb::MAX_MSG_SIZE);

	return 1;
}

// native bool:bhl_hud_text(player, const key[], variant, const fmt[], any:...)
static cell bhl_hud_text(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	int player = params[1];
	int variant = params[3];
	int textLength = 0;
	const char *text = MF_FormatAmxString(amx, params, 4, &textLength);

	// AMXX has reported the format error, the buffer holds a partial text
	if (amx->error != AMX_ERR_NONE)
		return 0;

	const char *key = MF_GetAmxString(amx, params[2], 0, nullptr);

	// A cut template could end inside a placeholder, so it is refused instead
	if (textLength > bhl::sb::MAX_TEMPLATE_LEN)
		return LogHudError(amx, __func__, key, "the text for player %d, variant %d has %d bytes, more than %d", player, variant, textLength, bhl::sb::MAX_TEMPLATE_LEN);

	if (!sb->HudSetText(player, key, variant, text))
		return LogHudError(amx, __func__, key, "unknown element, variant %d out of range, or player %d out of range or not connected", variant, player);

	return 1;
}

// native bool:bhl_hud_show(player, const key[], bool:show = true)
static cell bhl_hud_show(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	const char *key = MF_GetAmxString(amx, params[2], 0, nullptr);
	int player = params[1];

	if (!sb->HudShow(player, key, params[3] != 0))
		return LogHudError(amx, __func__, key, "unknown element, or player %d out of range or not connected", player);

	return 1;
}

// native bool:bhl_hud_slot_color(const key[], slot, r, g, b, a = 255)
static cell bhl_hud_slot_color(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	const char *key = MF_GetAmxString(amx, params[1], 0, nullptr);
	int slot = params[2];

	if (!sb->HudSetSlotColor(key, slot, params[3], params[4], params[5], params[6]))
		return LogHudError(amx, __func__, key, "unknown element, slot %d out of range, or the element would exceed %d bytes", slot, bhl::sb::MAX_MSG_SIZE);

	return 1;
}

// native bool:bhl_hud_event(player, const key[], variant, life_ms = 0, const sprite[] = "", const sprite_text[] = "", any:...)
static cell bhl_hud_event(AMX *amx, cell *params, bhl::IScoreboardExt *sb)
{
	constexpr int FIXED_PARAMS = 6;

	const char *key = MF_GetAmxString(amx, params[2], 0, nullptr);
	const char *sprite = MF_GetAmxString(amx, params[5], 1, nullptr);
	const char *spriteText = MF_GetAmxString(amx, params[6], 2, nullptr);
	int player = params[1];
	int variant = params[3];
	int argCount = (int)(params[0] / sizeof(cell)) - FIXED_PARAMS;
	int args[bhl::sb::MAX_EVENT_INTS] = {};

	if (argCount > bhl::sb::MAX_EVENT_INTS)
		return LogHudError(amx, __func__, key, "%d numbers, more than %d", argCount, bhl::sb::MAX_EVENT_INTS);

	// Variadic args are passed by reference
	for (int i = 0; i < argCount; i++)
		args[i] = *MF_GetAmxAddr(amx, params[FIXED_PARAMS + 1 + i]);

	if (!sb->HudEvent(player, key, variant, params[4], args, argCount, sprite, spriteText))
		return LogHudError(amx, __func__, key, "unknown element or not a feed, variant %d out of range, or player %d out of range or not connected", variant, player);

	return 1;
}

AMX_NATIVE_INFO bugfixedapi_Exports[] = {
	{ "bhl_is_api_ready", bhl_is_api_ready },
	{ "bhl_get_server_version", bhl_get_server_version },
	{ "bhl_get_client_supports", bhl_get_client_supports },
	{ "bhl_get_color_support", bhl_get_color_support },
	{ "bhl_get_client_version", bhl_get_client_version },
	{ "bhl_is_client_dirty", bhl_is_client_dirty },
	{ "bhl_get_client_version_commit", bhl_get_client_version_commit },
	{ "bhl_is_client_version_valid", bhl_is_client_version_valid },
	{ "bhl_get_automatic_motd", bhl_get_automatic_motd },
	{ "bhl_set_automatic_motd", bhl_set_automatic_motd },
	{ "bhl_show_motd_from_string", bhl_show_motd_from_string },
	{ "bhl_show_motd_from_file", bhl_show_motd_from_file },
	{ "bhl_set_player_score", bhl_set_player_score },
	{ "bhl_sb_team", ScoreboardNative<&bhl::IScoreboardExt::GetTeamTarget, -1> },
	{ "bhl_get_client_caps", bhl_get_client_caps },
	{ "bhl_sb_available", bhl_sb_available },
	{ "bhl_sb_reset", ScoreboardNative<&bhl::IScoreboardExt::Reset> },
	{ "bhl_sb_config", ScoreboardNative<&bhl::IScoreboardExt::SetConfig> },
	{ "bhl_sb_add_column", ScoreboardNative<&bhl::IScoreboardExt::AddColumn> },
	{ "bhl_sb_add_badge", ScoreboardNative<&bhl::IScoreboardExt::AddBadge> },
	{ "bhl_sb_set_int", ScoreboardNative<&bhl::IScoreboardExt::SetInt> },
	{ "bhl_sb_set_text", ScoreboardNative<&bhl::IScoreboardExt::SetText> },
	{ "bhl_sb_set_icon", ScoreboardNative<&bhl::IScoreboardExt::SetIcon> },
	{ "bhl_sb_set_progress", ScoreboardNative<&bhl::IScoreboardExt::SetProgress> },
	{ "bhl_sb_set_clock", ScoreboardNative<&bhl::IScoreboardExt::SetClock> },
	{ "bhl_sb_clear", ScoreboardNative<&bhl::IScoreboardExt::Clear> },
	{ "bhl_sb_list_resize", ScoreboardNative<&bhl::IScoreboardExt::ListResize> },
	{ "bhl_sb_list_set_int", ScoreboardNative<&bhl::IScoreboardExt::ListSetInt> },
	{ "bhl_sb_list_set_text", ScoreboardNative<&bhl::IScoreboardExt::ListSetText> },
	{ "bhl_sb_list_set_icon", ScoreboardNative<&bhl::IScoreboardExt::ListSetIcon> },
	{ "bhl_hud_add", ScoreboardNative<bhl_hud_add> },
	{ "bhl_hud_place", ScoreboardNative<bhl_hud_place> },
	{ "bhl_hud_color", ScoreboardNative<bhl_hud_color> },
	{ "bhl_hud_bind", ScoreboardNative<bhl_hud_bind> },
	{ "bhl_hud_param", ScoreboardNative<bhl_hud_param> },
	{ "bhl_hud_text", ScoreboardNative<bhl_hud_text> },
	{ "bhl_hud_show", ScoreboardNative<bhl_hud_show> },
	{ "bhl_hud_remove", ScoreboardNative<&bhl::IScoreboardExt::HudRemove> },
	{ "bhl_hud_hide_std", ScoreboardNative<&bhl::IScoreboardExt::HudHideStd> },
	{ "bhl_hud_slot_color", ScoreboardNative<bhl_hud_slot_color> },
	{ "bhl_hud_event", ScoreboardNative<bhl_hud_event> },
	{ nullptr, nullptr }
};

void OnAmxxAttach()
{
	MF_AddNatives(bugfixedapi_Exports);

	bhl::E_ApiInitResult res = bhl::InitServerApi();

	if (res != bhl::E_ApiInitResult::OK)
	{
		LOG_ERROR(PLID, "BugfixedAPI errors:");
		// Print error details
		if (res == bhl::E_ApiInitResult::ModuleNotFound)
		{
			LOG_ERROR(PLID, "Failed to load server library (Sys_LoadModule returned nullptr).");
#ifndef _WIN32
			LOG_ERROR(PLID, "Last dlerror(): %s", dlerror());
#endif
		}
		else if (res == bhl::E_ApiInitResult::FactoryNotFound)
		{
			LOG_ERROR(PLID, "No factory in server library (Sys_GetFactory returned nullptr).");
		}
		else if (res == bhl::E_ApiInitResult::InterfaceNotFound)
		{
			LOG_ERROR(PLID, "Server factory doesn't provide interface %s.", IBUGFIXEDSERVER_NAME);
		}
		else if (res == bhl::E_ApiInitResult::VersionMismatch)
		{
			int major = 0, minor = 0;
			bhl::GetServerApiVersion(major, minor);
			LOG_ERROR(PLID, "Interface version mismatch.");
			LOG_ERROR(PLID, "\tGot:      %d.%d", major, minor);
			LOG_ERROR(PLID, "\tRequired: %d.x", IBUGFIXEDSERVER_MAJOR);
		}

		// Print final message
		LOG_ERROR(PLID, "********************************************************");
		LOG_ERROR(PLID, "Failed to get IBugfixedServer from server library.");
		LOG_ERROR(PLID, "Check log above for details.");
		LOG_ERROR(PLID, "All bhl_ AMXX natives will not function.");
		LOG_ERROR(PLID, "********************************************************");
		return;
	}

	LOG_MESSAGE(PLID, "Got IBugfixedServer from server library.");
	LOG_MESSAGE(PLID, "BugfixedAPI AMXX module loaded successfully.");

	if (!bhl::IsScoreboardApiReady())
	{
		int major = 0, minor = 0;
		bhl::GetServerApiVersion(major, minor);
		LOG_MESSAGE(PLID, "Server API %d.%d doesn't support the scoreboard and HUD extension (requires %d.%d). bhl_sb_* and bhl_hud_* natives will not function.", major, minor, IBUGFIXEDSERVER_MAJOR, bhl::SCOREBOARD_API_MINOR);
	}
}

void OnAmxxDetach()
{
	bhl::ShutdownServerApi();
}

void OnPluginsLoaded()
{
	g_iClientCapsChangedFwd = MF_RegisterForward("bhl_client_caps_changed", ET_IGNORE, FP_CELL, FP_DONE);
	g_bScoreboardErrorLogged = false;
}

void StartFrame()
{
	if (bhl::IsScoreboardApiReady())
	{
		int maxClients = std::min(gpGlobals->maxClients, (int)ARRAYSIZE(g_ClientCaps) - 1);
		for (int i = 1; i <= maxClients; i++)
		{
			bhl::E_ClientCaps caps = bhl::serverapi()->GetClientCaps(i);
			if (caps == g_ClientCaps[i])
				continue;

			g_ClientCaps[i] = caps;

			if (MF_IsPlayerIngame(i))
				MF_ExecuteForward(g_iClientCapsChangedFwd, static_cast<cell>(i));
		}
	}

	RETURN_META(MRES_IGNORED);
}
