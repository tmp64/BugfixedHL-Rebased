#
# Bugfixed and Improved Half-Life Release
#

cmake_minimum_required( VERSION 3.15.0 )

# Use static MSVC runtime
set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )

# Enable PIC
set( CMAKE_POSITION_INDEPENDENT_CODE ON )

# Disable RPATH
set( CMAKE_SKIP_BUILD_RPATH TRUE )

#-----------------------------------------------------------------
# CMake modules
#-----------------------------------------------------------------
list( APPEND CMAKE_MODULE_PATH
	"${CMAKE_CURRENT_LIST_DIR}/cmake"
)

include( InputFilesList )
include( WinXPSupport )
include( GetGitRevisionDescription )

#-----------------------------------------------------------------
# Version
#-----------------------------------------------------------------
# Also needs to be changed in scripts/BuildRelease.py
set( BHL_REPO_VERSION_MAJOR 1 )
set( BHL_REPO_VERSION_MINOR 9 )
set( BHL_REPO_VERSION_PATCH 1 )
set( BHL_REPO_VERSION_TAG "dev" )

if( NOT BHL_VERSION_MAJOR )
	set( BHL_VERSION_MAJOR ${BHL_REPO_VERSION_MAJOR} )
endif()
if( NOT BHL_VERSION_MINOR )
	set( BHL_VERSION_MINOR ${BHL_REPO_VERSION_MINOR} )
endif()
if( NOT BHL_VERSION_PATCH )
	set( BHL_VERSION_PATCH ${BHL_REPO_VERSION_PATCH} )
endif()
if( NOT BHL_VERSION_TAG )
	set( BHL_VERSION_TAG ${BHL_REPO_VERSION_TAG} )
elseif( "${BHL_VERSION_TAG}" STREQUAL "no_tag" )
	set( BHL_VERSION_TAG "" )
endif()

unset( BHL_REPO_VERSION_MAJOR )
unset( BHL_REPO_VERSION_MINOR )
unset( BHL_REPO_VERSION_PATCH )
unset( BHL_REPO_VERSION_TAG )
# Use BHL_VERSION_XXX from now on

project( BugfixedHL VERSION "${BHL_VERSION_MAJOR}.${BHL_VERSION_MINOR}.${BHL_VERSION_PATCH}" )
include( PlatformInfo )
include( CTest )

#-----------------------------------------------------------------
# Build options
#-----------------------------------------------------------------
option( AUTO_DEPLOY		"Whether to automatically deploy to deploy paths" )
option( USE_UPDATER		"Enable update checked and downloader" )
option( NO_STEAM_API	"Disable Steam API" )

