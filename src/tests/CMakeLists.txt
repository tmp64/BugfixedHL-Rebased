if( BUILD_TESTING )
	enable_testing()

	set( TESTS_COMMON
		common/plat.h
	)

	if( PLATFORM_WINDOWS )
		set( TESTS_COMMON
			${TESTS_COMMON}
			common/plat_windows.cpp
		)
	elseif( PLATFORM_LINUX )
		set( TESTS_COMMON
			${TESTS_COMMON}
			common/plat_linux.cpp
		)
	endif()

	set( TESTS_CLIENT
		client/cl_exports.h
		client/main.cpp
	)

	set( TESTS_SERVER
		server/main.cpp
		server/sv_exports.h
	)

	#-----------------------------------------------------------------

	add_executable( test_client
		${TESTS_COMMON}
		${TESTS_CLIENT}
	)

	add_dependencies( test_client client )

	target_include_directories( test_client PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/common
		${GAME_COMMON_INCLUDE_PATHS}
		${SOURCE_SDK_INCLUDE_PATHS}
	)

	target_compile_definitions( test_client PRIVATE
		${SOURCE_SDK_DEFINES}
	)

	target_link_libraries( test_client PRIVATE
		tier1
		vstdlib
		tier0
	)

	#-----------------------------------------------------------------

	add_executable( test_server
		${TESTS_COMMON}
		${TESTS_SERVER}
	)

	add_dependencies( test_client hl )

	target_include_directories( test_server PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/common
		${SOURCE_SDK_INCLUDE_PATHS}
	)

	target_compile_definitions( test_server PRIVATE
		${SOURCE_SDK_DEFINES}
	)

	target_link_libraries( test_server PRIVATE
		tier1
		vstdlib
		tier0
	)

	#-----------------------------------------------------------------

	add_test( NAME client
		COMMAND test_client "$<TARGET_FILE:client>"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/workdir"
	)

	add_test( NAME server
		COMMAND test_server "$<TARGET_FILE:hl>"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/workdir"
	)

	set_tests_properties( client server PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=.:$ENV{LD_LIBRARY_PATH}")

endif()
