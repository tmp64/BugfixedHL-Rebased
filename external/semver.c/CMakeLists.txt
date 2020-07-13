add_library( semver STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/semver.c
)

target_compile_definitions( semver PRIVATE
	_CRT_SECURE_NO_WARNINGS
	_SCL_SECURE_NO_WARNINGS
)
target_include_directories( semver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )
