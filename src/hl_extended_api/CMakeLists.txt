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
	hl_extended_api.cpp
	hl_extended_api.def
	hl_extended_api.inc
)

source_group( TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCE_FILES} )

add_library( hl_extended_api SHARED ${SOURCE_FILES} )
bhl_version_info( hl_extended_api "hl_extended_api.dll" "Half-Life Extended AMXX API Module" )

target_include_directories( hl_extended_api PRIVATE
	.
	${GAME_COMMON_INCLUDE_PATHS}
	${CMAKE_SOURCE_DIR}/external/metamod/metamod
	${SOURCE_SDK_INCLUDE_PATHS}
)

target_compile_definitions( hl_extended_api PRIVATE
	${GAME_COMMON_DEFINES}
	${SOURCE_SDK_DEFINES}
	USE_METAMOD
	HAVE_STDINT_H
	MATHLIB_USE_C_ASSERT
	MATHLIB_VECTOR_NONTRIVIAL
)

if( WIN32 )
	set( HL_EXTENDED_API_PLAT_LIBS )
else()
	set( HL_EXTENDED_API_PLAT_LIBS
		dl
	)
endif()

target_link_libraries( hl_extended_api
	${HL_EXTENDED_API_PLAT_LIBS}
	Threads::Threads
)

set( SERVER_LINK_FLAGS "" )
set( SERVER_COMPILE_FLAGS "" )

set_target_properties( hl_extended_api PROPERTIES
	COMPILE_FLAGS "${SERVER_COMPILE_FLAGS}"
	LINK_FLAGS "${SHARED_GAME_LINKER_FLAGS} ${SERVER_LINK_FLAGS}"
)

set_target_properties( hl_extended_api PROPERTIES PREFIX "" )
if( NOT MSVC )
	set_target_properties( hl_extended_api PROPERTIES OUTPUT_NAME "hl_extended_api_i386" )
endif()
