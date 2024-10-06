add_sources(
	CMakeLists.txt
	bhlcfg.cpp
	bhlcfg.h
	camera.h
	cdll_int.cpp
	cl_dll.h
	cl_util.cpp
	cl_util.h
	cl_voice_status.cpp
	cl_voice_status.h
	client_steam_context.cpp
	client_steam_context.h
	client_vgui.cpp
	client_vgui.h
	com_weapons.cpp
	com_weapons.h
	console.cpp
	console.h
	demo.cpp
	demo.h
	engfuncs.cpp
	engfuncs.h
	engine_builds.h
	engine_patches.cpp
	engine_patches.h
	entity.cpp
	ev_common.cpp
	ev_hldm.cpp
	ev_hldm.h
	events.cpp
	eventscripts.h
	Exports.h
	fog.cpp
	fog.h
	GameStudioModelRenderer.cpp
	GameStudioModelRenderer.h
	global_consts.h
	hud.cpp
	hud.h
	hud_iface.h
	hud_msg.cpp
	hud_redraw.cpp
	hud_renderer.cpp
	hud_renderer.h
	hud_update.cpp
	in_camera.cpp
	in_defs.h
	input.cpp
	inputw32.cpp
	interpolation.cpp
	interpolation.h
	kbutton.h
	net.h
	opengl.cpp
	opengl.h
	pch.h
	player_info.cpp
	player_info.h
	rainbow.cpp
	rainbow.h
	results.cpp
	results.h
	sdl_rt.cpp
	sdl_rt.h
	studio_util.cpp
	studio_util.h
	StudioModelRenderer.cpp
	StudioModelRenderer.h
	svc_messages.cpp
	svc_messages.h
	tf_defs.h
	tri.cpp
	tri.h
	view.cpp
	view.h
	voice_banmgr.cpp
	voice_banmgr.h
	wrect.h
	
	# Weapons
	../server/crossbow.cpp
	../server/crowbar.cpp
	../server/egon.cpp
	../server/gauss.cpp
	../server/handgrenade.cpp
	../server/wpn_shared/hl_wpn_glock.cpp
	../server/hornetgun.cpp
	../server/mp5.cpp
	../server/python.cpp
	../server/rpg.cpp
	../server/satchel.cpp
	../server/shotgun.cpp
	../server/squeakgrenade.cpp
	../server/tripmine.cpp
	../server/util_shared.cpp
)

if( PLATFORM_WINDOWS )
	add_sources(
		engine_patches_windows.cpp
		net_windows.cpp
	)
elseif( PLATFORM_LINUX )
	add_sources(
		engine_patches_linux.cpp
		net_linux.cpp
	)
else()
	add_sources(
		engine_patches_stub.cpp
		net_stub.cpp
	)
endif()

add_subdirectory( gameui )
add_subdirectory( hl )
add_subdirectory( hud )
add_subdirectory( vgui )

if( USE_UPDATER )
	add_subdirectory( updater )
endif()
