block()
	set( lib_name SDL2 )
	set( lib_dir ${PROJECT_SOURCE_DIR}/lib/public )

	add_library( ${lib_name} SHARED IMPORTED GLOBAL )

	if( WIN32 )
			set_property( TARGET ${lib_name} PROPERTY IMPORTED_IMPLIB ${lib_dir}/SDL2.lib )
		elseif( APPLE )
			set_property( TARGET ${lib_name} PROPERTY IMPORTED_LOCATION ${lib_dir}/libSDL2-2.0.0.dylib )
		else()
			set_property( TARGET ${lib_name} PROPERTY IMPORTED_LOCATION ${lib_dir}/libSDL2-2.0.so.0 )
	endif()
endblock()
