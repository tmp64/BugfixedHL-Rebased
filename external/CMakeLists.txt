add_subdirectory( nlohmann_json )
add_subdirectory( pcre )
add_subdirectory( SDL2 )
add_subdirectory( semver.c )

if (WIN32)
    add_subdirectory( stackwalker )
endif()

set( SDL2_INCLUDE_FILES "${SDL2_INCLUDE_FILES}" PARENT_SCOPE )
