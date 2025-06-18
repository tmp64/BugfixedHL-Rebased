block()
	if( NOT MSVC )
		message( FATAL_ERROR "DInput is only supported on Windows with MSVC++" )
	endif()

	set( lib_dir ${PROJECT_SOURCE_DIR}/external/windows/dinput )

	# DInput
	add_library( DInput SHARED IMPORTED GLOBAL )
	set_property( TARGET DInput PROPERTY IMPORTED_IMPLIB ${lib_dir}/lib/dinput8.lib )
	target_include_directories( DInput INTERFACE ${lib_dir}/include )
	target_compile_definitions( DInput INTERFACE DIRECTINPUT_VERSION=0x0800 )

	# DXGUID
	add_library( DXGUID SHARED IMPORTED GLOBAL )
	set_property( TARGET DXGUID PROPERTY IMPORTED_IMPLIB ${lib_dir}/lib/dxguid.lib )
endblock()
