set( SOURCE_FILES
	CMakeLists.txt
	${HL_COMMON_SRCS}
	${HL_ENGINE_SRCS}
	${HL_PUBLIC_SRCS}

	../game/server/globals.cpp
	../public/vinterface/interface.cpp

	amxxmodule.cpp
	amxxmodule.h
	moduleconfig.h
	IGameConfigs.h
	ITextParsers.h
	bugfixedapi_amxx.cpp
	bugfixedapi_amxx.def
	bhl_api.h
	bhl_api.cpp
)

source_group( TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCE_FILES} )

add_library( bugfixedapi_amxx SHARED ${SOURCE_FILES} )
bhl_version_info( bugfixedapi_amxx "bugfixedapi_amxx.dll" "BugfixedHL-Rebased AMXX Module" )

target_include_directories( bugfixedapi_amxx PRIVATE
	.
	${GAME_COMMON_INCLUDE_PATHS}
	${CMAKE_SOURCE_DIR}/external/metamod/metamod
	${SOURCE_SDK_INCLUDE_PATHS} # For mathlib
)

target_compile_definitions( bugfixedapi_amxx PRIVATE
	${GAME_COMMON_DEFINES}
	${SOURCE_SDK_DEFINES}
	USE_METAMOD
	HAVE_STDINT_H
	MATHLIB_USE_C_ASSERT
	MATHLIB_VECTOR_NONTRIVIAL
)

# Dependencies
if( WIN32 )
	set( AMXX_PLAT_LIBS )
else()
	set( AMXX_PLAT_LIBS
		dl
	)
endif()

# Link with dependencies
target_link_libraries( bugfixedapi_amxx 
	${AMXX_PLAT_LIBS}
	Threads::Threads
)

# Linker flags
set( SERVER_LINK_FLAGS "")
set( SERVER_COMPILE_FLAGS "")

set_target_properties( bugfixedapi_amxx PROPERTIES
	COMPILE_FLAGS "${SERVER_COMPILE_FLAGS}" 
	LINK_FLAGS "${SHARED_GAME_LINKER_FLAGS} ${SERVER_LINK_FLAGS}"
)

# Remove "lib" prefix
set_target_properties( bugfixedapi_amxx PROPERTIES PREFIX "" )
if ( NOT MSVC )
	# Add i386 to the file name
	set_target_properties( bugfixedapi_amxx PROPERTIES OUTPUT_NAME "bugfixedapi_amxx_i386" )
endif()
