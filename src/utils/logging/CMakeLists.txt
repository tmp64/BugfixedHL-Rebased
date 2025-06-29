set( TARGET_NAME utils_logging )

add_library( ${TARGET_NAME} STATIC
	include/bhl/logging/ILogger.h
	include/bhl/logging/prefix_logger.h

	src/prefix_logger.cpp
)

target_include_directories( ${TARGET_NAME}
	PUBLIC
		include
	PRIVATE
		src
)

target_link_libraries( ${TARGET_NAME}
	PUBLIC
		fmt::fmt
)
