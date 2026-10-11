#
# Bugfixed and Improved Half-Life Release
#

cmake_minimum_required( VERSION 3.30.0 )

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

include( FetchContent )
include( InputFilesList )
include( GitVersionSemverfier )

#-----------------------------------------------------------------
# Version
#-----------------------------------------------------------------
set( BHL_REPO_VERSION_MAJOR 1 )
set( BHL_REPO_VERSION_MINOR 14 )
set( BHL_REPO_VERSION_PATCH 0 )
set( BHL_REPO_VERSION_TAG "dev" )

if( GIT_SUCCESS )
	message( "Version: ${GIT_SEM_VERSION} (from Git)" )
else()
	set(GIT_MAJOR ${BHL_REPO_VERSION_MAJOR})
	set(GIT_MINOR ${BHL_REPO_VERSION_MINOR})
	set(GIT_PATCH ${BHL_REPO_VERSION_PATCH})
	set(GIT_SKIP 0)

	if( BHL_REPO_VERSION_TAG STREQUAL "" )
		set(GIT_TAG "v${GIT_MAJOR}.${GIT_MINOR}.${GIT_PATCH}")
		set(GIT_SEM_VERSION "${GIT_MAJOR}.${GIT_MINOR}.${GIT_PATCH}+no-git")
	else()
		set(GIT_TAG "v${GIT_MAJOR}.${GIT_MINOR}.${GIT_PATCH}-${BHL_REPO_VERSION_TAG}")
		set(GIT_SEM_VERSION "${GIT_MAJOR}.${GIT_MINOR}.${GIT_PATCH}-${BHL_REPO_VERSION_TAG}+no-git")
	endif()

	message( "Version: ${GIT_SEM_VERSION} (from repo)" )
endif()

unset( BHL_REPO_VERSION_MAJOR )
unset( BHL_REPO_VERSION_MINOR )
unset( BHL_REPO_VERSION_PATCH )
unset( BHL_REPO_VERSION_TAG )
# Use GIT_XXX from now on

# This will be used by CI build scripts
file( WRITE ${CMAKE_BINARY_DIR}/version.txt ${GIT_SEM_VERSION} )

project( BugfixedHL VERSION "${GIT_MAJOR}.${GIT_MINOR}.${GIT_PATCH}.${GIT_SKIP}" )
include( PlatformInfo )
include( CTest )

#-----------------------------------------------------------------
# Build options
#-----------------------------------------------------------------
option( AUTO_DEPLOY				"Whether to automatically deploy to deploy paths" )
option( USE_UPDATER				"Enable update checked and downloader" )
option( NO_STEAM_API			"Disable Steam API" )
option( GENERATE_AMXX_OFFSETS	"Generate AMXX offsets when building" )

#-----------------------------------------------------------------
# Compiler checks
#-----------------------------------------------------------------
if( NOT CMAKE_SIZEOF_VOID_P EQUAL 4 )
	if( NO_SIZEOF_VOID_P_CHECK )
		message( WARNING "You are not building for 32 bit systems. This is not supported." )
	else()
		message( FATAL_ERROR "You are not building for 32 bit systems. Did you forget VCPKG_CHAINLOAD_TOOLCHAIN_FILE?" )
	endif()
endif()

#-----------------------------------------------------------------
# Compiler and linker settings
#-----------------------------------------------------------------
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

# Enable project folders in VS
set_property( GLOBAL PROPERTY USE_FOLDERS ON )

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
	
	# Target Windows 7
	add_compile_definitions( _WIN32_WINNT=0x0601 )
	add_compile_definitions( WINVER=0x0601 )
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
	# LNK4099: PDB  was not found with 'xxx.lib(yyy.obj)' or at ''; linking object as if no debug info
	set(GAME_LINKER_FLAGS "${GAME_LINKER_FLAGS} /IGNORE:4099")

	if( WARNINGS_ARE_ERRORS )
		set( WARNING_LEVEL_C_CXX "${WARNING_LEVEL_C_CXX} /WX" )
	endif()
endif()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_LEVEL_C} ${WARNING_LEVEL_C_CXX}" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_LEVEL_CXX} ${WARNING_LEVEL_C_CXX}" )

