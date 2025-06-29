set( TARGET_NAME vgui_fonts_freetype )

add_library( ${TARGET_NAME} STATIC
	include/vgui/fonts_freetype/factory.h

	src/factory.cpp
	src/vgui_scheme_wrap.h
)

target_precompile_headers( ${TARGET_NAME} PRIVATE src/pch.h)

target_include_directories( ${TARGET_NAME}
	PUBLIC
		${GAME_COMMON_INCLUDE_PATHS}
		${SOURCE_SDK_INCLUDE_PATHS}
		include
	PRIVATE
		src
)

target_compile_definitions( ${TARGET_NAME}
	PUBLIC
		${SOURCE_SDK_DEFINES}
)

target_link_libraries( ${TARGET_NAME}
	PUBLIC
		${SOURCE_SDK_LIBS}
		fmt::fmt
		utils_logging
	PRIVATE
		Freetype::Freetype
)
