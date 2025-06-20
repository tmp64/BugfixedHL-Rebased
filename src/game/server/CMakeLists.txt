set( TARGET_NAME server )

set( SOURCE_FILES
	${GAME_COMMON_SRCS}
	CMakeLists.txt

	../../public/vinterface/interface.cpp
	../../public/vinterface/interface.h

	wpn_shared/hl_wpn_glock.cpp
	
	activity.h
	activitymap.h
	aflock.cpp
	agrunt.cpp
	airtank.cpp
	AmxxOffsetClassList.txt
	animating.cpp
	animation.cpp
	animation.h
	apache.cpp
	barnacle.cpp
	barney.cpp
	basemonster.h
	bigmomma.cpp
	bloater.cpp
	bmodels.cpp
	bullsquid.cpp
	buttons.cpp
	cbase.cpp
	cbase.h
	CBugfixedServer.cpp
	CBugfixedServer.h
	cdll_dll.h
	client.cpp
	client.h
	com_utils.cpp
	com_utils.h
	combat.cpp
	controller.cpp
	crossbow.cpp
	crowbar.cpp
	decals.h
	defaultai.cpp
	defaultai.h
	doors.cpp
	doors.h
	effects.cpp
	effects.h
	egon.cpp
	enginecallback.h
	explode.cpp
	explode.h
	extdll.h
	flyingmonster.cpp
	flyingmonster.h
	func_break.cpp
	func_break.h
	func_tank.cpp
	game.cpp
	gamerules.cpp
	gamerules.h
	gargantua.cpp
	gauss.cpp
	genericmonster.cpp
	ggrenade.cpp
	globals.cpp
	gman.cpp
	h_ai.cpp
	h_battery.cpp
	h_cine.cpp
	h_cycler.cpp
	h_export.cpp
	handgrenade.cpp
	hassassin.cpp
	headcrab.cpp
	healthkit.cpp
	hgrunt.cpp
	hornet.cpp
	hornet.h
	hornetgun.cpp
	houndeye.cpp
	ichthyosaur.cpp
	islave.cpp
	items.cpp
	items.h
	leech.cpp
	lights.cpp
	maprules.cpp
	monsterevent.h
	monstermaker.cpp
	monsters.cpp
	monsters.h
	monsterstate.cpp
	mortar.cpp
	mp5.cpp
	multiplay_gamerules.cpp
	nihilanth.cpp
	nodes.cpp
	nodes.h
	observer.cpp
	osprey.cpp
	path.cpp
	path.h
	pathcorner.cpp
	pch.h
	plane.cpp
	plane.h
	plats.cpp
	player.cpp
	player.h
	python.cpp
	rat.cpp
	roach.cpp
	rpg.cpp
	satchel.cpp
	saverestore.h
	schedule.cpp
	schedule.h
	scientist.cpp
	scripted.cpp
	scripted.h
	scriptevent.h
	shotgun.cpp
	singleplay_gamerules.cpp
	skill.cpp
	skill.h
	sound.cpp
	soundent.cpp
	soundent.h
	spectator.cpp
	spectator.h
	squadmonster.cpp
	squadmonster.h
	squeakgrenade.cpp
	subs.cpp
	talkmonster.cpp
	talkmonster.h
	teamplay_gamerules.cpp
	teamplay_gamerules.h
	tempmonster.cpp
	tentacle.cpp
	trains.h
	triggers.cpp
	tripmine.cpp
	turret.cpp
	util.cpp
	util.h
	util_shared.cpp
	vehicle.cpp
	voice_gamemgr.cpp
	voice_gamemgr.h
	weapons.cpp
	weapons.h
	world.cpp
	xen.cpp
	zombie.cpp
)

if( PLATFORM_WINDOWS )
	set( SOURCE_FILES
		${SOURCE_FILES}
		hl.def
		resource.h
	)
endif()

source_group( TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCE_FILES} )

add_library( ${TARGET_NAME} SHARED ${SOURCE_FILES} )
add_auto_deploy( ${TARGET_NAME} server PublishPathServer.txt )
bhl_version_info( ${TARGET_NAME} "hl.dll" "BugfixedHL-Rebased Server" )

target_include_directories( ${TARGET_NAME} PRIVATE
	.
	${GAME_COMMON_INCLUDE_PATHS}
	${SOURCE_SDK_INCLUDE_PATHS} # For mathlib
)

target_compile_definitions( ${TARGET_NAME} PRIVATE
	${GAME_COMMON_DEFINES}
	${SOURCE_SDK_DEFINES}
	SERVER_DLL
	MATHLIB_USE_C_ASSERT
	MATHLIB_VECTOR_NONTRIVIAL
)

target_precompile_headers( ${TARGET_NAME} PRIVATE pch.h )

# Dependencies
if( WIN32 )
	set( SERVER_PLAT_LIBS )
else()
	set( SERVER_PLAT_LIBS
		dl
	)
endif()

