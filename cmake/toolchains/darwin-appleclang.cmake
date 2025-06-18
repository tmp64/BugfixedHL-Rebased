# This toolchain adds -m32 to compiler flags
# It is required to make CMake link with proper 32-bit libraries

set( CMAKE_SYSTEM_NAME Darwin )

# Valve built Half-Life to support 10.5+ using a 10.8 SDK
# The most modern SDK to support i386 is 10.13 while 10.10 is the lowest deployment target
set( CMAKE_OSX_DEPLOYMENT_TARGET 10.10 CACHE STRING "" FORCE )
set( CMAKE_OSX_ARCHITECTURES i386 )
set( CMAKE_OSX_SYSROOT ${CMAKE_OSX_SYSROOT} CACHE PATH "" FORCE )

if( NOT CMAKE_OSX_SYSROOT MATCHES "10.13" )
	message( FATAL_ERROR "The provided SDK does not match the required 10.13 SDK." )
endif()

set( CMAKE_SYSTEM_VERSION "10.10" )
set( CMAKE_SYSTEM_PROCESSOR "i386" )

set( CMAKE_C_COMPILER clang -m32 )
set( CMAKE_CXX_COMPILER clang++ -m32 )