#-----------------------------------------------------------------
# Compiler checks
#-----------------------------------------------------------------
if( NOT CMAKE_SIZEOF_VOID_P EQUAL 4 )
	if( NO_SIZEOF_VOID_P_CHECK )
		message( WARNING "You are not building for 32 bit systems. This is not supported." )
	else()
		set( PLAT_ERROR "" )
		
		if( CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" )
			set( PLAT_ERROR "Did you forget CMAKE_TOOLCHAIN_FILE?" )
		elseif( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
			set( PLAT_ERROR "Are you generating for Win64?" )
		else()
			set( PLAT_ERROR "Check your compiler options." )
		endif()
		
		message( FATAL_ERROR "You are not building for 32 bit systems. ${PLAT_ERROR} Use NO_SIZEOF_VOID_P_CHECK to disable this error." )
	endif()
endif()

#-----------------------------------------------------------------
# Compiler and linker settings
#-----------------------------------------------------------------
# C++14 support
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

set( C_CXX_COMPILER_FLAGS "" )			# Flags for C and C++ compilers
set( GAME_LINKER_FLAGS "" )				# Flags for Game DLL linker

if( COMPILER_GNU )
	
	#-----------------------------------------------------------------
	# Sanitizers
	#-----------------------------------------------------------------
	set( SANITIZERS "" )
	option( SANITIZE_ADDRESS, "Enable AddressSanitizer" )
	option( SANITIZE_UNDEFINED, "Enable UndefinedBehaviorSanitizer" )
	option( SANITIZE_MEMORY, "Enable MemorySanitizer" )
	
	if( SANITIZE_ADDRESS )
		set( SANITIZERS "${SANITIZERS},address" )
	endif()
	
	if( SANITIZE_UNDEFINED )
		set( SANITIZERS "${SANITIZERS},undefined" )
	endif()

	if( SANITIZE_MEMORY )
		set( SANITIZERS "${SANITIZERS},memory" )
	endif()

	if( SANITIZERS )
		string( SUBSTRING "${SANITIZERS}" 1 -1 SANITIZERS )
		message( STATUS "Enabled sanitizers: ${SANITIZERS}" )
		set( C_CXX_COMPILER_FLAGS "${C_CXX_COMPILER_FLAGS} -fsanitize=${SANITIZERS}" )
		set( GAME_LINKER_FLAGS "${GAME_LINKER_FLAGS} -fsanitize=${SANITIZERS}" )
	endif()
	
	unset( SANITIZERS )
	
	#-----------------------------------------------------------------
	# Linker options
	#-----------------------------------------------------------------
	# From the Github 2013 Makefile, match the settings for Unix environments.
	# - OLD: Force 387 for FP math so the precision between win32 and linux and osx match
	# - Use SSE2 for FP math so Source SDK mathlib compiles correctly
	# - Trigger an error if any code tries to use an implicit return type
	# - Default visibility is hidden unless explicitly altered with __attribute__( visibility() )
	set( C_CXX_COMPILER_FLAGS "${C_CXX_COMPILER_FLAGS} -mfpmath=sse -msse2 -fvisibility=hidden" )
	set( GAME_LINKER_FLAGS "${GAME_LINKER_FLAGS} -mfpmath=sse -msse2 -Werror=return-type -fvisibility=hidden" )
	
	if( PLATFORM_MACOS )
		set( GAME_LINKER_FLAGS "${GAME_LINKER_FLAGS} -momit-leaf-frame-pointer -mtune=core2" )
	endif()
	
	#-----------------------------------------------------------------
	# Additional compiler options
	#-----------------------------------------------------------------
	# Link-Time Optimizations
	option( GNU_USE_LTO "GCC/Clang: Enable Link-Time Optimizations (LTO)" )
	if( GNU_USE_LTO )
		set( C_CXX_COMPILER_FLAGS       "${C_CXX_COMPILER_FLAGS} -flto" )
		set( GAME_LINKER_FLAGS "${GAME_LINKER_FLAGS} -flto" )
	endif()
	
	# Static link with libstdc++
	option( GNU_STATIC_LIBCPP "GCC/Clang: Link statically with libstdc++" )
	if( GNU_STATIC_LIBCPP )
		set( GAME_LINKER_FLAGS "${GAME_LINKER_FLAGS} -static-libstdc++" )
	endif()

	# Colored output
	option( GNU_FORCE_COLORED_OUTPUT "Always produce ANSI-colored output" )
	if ( GNU_FORCE_COLORED_OUTPUT )
		if ( COMPILER_GCC )
			add_compile_options (-fdiagnostics-color=always)
		elseif ( COMPILER_CLANG )
			add_compile_options (-fcolor-diagnostics)
		endif ()
	endif ()
	
elseif( COMPILER_MSVC )

	# Set Windows subsystem
	set( GAME_LINKER_FLAGS "${GAME_LINKER_FLAGS} /SUBSYSTEM:WINDOWS " )
	
	# Set target OS to WinXP for XP toolsets
	if( CMAKE_VS_PLATFORM_TOOLSET MATCHES ".*_xp$" )
		add_compile_definitions( _WIN32_WINNT=0x0501 )
	endif()
endif()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_COMPILER_FLAGS}" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_COMPILER_FLAGS}" )

unset( C_CXX_COMPILER_FLAGS )

