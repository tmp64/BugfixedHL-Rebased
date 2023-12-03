#
# Finds DXGUID library
#

if( NOT MSVC )
	message( FATAL_ERROR "DXGUID is only supported on Windows with MSVC++" )
endif()

find_library( DXGUID_LIB NAMES dxguid.lib PATHS ${CMAKE_SOURCE_DIR}/external/windows/dinput NO_DEFAULT_PATH )

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( DXGUID DEFAULT_MSG DXGUID_LIB )

if( DXGUID_LIB )
	add_library( DXGUID SHARED IMPORTED )
	set_property( TARGET DXGUID PROPERTY IMPORTED_IMPLIB ${DXGUID_LIB} )
endif()

unset( DXGUID_LIB CACHE )
