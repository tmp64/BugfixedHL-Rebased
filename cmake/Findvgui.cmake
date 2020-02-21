#
# Finds the VGUI1 library
# Expects to be called from the SDK root
#

find_library( VGUI1 NAMES vgui vgui${CMAKE_SHARED_LIBRARY_SUFFIX} PATHS ${vgui_DIR} ${CMAKE_SOURCE_DIR}/utils/vgui/lib/win32_vc6 ${CMAKE_SOURCE_DIR}/lib/public NO_DEFAULT_PATH )

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( vgui DEFAULT_MSG VGUI1 )

if( VGUI1 )
	add_library( vgui SHARED IMPORTED )
	
	set_property( TARGET vgui PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/external/vgui/include )
	
	if( MSVC )
		set_property( TARGET vgui PROPERTY IMPORTED_IMPLIB ${VGUI1} )
	else()
		set_property( TARGET vgui PROPERTY IMPORTED_LOCATION ${VGUI1} )
	endif()
endif()

unset( VGUI1 CACHE )