if (NOT HAS_STD_FILESYSTEM)
	if( PLATFORM_MACOS )
		# macOS 10.13 SDK doesn't have <filesystem>
		set( HAS_STD_FILESYSTEM FALSE )
		add_compile_definitions( HAS_STD_FILESYSTEM=0 )
	else()
		# MSVC 2017 and GCC 8+ support std::filesystem
		set( HAS_STD_FILESYSTEM TRUE )
		add_compile_definitions( HAS_STD_FILESYSTEM=1 )
	endif()
endif()

#-----------------------------------------------------------------
# Preprocessor definitions
#-----------------------------------------------------------------
set( COMMON_DEFINES "" )				# Preprocessor definitions for all targets

# Add platform defines to common defines
set( COMMON_DEFINES "${COMMON_DEFINES} ${PLATFORM_DEFINES}" )

if( COMPILER_GNU )
	
	set( COMMON_DEFINES
		${COMMON_DEFINES}
		_stricmp=strcasecmp
		_strnicmp=strncasecmp
		_snprintf=snprintf
		_alloca=alloca
		_vsnprintf=vsnprintf
		_snwprintf=swprintf
	)
	
elseif( COMPILER_MSVC )

	# Disable "unsafe" warnings
	set( COMMON_DEFINES
		${COMMON_DEFINES}
		_CRT_SECURE_NO_WARNINGS
		_SCL_SECURE_NO_WARNINGS
	)
	
endif()

#-----------------------------------------------------------------
# Warning level
#-----------------------------------------------------------------

option( WARNINGS_ARE_ERRORS "Treat warnings as errors" OFF )

set( WARNING_LEVEL_C "" )
set( WARNING_LEVEL_CXX "" )
set( WARNING_LEVEL_C_CXX "" )

if( COMPILER_GNU )

	set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} -Wno-write-strings -Wno-unknown-pragmas -Wno-endif-labels" )
	set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} -Wno-switch -Wno-unused-variable" )
	set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} -Wno-unused-function -Wno-unused-value" )
	set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} -Wno-pointer-arith" )
	set( WARNING_LEVEL_CXX "${WARNING_LEVEL_CXX} -Wno-delete-non-virtual-dtor -Wno-invalid-offsetof -Wno-conversion-null" )
	
	if( COMPILER_GCC )
		set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} -Wno-format-truncation -Wno-unused-but-set-variable" )
	elseif( COMPILER_CLANG )
		set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} -Wno-null-arithmetic" )
	endif()
	
	if( WARNINGS_ARE_ERRORS )
		set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} -Werror" )
	endif()
elseif( COMPILER_MSVC )
	if( WARNINGS_ARE_ERRORS )
		set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} /WX" )
	endif()
endif()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_LEVEL_C} ${WARNING_LEVEL_C_CXX}" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_LEVEL_CXX} ${WARNING_LEVEL_C_CXX}" )

#-----------------------------------------------------------------
# appversion.h generator
#-----------------------------------------------------------------
find_package( Git )
if( GIT_FOUND )
	# Get branch name
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	
	# Replace slash with hyphen
	string( REPLACE "/" "-" GIT_BRANCH ${GIT_BRANCH} )
	
	# Get last commit hash (short)
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_COMMIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	
	# Get whether worktree is dirty
	git_local_changes( GIT_DIRTY )
	if( GIT_DIRTY STREQUAL "CLEAN" )
		set( GIT_DIRTY FALSE )
	else()
		set( GIT_DIRTY TRUE )
	endif()
else()
	set( GIT_BRANCH "unknown" )
	set( GIT_COMMIT_HASH "0000000" )
	set( GIT_DIRTY FALSE )
endif()

# Assemble version string
set( BHL_VERSION_STRING "${BHL_VERSION_MAJOR}.${BHL_VERSION_MINOR}.${BHL_VERSION_PATCH}" )
if( BHL_VERSION_TAG )
	set( BHL_VERSION_STRING "${BHL_VERSION_STRING}-${BHL_VERSION_TAG}" )
