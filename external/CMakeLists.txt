add_subdirectory( lodepng )
add_subdirectory( pcre )
add_subdirectory( SDL2 )
add_subdirectory( semver.c )

set( SDL2_INCLUDE_FILES "${SDL2_INCLUDE_FILES}" PARENT_SCOPE )