# Link with dependencies
target_link_libraries( ${TARGET_NAME} 
	${SERVER_PLAT_LIBS}
	semver
	Threads::Threads
)

# Linker flags
set( SERVER_LINK_FLAGS "")
set( SERVER_COMPILE_FLAGS "")

set_target_properties( ${TARGET_NAME} PROPERTIES
	COMPILE_FLAGS "${SERVER_COMPILE_FLAGS}" 
	LINK_FLAGS "${SHARED_GAME_LINKER_FLAGS} ${SERVER_LINK_FLAGS}"
)

# Remove "lib" prefix
set_target_properties( ${TARGET_NAME} PROPERTIES PREFIX "" )

# Set the original name
set_target_properties( ${TARGET_NAME} PROPERTIES OUTPUT_NAME "hl" )

if( GENERATE_AMXX_OFFSETS )
	set( AMXX_OFFSET_GENERATOR_VERSION "1.0.0" )
	set( AMXX_OFFSET_GENERATOR_DIR "${CMAKE_BINARY_DIR}/amxx-offset-generator" )
	set( AMXX_OFFSETS_SCRIPT "${AMXX_OFFSET_GENERATOR_DIR}/create_amxx_files.py" )
	set( AMXX_OFFSETS_CLASS_LIST "${CMAKE_CURRENT_SOURCE_DIR}/AmxxOffsetClassList.txt" )
	set( AMXX_OFFSETS_OUT "${CMAKE_BINARY_DIR}/amxx-offsets" )

	# Check if platform is supported
	if( PLATFORM_WINDOWS )
		set( AMXX_OFFSETS_EXEC "${AMXX_OFFSET_GENERATOR_DIR}/win64/OffsetExporter.Pdb.exe" )
		set( AMXX_OFFSETS_DBG_FILE $<TARGET_PDB_FILE:server> )
		set( AMXX_OFFSETS_DBG_FILE_ARG --pdb )
		set( AMXX_OFFSETS_JSON "${CMAKE_BINARY_DIR}/bhl-amxx-offsets-windows.json" )
		set( AMXX_OFFSETS_SCRIPT_ARG "--windows=${AMXX_OFFSETS_JSON}" )
	elseif( PLATFORM_LINUX )
		set( AMXX_OFFSETS_EXEC "${AMXX_OFFSET_GENERATOR_DIR}/linux64/OffsetExporter.Dwarf" )
		set( AMXX_OFFSETS_DBG_FILE $<TARGET_FILE:server> )
		set( AMXX_OFFSETS_DBG_FILE_ARG --so )
		set( AMXX_OFFSETS_JSON "${CMAKE_BINARY_DIR}/bhl-amxx-offsets-linux.json" )
		set( AMXX_OFFSETS_SCRIPT_ARG "--linux=${AMXX_OFFSETS_JSON}" )

		# File doesn't have +x set in the ZIP. Fix that.
		file(
			CHMOD ${AMXX_OFFSETS_EXEC}
			PERMISSIONS
				OWNER_READ OWNER_WRITE OWNER_EXECUTE
				GROUP_WRITE GROUP_EXECUTE
				WORLD_READ WORLD_EXECUTE
		)
	else()
		message( FATAL_ERROR "GENERATE_AMXX_OFFSETS is not supported on this platform" )
	endif()

	# Download the release
	FetchContent_Declare(
		amxx-offset-generator
		URL https://github.com/tmp64/amxx-offset-generator/releases/download/v${AMXX_OFFSET_GENERATOR_VERSION}/amxx-offset-generator-v${AMXX_OFFSET_GENERATOR_VERSION}.zip
		URL_HASH SHA256=a2e26ea8b94162f505f430094b26eb114fa446f2955db3396394e6c3702e6d50
		SOURCE_DIR ${AMXX_OFFSET_GENERATOR_DIR}
	)

	FetchContent_MakeAvailable( amxx-offset-generator )

	# Add command to generate the JSON file and the offset files
	add_custom_command(
		TARGET server POST_BUILD
		COMMAND
			${AMXX_OFFSETS_EXEC}
			--class-list ${AMXX_OFFSETS_CLASS_LIST}
			${AMXX_OFFSETS_DBG_FILE_ARG} ${AMXX_OFFSETS_DBG_FILE}
			--out ${AMXX_OFFSETS_JSON}
		COMMAND
			Python::Interpreter
			${AMXX_OFFSETS_SCRIPT}
			${AMXX_OFFSETS_SCRIPT_ARG}
			--out ${AMXX_OFFSETS_OUT}
			--banner "BugfixedHL-Rebased offset file. Generated using amxx-offset-generator."
			--file-prefix "bhl"
		COMMENT "Generating AMXX offsets"
	)

	# Recompile when any of that changes
	target_sources( server PRIVATE
		${AMXX_OFFSETS_EXEC}
		${AMXX_OFFSETS_CLASS_LIST}
	)
endif()
