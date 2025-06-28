set( TXT_UNICODE_CONVERT_REGEX "\\.utf8\\.txt$" )
set( TXT_UNICODE_CONVERT_REPLACE ".txt" )

set( CLIENT_FILES
	resource/GameMenu.res

	sound/misc/talk_bhl_fallback.wav

	sprites/ag_ctf.spr
	sprites/ag_ctf_score.spr

	ui/gfx/circle_bg.tga
	ui/gfx/circle_fg.tga
	ui/gfx/cross_bg.tga
	ui/gfx/default_avatar.tga
	ui/gfx/hud/speaker.tga
	ui/gfx/hud/speaker_self.tga
	ui/gfx/hud/ammo_357.tga
	ui/gfx/hud/ammo_9mm.tga
	ui/gfx/hud/ammo_bolt.tga
	ui/gfx/hud/ammo_buckshot.tga
	ui/gfx/hud/ammo_energy.tga
	ui/gfx/hud/ammo_grenade_frag.tga
	ui/gfx/hud/ammo_grenade_hornet.tga
	ui/gfx/hud/ammo_grenade_mp5.tga
	ui/gfx/hud/ammo_grenade_rpg.tga
	ui/gfx/hud/ammo_grenade_satchel.tga
	ui/gfx/hud/ammo_grenade_tripmine.tga
	ui/gfx/hud/ammo_snark.tga
	ui/gfx/hud/default_ammo.tga
	ui/gfx/muted_icon32.tga
	ui/gfx/vgui/800corner1.tga
	ui/gfx/vgui/800corner2.tga
	ui/gfx/vgui/800corner3.tga
	ui/gfx/vgui/800corner4.tga

	ui/resource/fonts/HemiHead426-Regular.ttf
	ui/resource/fonts/boxrocket.ttf

	ui/resource/language/bugfixedhl_english.utf8.txt
	ui/resource/language/bugfixedhl_russian.utf8.txt
	ui/resource/language/chat_english.utf8.txt
	ui/resource/language/chat_french.utf8.txt
	ui/resource/language/chat_german.utf8.txt
	ui/resource/language/chat_italian.utf8.txt
	ui/resource/language/chat_korean.utf8.txt
	ui/resource/language/chat_russian.utf8.txt
	ui/resource/language/chat_schinese.utf8.txt
	ui/resource/language/chat_spanish.utf8.txt
	ui/resource/language/chat_tchinese.utf8.txt

	ui/resource/options/AboutSubOptions.res
	ui/resource/options/ChatSubOptions.res
	ui/resource/options/ColorPicker.res
	ui/resource/options/CrosshairSubOptions.res
	ui/resource/options/GeneralSubOptions.res
	ui/resource/options/HudSubOptions.res
	ui/resource/options/HudSubOptionsColors.res
	ui/resource/options/ModelSubOptions.res
	ui/resource/options/ScoreboardSubOptions.res

	ui/resource/updater/UpdateDownloadStatusDialog.res
	ui/resource/updater/UpdateFileProgressDialog.res
	ui/resource/updater/UpdateFileReplaceDialog.res
	ui/resource/updater/UpdateNotificationDialog.res

	ui/resource/Chat.res
	ui/resource/ChatScheme.res
	ui/resource/ChatSchemeBase.res
	ui/resource/ClientScheme.res
	ui/resource/ClientSchemeBase.res
	ui/resource/ClientSourceScheme.res
	ui/resource/ClientSourceSchemeBase.res
	ui/resource/HudAmmo.res
	ui/resource/HudAmmoSecondary.res
	ui/resource/HudBattery.res
	ui/resource/HudHealth.res
	ui/resource/MOTD.res
	ui/resource/open_source_software.txt
	ui/resource/ScorePanel.res
	ui/resource/SpectatorInfoPanel.res
	ui/resource/TeamMenu.res

	ui/scripts/HudAnimations.txt
	ui/scripts/HudLayout.res
	ui/scripts/HudLayoutBase.res

	commandmenu_default.txt
)

set( SERVER_FILES
	addons/amxmodx/scripting/include/bugfixedapi.inc
	
	delta.lst
)

function(bhl_install_files component_name file_list)
	set( converted_txt_files )

	foreach( rel_file_path IN LISTS file_list)
		cmake_path( HAS_PARENT_PATH rel_file_path has_parent_path )

		if( has_parent_path)
			cmake_path( GET rel_file_path PARENT_PATH parent_path )
		else()
			set( parent_path "." )
		endif()

		if( rel_file_path MATCHES ${TXT_UNICODE_CONVERT_REGEX})
			# Convert file into UTF-16 LE for VGUI2
			string( REGEX REPLACE ${TXT_UNICODE_CONVERT_REGEX} ${TXT_UNICODE_CONVERT_REPLACE} converted_rel_file_path ${rel_file_path} )

			file( MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${parent_path}" )
			set( file_to_copy "${CMAKE_CURRENT_BINARY_DIR}/${converted_rel_file_path}" )

			add_custom_command(
				OUTPUT ${file_to_copy}
				COMMAND
					Python::Interpreter
					"${CMAKE_SOURCE_DIR}/scripts/reencode_text_file.py"
					"${CMAKE_CURRENT_SOURCE_DIR}/${rel_file_path}"
					${file_to_copy}
				DEPENDS
					"${CMAKE_SOURCE_DIR}/scripts/reencode_text_file.py"
					"${CMAKE_CURRENT_SOURCE_DIR}/${rel_file_path}"
			)

			list( APPEND converted_txt_files ${file_to_copy} )
		else()
			# Copy the file as-is
			set( file_to_copy "${CMAKE_CURRENT_SOURCE_DIR}/${rel_file_path}" )
		endif()

		install(
			FILES ${file_to_copy}
			DESTINATION ${parent_path}
			COMPONENT ${component_name}
		)
	endforeach()

	if( converted_txt_files )
		# Create custom target to convert the files
		add_custom_target( convert_utf_files_${component_name} DEPENDS ${converted_txt_files} )
	endif()

	# Install readme
	install(
		FILES ${CMAKE_SOURCE_DIR}/README.md
		DESTINATION "."
		COMPONENT ${component_name}
		RENAME README_BugfixedHL.md
	)
endfunction()


function(bhl_install_target component_name target_name install_dir)
	install(
		FILES $<TARGET_FILE:${target_name}>
		DESTINATION ${install_dir}
		COMPONENT ${component_name}
	)

	if( CMAKE_CXX_LINKER_SUPPORTS_PDB )
		install(
			FILES $<TARGET_PDB_FILE:${target_name}>
			DESTINATION ${install_dir}
			COMPONENT ${component_name}
			OPTIONAL
		)
	endif()
endfunction()

# Client
bhl_install_files( client "${CLIENT_FILES}" )
bhl_install_target( client client "cl_dlls" )
add_dependencies( client convert_utf_files_client )

# Server
bhl_install_files( server "${SERVER_FILES}" )
bhl_install_target( server server "dlls" )
bhl_install_target( server bugfixedapi_amxx "addons/amxmodx/modules" )

if( GENERATE_AMXX_OFFSETS )
	# Generate the install script
	configure_file(
		${CMAKE_SOURCE_DIR}/cmake/InstallAmxxOffsets.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/InstallAmxxOffsets.cmake
		@ONLY
	)

	# Install offset files
	install(
		SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/InstallAmxxOffsets.cmake
		COMPONENT server
	)
endif()
