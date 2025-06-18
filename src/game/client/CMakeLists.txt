set( SOURCE_FILES
	CMakeLists.txt

	${GAME_COMMON_SRCS}
	${SDL2_INCLUDE_FILES}

	gameui/options/colorpicker/bar_panel.cpp
	gameui/options/colorpicker/bar_panel.h
	gameui/options/colorpicker/picker_panel.cpp
	gameui/options/colorpicker/picker_panel.h
	gameui/options/colorpicker/sel_circle_panel.cpp
	gameui/options/colorpicker/sel_circle_panel.h
	gameui/options/colorpicker/texture_manager.cpp
	gameui/options/colorpicker/texture_manager.h

	gameui/options/adv_options_dialog.cpp
	gameui/options/adv_options_dialog.h
	gameui/options/color_picker.cpp
	gameui/options/color_picker.h
	gameui/options/cvar_check_button.cpp
	gameui/options/cvar_check_button.h
	gameui/options/cvar_color.cpp
	gameui/options/cvar_color.h
	gameui/options/cvar_combo_box.cpp
	gameui/options/cvar_combo_box.h
	gameui/options/cvar_slider.cpp
	gameui/options/cvar_slider.h
	gameui/options/cvar_text_entry.cpp
	gameui/options/cvar_text_entry.h
	gameui/options/options_about.cpp
	gameui/options/options_about.h
	gameui/options/options_chat.cpp
	gameui/options/options_chat.h
	gameui/options/options_crosshair.cpp
	gameui/options/options_crosshair.h
	gameui/options/options_general.cpp
	gameui/options/options_general.h
	gameui/options/options_hud_colors.cpp
	gameui/options/options_hud_colors.h
	gameui/options/options_hud_root.cpp
	gameui/options/options_hud_root.h
	gameui/options/options_hud.cpp
	gameui/options/options_hud.h
	gameui/options/options_models.cpp
	gameui/options/options_models.h
	gameui/options/options_scoreboard.cpp
	gameui/options/options_scoreboard.h

	gameui/gameui_test_panel.cpp
	gameui/gameui_test_panel.h
	gameui/gameui_viewport.cpp
	gameui/gameui_viewport.h

	hl/hl_baseentity.cpp
	hl/hl_events.cpp
	hl/hl_objects.cpp
	hl/hl_weapons.cpp

	hud/ag/ag_countdown.cpp
	hud/ag/ag_countdown.h
	hud/ag/ag_ctf.cpp
	hud/ag/ag_ctf.h
	hud/ag/ag_global.cpp
	hud/ag/ag_global.h
	hud/ag/ag_location.cpp
	hud/ag/ag_location.h
	hud/ag/ag_longjump.cpp
	hud/ag/ag_longjump.h
	hud/ag/ag_nextmap.cpp
	hud/ag/ag_nextmap.h
	hud/ag/ag_playerid.cpp
	hud/ag/ag_playerid.h
	hud/ag/ag_settings.cpp
	hud/ag/ag_settings.h
	hud/ag/ag_sudden_death.cpp
	hud/ag/ag_sudden_death.h
	hud/ag/ag_timeout.cpp
	hud/ag/ag_timeout.h
	hud/ag/ag_vote.cpp
	hud/ag/ag_vote.h

	hud/ammo.cpp
	hud/ammo.h
	hud/ammo_secondary.cpp
	hud/ammo_secondary.h
	hud/ammohistory.cpp
	hud/ammohistory.h
	hud/base.h
	hud/base.cpp
	hud/battery.cpp
	hud/battery.h
	hud/chat.cpp
	hud/chat.h
	hud/crosshair.cpp
	hud/crosshair.h
	hud/death_notice.cpp
	hud/death_notice.h
	hud/death_notice_panel.cpp
	hud/death_notice_panel.h
	hud/flashlight.cpp
	hud/flashlight.h
	hud/geiger.cpp
	hud/geiger.h
	hud/health.cpp
	hud/health.h
	hud/jumpspeed.cpp
	hud/jumpspeed.h
	hud/menu.cpp
	hud/menu.h
	hud/message.cpp
	hud/message.h
	hud/scores.cpp
	hud/scores.h
	hud/spectator.cpp
	hud/spectator.h
	hud/speedometer.cpp
	hud/speedometer.h
	hud/status_icons.cpp
	hud/status_icons.h
	hud/statusbar.cpp
	hud/statusbar.h
	hud/strafeguide.cpp
	hud/strafeguide.h
	hud/text_message.cpp
	hud/text_message.h
	hud/timer.cpp
	hud/timer.h
	hud/train.cpp
	hud/train.h
	hud/voice_status.cpp
	hud/voice_status.h
	hud/voice_status_self.cpp
	hud/voice_status_self.h

	vgui/avatar_image.cpp
	vgui/avatar_image.h
	vgui/client_motd.cpp
	vgui/client_motd.h
	vgui/client_viewport.cpp
	vgui/client_viewport.h
	vgui/command_menu.cpp
	vgui/command_menu.h
	vgui/crosshair_image.cpp
	vgui/crosshair_image.h
	vgui/score_panel.cpp
	vgui/score_panel.h
	vgui/spectator_panel.cpp
	vgui/spectator_panel.h
	vgui/team_menu.cpp
	vgui/team_menu.h
	vgui/tga_image.cpp
	vgui/tga_image.h
	vgui/viewport_panel_names.h

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
	set( SOURCE_FILES
		${SOURCE_FILES}
		engine_patches_windows.cpp
		net_windows.cpp
	)
elseif( PLATFORM_LINUX )
	set( SOURCE_FILES
		${SOURCE_FILES}
		engine_patches_linux.cpp
		net_linux.cpp
	)
else()
	set( SOURCE_FILES
		${SOURCE_FILES}
		engine_patches_stub.cpp
		net_stub.cpp
	)
endif()

if( USE_UPDATER )
	set( SOURCE_FILES
		${SOURCE_FILES}
		updater/http_client.cpp
		updater/http_client.h
		updater/update_checker.cpp
		updater/update_checker.h
		updater/update_dialogs.cpp
		updater/update_dialogs.h
		updater/update_installer.cpp
		updater/update_installer.h
	)

	if( PLATFORM_WINDOWS )
		set( SOURCE_FILES
			${SOURCE_FILES}
			updater/update_installer_windows.cpp
		)
	else()
		set( SOURCE_FILES
			${SOURCE_FILES}
			updater/update_installer_linux.cpp
		)
	endif()
endif()

source_group( TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCE_FILES} )

