add_library( stackwalker STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/src/StackWalker.cpp
)

target_include_directories( stackwalker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src )
