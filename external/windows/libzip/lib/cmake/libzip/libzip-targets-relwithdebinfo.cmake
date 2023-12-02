#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libzip::zip" for configuration "RelWithDebInfo"
set_property(TARGET libzip::zip APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(libzip::zip PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/zip.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS libzip::zip )
list(APPEND _IMPORT_CHECK_FILES_FOR_libzip::zip "${_IMPORT_PREFIX}/lib/zip.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