endif()
set( BHL_VERSION_STRING "${BHL_VERSION_STRING}+${GIT_BRANCH}.${GIT_COMMIT_HASH}" )
if( GIT_DIRTY )
	set( BHL_VERSION_STRING "${BHL_VERSION_STRING}.m" )
endif()

# Generate version file
file( MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated" )
configure_file( "src/common/appversion.h.in" "${CMAKE_BINARY_DIR}/generated/appversion.h" )

#-----------------------------------------------------------------
# Autodeploy macro
#-----------------------------------------------------------------
if( AUTO_DEPLOY )
	macro( add_auto_deploy TARGET_NAME PUBLISH_PATHS_FILE )
		if ( WIN32 )
			add_custom_command( TARGET ${TARGET_NAME}
				POST_BUILD
				COMMAND ${CMAKE_SOURCE_DIR}/scripts/DeployLibs.bat "${CMAKE_BINARY_DIR}\\${PUBLISH_PATHS_FILE}" "$<TARGET_FILE:${TARGET_NAME}>" "$<TARGET_PDB_FILE:${TARGET_NAME}>"
			)
		else()
			add_custom_command( TARGET ${TARGET_NAME}
				POST_BUILD
				COMMAND ${CMAKE_SOURCE_DIR}/scripts/DeployLibs.sh "${CMAKE_BINARY_DIR}/${PUBLISH_PATHS_FILE}" "$<TARGET_FILE:${TARGET_NAME}>"
			)
		endif()
	endmacro()
else()
	macro( add_auto_deploy )
	endmacro()
endif()

#-----------------------------------------------------------------
# Libraries
#-----------------------------------------------------------------
set( THREADS_PREFER_PTHREAD_FLAG ON )
find_package( Threads REQUIRED )
add_subdirectory( external )

#-----------------------------------------------------------------
# Half-Life SDK
#-----------------------------------------------------------------
add_subdirectory( src/common )		# Sets HL_COMMON_SRCS
add_subdirectory( src/engine )		# Sets HL_ENGINE_SRCS
add_subdirectory( src/pm_shared )	# Sets HL_PM_SHARED_SRCS
add_subdirectory( src/public )		# Sets HL_PUBLIC_SRCS
add_subdirectory( src/game/shared )	# Sets GAME_SHARED_SRCS

# This is a strange one. The rationale behind this is that the libiconv.tbd included with the 10.13 SDK provides _libiconv* symbols while the corresponding headers provides _iconv* definitions instead,
# the reason this happens is that libiconv.tbd (in the SDK) references /usr/lib/libiconv.dylib (which is macOS version-dependent) and that copy of libiconv.dylib (on 10.13.6) provides _iconv*. The issue should be clear.
# There probably should be a Findlibiconv.cmake eventually that resolves these issues. Until then, this solution works and should always force the right library to be used in any library that links against tier1 (and uses it).
if( PLATFORM_MACOS )
	set( GAME_COMMON_LIBS "/usr/lib/libiconv.dylib" )
else()
	set( GAME_COMMON_LIBS "" )
endif()

set( GAME_COMMON_SRCS
	${HL_COMMON_SRCS}
	${HL_ENGINE_SRCS}
	${HL_PM_SHARED_SRCS}
	${HL_PUBLIC_SRCS}
	${GAME_SHARED_SRCS}
)

set( GAME_COMMON_DEFINES
	${COMMON_DEFINES}
	CLIENT_WEAPONS
)

set( GAME_COMMON_INCLUDE_PATHS
	${CMAKE_BINARY_DIR}/generated
	${CMAKE_CURRENT_SOURCE_DIR}/src/game/server
	${CMAKE_CURRENT_SOURCE_DIR}/src/game/shared
	${CMAKE_CURRENT_SOURCE_DIR}/src/public
	${CMAKE_CURRENT_SOURCE_DIR}/src/common
	${CMAKE_CURRENT_SOURCE_DIR}/src/engine
	${CMAKE_CURRENT_SOURCE_DIR}/src/game_shared
	${CMAKE_CURRENT_SOURCE_DIR}/src/pm_shared
)

#-----------------------------------------------------------------
# Source SDK
#-----------------------------------------------------------------
set( SOURCE_SDK_MIN_STEAM_API TRUE )
set( SOURCE_SDK_GFX_PATH "ui/gfx/vgui" )

if (NO_STEAM_API)
	set( SOURCE_SDK_NO_STEAM TRUE )
endif()

add_subdirectory( source_sdk )

#-----------------------------------------------------------------
# Updater
#-----------------------------------------------------------------
set( UPDATER_LIBS "" )
set( UPDATER_DEFINES USE_UPDATER=0 )

if( USE_UPDATER )
	if( NOT HAS_STD_FILESYSTEM )
		message( FATAL_ERROR "Updater requires support for std::filesystem." )
	endif()

	option( CURL_STATICLIB "Enable if cURL was built as a static library" 0 )
	option( CURL_REPO_LIBS "Use static curl from external/curl" 1 )

	if( WIN32 )
		if( CURL_REPO_LIBS )
			set( CURL_STATICLIB 1 CACHE BOOL "" FORCE )
			set( CURL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/curl/include" CACHE PATH "" FORCE )
			set( CURL_LIBRARY "" CACHE FILEPATH "" FORCE )
			set( CURL_LIBRARY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/external/curl/lib/libcurl_static_debug.lib" CACHE FILEPATH "" FORCE )
			set( CURL_LIBRARY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/external/curl/lib/libcurl_static.lib" CACHE FILEPATH "" FORCE )
		endif()
		
		find_package( libzip REQUIRED PATHS "${CMAKE_CURRENT_SOURCE_DIR}/external/libzip/lib/cmake" NO_DEFAULT_PATH )
	else()
		if( CURL_REPO_LIBS )
			set( CURL_STATICLIB 1 CACHE BOOL "" FORCE )
			set( CURL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/curl_linux/include" CACHE PATH "" FORCE )
			set( CURL_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/external/curl_linux/lib/libcurl.a" CACHE FILEPATH "" FORCE )
		endif()

		find_package( ZLIB REQUIRED )
		find_package( OpenSSL REQUIRED )
		find_package( libzip REQUIRED PATHS "${CMAKE_CURRENT_SOURCE_DIR}/external/libzip_linux/lib/cmake" NO_DEFAULT_PATH )

		set( UPDATER_LIBS OpenSSL::Crypto OpenSSL::SSL OpenSSL::Crypto OpenSSL::SSL )
	endif()

	find_package( CURL MODULE REQUIRED )
	set( UPDATER_LIBS
		${CURL_LIBRARIES}
		${UPDATER_LIBS}
		nlohmann_json
		libzip::zip
	)

	set( UPDATER_INCLUDE_PATHS
		${CURL_INCLUDE_DIRS}
	)
	
	set( UPDATER_DEFINES
		USE_UPDATER=1
	)
	
	if( CURL_STATICLIB )
		set( UPDATER_DEFINES
			${UPDATER_DEFINES}
			CURL_STATICLIB=1
		)
		if( WIN32 )
			set( UPDATER_LIBS
				${UPDATER_LIBS}
				wldap32.lib
				Crypt32.lib
				Ws2_32.lib
			)
		endif()
	endif()
	
endif()

#-----------------------------------------------------------------
# Client library
#-----------------------------------------------------------------
# Add in the shared sources
add_sources( "${GAME_COMMON_SRCS}" )

# Add in the library headers
add_sources( "${SDL2_INCLUDE_FILES}" )

# Add in the sources
add_subdirectory( src/game/client )

preprocess_sources()
add_library( client SHARED ${PREP_SRCS} )
check_winxp_support( client )
add_auto_deploy( client PublishPathClient.txt )

target_include_directories( client PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src/game/client
	${GAME_COMMON_INCLUDE_PATHS}
	${SOURCE_SDK_INCLUDE_PATHS}
	${UPDATER_INCLUDE_PATHS}
)

target_compile_definitions( client PRIVATE
	${GAME_COMMON_DEFINES}
	${SOURCE_SDK_DEFINES}
	${UPDATER_DEFINES}
	VERSION_SAFE_STEAM_API_INTERFACES
	CLIENT_DLL
)

# Dependencies
find_package( SDL2 MODULE REQUIRED )

if( WIN32 )
	# DInput for direct mouse input
	find_package( DInput REQUIRED )
	find_package( DXGUID REQUIRED )

	set( CLIENT_PLAT_LIBS
		wsock32
		psapi
		DInput
		DXGUID
		Dbghelp
		stackwalker
    )

	target_include_directories( client PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/external/SDL2/include
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
	pcre
	semver
	nlohmann_json
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

create_source_groups( "${CMAKE_SOURCE_DIR}" )
clear_sources()

#-----------------------------------------------------------------
# Server library
#-----------------------------------------------------------------
# Add in the shared sources
add_sources( "${GAME_COMMON_SRCS}" )

# Add in the sources
add_subdirectory( src/game/server )

preprocess_sources()
add_library( hl SHARED ${PREP_SRCS} )
check_winxp_support( hl )
add_auto_deploy( hl PublishPathServer.txt )

target_include_directories( hl PRIVATE
	#${CMAKE_CURRENT_SOURCE_DIR}/src/game/server	# Already in GAME_COMMON_INCLUDE_PATHS
	${GAME_COMMON_INCLUDE_PATHS}
	${SOURCE_SDK_INCLUDE_PATHS} # For mathlib
)

target_compile_definitions( hl PRIVATE
	${GAME_COMMON_DEFINES}
	${SOURCE_SDK_DEFINES}
	SERVER_DLL
	MATHLIB_USE_C_ASSERT
	MATHLIB_VECTOR_NONTRIVIAL
)

# Dependencies
if( WIN32 )
	set( SERVER_PLAT_LIBS )
else()
	set( SERVER_PLAT_LIBS
		dl
	)
endif()

# Link with dependencies
target_link_libraries( hl 
	${SERVER_PLAT_LIBS}
	semver
	Threads::Threads
)

# Linker flags
set( SERVER_LINK_FLAGS "")
set( SERVER_COMPILE_FLAGS "")

set_target_properties( hl PROPERTIES
	COMPILE_FLAGS "${SERVER_COMPILE_FLAGS}" 
	LINK_FLAGS "${SHARED_GAME_LINKER_FLAGS} ${SERVER_LINK_FLAGS}"
)

# Remove "lib" prefix
set_target_properties( hl PROPERTIES PREFIX "" )

create_source_groups( "${CMAKE_SOURCE_DIR}" )
clear_sources()

#-----------------------------------------------------------------
# AMXX module
#-----------------------------------------------------------------
# Add in the shared sources
add_sources( "${HL_COMMON_SRCS}" )
add_sources( "${HL_ENGINE_SRCS}" )
add_sources( "${HL_PUBLIC_SRCS}" )

# Add in the sources
add_subdirectory( src/bugfixedapi_amxx )

preprocess_sources()
add_library( bugfixedapi_amxx SHARED ${PREP_SRCS} )
check_winxp_support( bugfixedapi_amxx )
add_auto_deploy( bugfixedapi_amxx PublishPathAMXX.txt )

target_include_directories( bugfixedapi_amxx PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src/bugfixedapi_amxx
	${GAME_COMMON_INCLUDE_PATHS}
	${CMAKE_CURRENT_SOURCE_DIR}/external/metamod/metamod
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

create_source_groups( "${CMAKE_SOURCE_DIR}" )
clear_sources()

#-----------------------------------------------------------------
# Tests
#-----------------------------------------------------------------
add_subdirectory( src/tests )