add_library( client SHARED ${SOURCE_FILES} )
add_auto_deploy( client client PublishPathClient.txt )
bhl_version_info( client "client.dll" "BugfixedHL-Rebased Client" )

target_include_directories( client PRIVATE
	.
	${GAME_COMMON_INCLUDE_PATHS}
	${SOURCE_SDK_INCLUDE_PATHS}
	${UPDATER_INCLUDE_PATHS}
	${CMAKE_SOURCE_DIR}/external/SDL2/include
)

target_compile_definitions( client PRIVATE
	${GAME_COMMON_DEFINES}
	${SOURCE_SDK_DEFINES}
	${UPDATER_DEFINES}
	VERSION_SAFE_STEAM_API_INTERFACES
	CLIENT_DLL
)

target_precompile_headers( client PRIVATE pch.h )

if( WIN32 )
	set( CLIENT_PLAT_LIBS
		wsock32
		psapi
		DInput
		DXGUID
		Dbghelp
		unofficial::stackwalker::StackWalker
    )
else()
	# Only link with SDL2 on Linux. Windows will conditionally link in runtime.
	set( CLIENT_PLAT_LIBS
		dl
		SDL2
	)
endif()

# Link with dependencies
target_link_libraries( client
	${GAME_COMMON_LIBS}
	${SOURCE_SDK_LIBS}
	${UPDATER_LIBS}
	glad
	unofficial::pcre::pcre
	semver
	nlohmann_json::nlohmann_json
	${CLIENT_PLAT_LIBS}
	Threads::Threads
)

# Linker flags
set( CLIENT_LINK_FLAGS "")
set( CLIENT_COMPILE_FLAGS "")

set_target_properties( client PROPERTIES
	COMPILE_FLAGS "${CLIENT_COMPILE_FLAGS}"
	LINK_FLAGS "${GAME_LINKER_FLAGS} ${CLIENT_LINK_FLAGS}"
)

# Remove "lib" prefix
set_target_properties( client PROPERTIES PREFIX "" )
