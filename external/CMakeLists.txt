set( CMAKE_FOLDER "External" )

add_subdirectory( glad )
add_subdirectory( SDL2 )
add_subdirectory( semver.c )

set( SDL2_INCLUDE_FILES "${SDL2_INCLUDE_FILES}" PARENT_SCOPE )
