set(PCRE_PUBLIC_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/include/pcre.h

)

set(PCRE_PRIVATE_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/config.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ucp.h
)

set(PCRE_PRIVATE_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_chartables.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_compile.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_exec.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_globals.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_newline.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_tables.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/pcre_try_flipped.c

)

add_library(pcre STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
	${PCRE_PUBLIC_HEADERS}
	${PCRE_PRIVATE_HEADERS}
	${PCRE_PRIVATE_SOURCES}
)

target_include_directories(pcre PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(pcre PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(pcre PUBLIC
	PCRE_STATIC
)

target_compile_definitions(pcre PRIVATE
	HAVE_CONFIG_H
)

source_group("Public Headers" FILES ${PCRE_PUBLIC_HEADERS})
source_group("Private Headers" FILES ${PCRE_PRIVATE_HEADERS})
source_group("Private Sources" FILES ${PCRE_PRIVATE_SOURCES})
