# This file sets following variables to TRUE when building for...
#
#	PLATFORM_WINDOWS			Windows
#	PLATFORM_UNIX				any Unix-like OS
#		PLATFORM_LINUX				any OS with Linux kernel
#			PLATFORM_ANDROID		Android specifically
#		PLATFORM_MACOS				macOS
#
#
#
# This file sets following variables to TRUE when building with...
#
#	COMPILER_MSVC				MS Visual Studio
#	COMPILER_GNU				any GCC-compatible compiler
#		COMPILER_GCC			GCC
#		COMPILER_CLANG			LLVM/Clang
#

set( PLATFORM_WINDOWS FALSE )
set( PLATFORM_UNIX FALSE )
set( PLATFORM_LINUX FALSE )
set( PLATFORM_ANDROID FALSE )
set( PLATFORM_MACOS FALSE )

set( COMPILER_MSVC FALSE )
set( COMPILER_GNU FALSE )
set( COMPILER_GCC FALSE )
set( COMPILER_CLANG FALSE )

set( PLATFORM_DEFINES "" )

#-----------------------------------------------------------------------
# Platform identification
#-----------------------------------------------------------------------
if( WIN32 )

	# Windows
	set( PLATFORM_WINDOWS TRUE )
	set( PLATFORM_DEFINES ${PLATFORM_DEFINES} PLATFORM_WINDOWS )
	
elseif( UNIX )
	set( PLATFORM_UNIX TRUE )
	set( PLATFORM_DEFINES ${PLATFORM_DEFINES} PLATFORM_UNIX )
	
	if( APPLE )
		# macOS
		set( PLATFORM_MACOS TRUE )
		set( PLATFORM_DEFINES ${PLATFORM_DEFINES} PLATFORM_MACOS )
	else()
		# Linux kernel (most likely)
		set( PLATFORM_LINUX TRUE )
		set( PLATFORM_DEFINES ${PLATFORM_DEFINES} PLATFORM_LINUX )
		
		if( ANDROID )
			set( PLATFORM_ANDROID TRUE )
			set( PLATFORM_DEFINES ${PLATFORM_DEFINES} PLATFORM_ANDROID )
		endif()
	endif()
	
else()
	message( FATAL_ERROR "Platform identification failed. Please add your platform to cmake/PlatformInfo.cmake" )
endif()

#-----------------------------------------------------------------------
# Compiler identification
#-----------------------------------------------------------------------
if( CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" )
	
	# GNU
	set( COMPILER_GNU TRUE )
	set( PLATFORM_DEFINES ${PLATFORM_DEFINES} COMPILER_GNU )
	
	if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
		# GCC
		set( COMPILER_GCC TRUE )
		set( PLATFORM_DEFINES ${PLATFORM_DEFINES} COMPILER_GCC )
	elseif( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
		# Clang
		set( COMPILER_CLANG TRUE )
		set( PLATFORM_DEFINES ${PLATFORM_DEFINES} COMPILER_CLANG )
	else()
		message( FATAL_ERROR "This isn't supposed to happen." )
	endif()
	
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
	# MSVC
	set( COMPILER_MSVC TRUE )
	set( PLATFORM_DEFINES ${PLATFORM_DEFINES} COMPILER_MSVC )
else()
	message( WARNING "Unknown compiler deteceted: ${CMAKE_CXX_COMPILER_ID}. Defaulting to GNU-compatible." )
	set( COMPILER_GNU TRUE )
	set( PLATFORM_DEFINES ${PLATFORM_DEFINES} COMPILER_GNU )
endif()
