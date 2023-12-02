#
# Finds DInput library
#

if( NOT MSVC )
	message( FATAL_ERROR "DInput is only supported on Windows with MSVC++" )
endif()

find_library( DINPUT_LIB NAMES dinput8.lib PATHS ${CMAKE_SOURCE_DIR}/external/windows/dinput NO_DEFAULT_PATH )

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( DInput DEFAULT_MSG DINPUT_LIB )

if( DINPUT_LIB )
	add_library( DInput SHARED IMPORTED )
	set_property( TARGET DInput PROPERTY IMPORTED_IMPLIB ${DINPUT_LIB} )
	target_include_directories( DInput INTERFACE ${CMAKE_SOURCE_DIR}/external/windows/dinput/include )
	target_compile_definitions( DInput INTERFACE DIRECTINPUT_VERSION=0x0800 )
endif()

unset( DINPUT_LIB CACHE )
