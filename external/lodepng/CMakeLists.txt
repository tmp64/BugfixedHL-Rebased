add_library( lodepng STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/lodepng.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/lodepng.h
)

target_compile_definitions( lodepng PUBLIC
	LODEPNG_NO_COMPILE_CPP
)

target_include_directories( lodepng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