#-----------------------------------------------------------------
# appversion.h generator
#-----------------------------------------------------------------
set( BHL_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated" )
set( BHL_GENERATED_INCLUDE_DIR "${BHL_GENERATED_DIR}/include" )
configure_file( "src/common/appversion.h.in" "${BHL_GENERATED_INCLUDE_DIR}/appversion.h" )
include_directories( ${BHL_GENERATED_INCLUDE_DIR} )

# Generates version_info.rc resource file for the target.
# @param target_name		Target name. Must be an EXE or a DLL.
# @param file_name			Output file name.
# @param file_description	Short file description.
function(bhl_version_info target_name file_name file_description)
	get_target_property(target_type ${target_name} TYPE)

	if(target_type STREQUAL "EXECUTABLE")
		set(is_dll 0)
	elseif(target_type STREQUAL "SHARED_LIBRARY")
		set(is_dll 1)
	else()
		message(FATAL_ERROR "Target ${target_name} is neither EXE nor DLL, it's ${target_type}")
	endif()

	if(GIT_SKIP GREATER 0)
		set(is_prerelease 1)
	else()
		set(is_prerelease 0)
	endif()

	string(TIMESTAMP current_year "%Y" UTC)

	set(generated_dir "${BHL_GENERATED_DIR}/${target_name}")
	set(in_file_path "${CMAKE_SOURCE_DIR}/src/common/version_info.rc.in")
	set(out_file_path "${generated_dir}/version_info.rc")

	configure_file(
		${in_file_path}
		${out_file_path}
		@ONLY
	)

	target_sources(${target_name} PRIVATE ${out_file_path})
	source_group(TREE ${generated_dir} PREFIX "Generated" FILES ${out_file_path})
endfunction()

#-----------------------------------------------------------------
# Autodeploy macro
#-----------------------------------------------------------------
if( AUTO_DEPLOY )
	macro( add_auto_deploy target_name component_name publish_paths_file )
		add_custom_command( TARGET ${target_name}
			POST_BUILD
			COMMAND
				${CMAKE_COMMAND}
				-P
				${CMAKE_SOURCE_DIR}/cmake/DeployLibs.cmake
				--
				${CMAKE_BINARY_DIR}
				${component_name}
				"$<CONFIG>"
				${CMAKE_BINARY_DIR}/${publish_paths_file}
		)
	endmacro()
else()
	macro( add_auto_deploy )
	endmacro()
endif()

#-----------------------------------------------------------------
# Libraries
#-----------------------------------------------------------------
set( THREADS_PREFER_PTHREAD_FLAG ON )
find_package( nlohmann_json CONFIG REQUIRED )
find_package( Python 3.8...<4.0 COMPONENTS Interpreter )
find_package( Threads REQUIRED )
find_package( unofficial-pcre CONFIG REQUIRED )

include( cmake/lib/SDL2.cmake )

add_subdirectory( external )

if( WIN32 )
	find_package( stackwalker CONFIG REQUIRED )
	include( cmake/lib/DInput.cmake )
endif()

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

	# Find libraries
	find_package( libzip CONFIG REQUIRED )
	find_package( CURL REQUIRED )

	set( UPDATER_LIBS
		${UPDATER_LIBS}
		libzip::zip
		CURL::libcurl
	)

	set( UPDATER_DEFINES
		USE_UPDATER=1
	)
endif()

#-----------------------------------------------------------------
# BHL Targets
#-----------------------------------------------------------------
set( CMAKE_FOLDER "HL SDK" )

add_subdirectory( src/bugfixedapi_amxx )
add_subdirectory( src/game/client )
add_subdirectory( src/game/server )
add_subdirectory( src/tests )
add_subdirectory( gamedir )

# Disable PCH for common sources because includes are a mess there
set_source_files_properties(
	${GAME_COMMON_SRCS}
	TARGET_DIRECTORY
		client
		server
		bugfixedapi_amxx
	PROPERTIES
		SKIP_PRECOMPILE_HEADERS ON
)

# Add custom targets for CI
add_custom_target( ci-client )
add_dependencies( ci-client client test_client )

add_custom_target( ci-server )
add_dependencies( ci-server server test_server bugfixedapi_amxx )
