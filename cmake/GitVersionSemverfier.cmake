# Translates git describe into more semver friendly format
# Copyright (C) EzicMan 2023
# Distributed under WTFPL license

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    # Depend on current commit
    # Forces reconfigure when commit changes
    set(_git_dir "${CMAKE_CURRENT_SOURCE_DIR}/.git")

    file(READ ${_git_dir}/HEAD _git_head)
    string(REGEX MATCH "^ref: (.*)\n$" _git_ref_match ${_git_head})

    if(_git_ref_match)
        # Matches a ref
        set(_git_ref_path "${_git_dir}/${CMAKE_MATCH_1}")
    else()
        # Detached HEAD
        # No file to reference
    endif()

    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_git_ref_path}"
        "${_git_dir}/HEAD"
    )

    execute_process(COMMAND ${GIT_EXECUTABLE} describe --long
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE _git_code
                    OUTPUT_VARIABLE _git_result
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_BRANCH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_HASH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    # Replace slash in Git branch name
    string( REPLACE "/" "-" GIT_BRANCH ${GIT_BRANCH} )

    if(NOT _git_code EQUAL 128)
        string(REGEX MATCH "(.*)-([0-9]+)-g(.+)" _git_result "${_git_result}")
        set(GIT_TAG ${CMAKE_MATCH_1})
        set(GIT_SKIP ${CMAKE_MATCH_2})
    else()
        set(GIT_TAG "v0.0.0")
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_SKIP
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    string(REGEX MATCH "v([0-9]+)\.([0-9]+)\.([0-9]+)(.*)" _git_result "${GIT_TAG}")
    set(GIT_MAJOR ${CMAKE_MATCH_1})
    set(GIT_MINOR ${CMAKE_MATCH_2})
    set(GIT_PATCH ${CMAKE_MATCH_3})
    if(NOT CMAKE_MATCH_4)
        set(_git_delimiter "-")
    else()
        set(_git_delimiter ".")
    endif()
    if(GIT_SKIP GREATER 0)
        math(EXPR GIT_MINOR "${GIT_MINOR} + 1" OUTPUT_FORMAT DECIMAL)
        string(REGEX REPLACE "(v[0-9]+\.)([0-9]+)(\.[0-9]+)(.*)" "\\1${GIT_MINOR}.0\\4" GIT_TAG "${GIT_TAG}")
        set(GIT_SEM_VERSION "${GIT_TAG}${_git_delimiter}dev.${GIT_SKIP}+${GIT_BRANCH}.${GIT_HASH}")
        set(GIT_PATCH 0)
    else()
        set(GIT_SEM_VERSION "${GIT_TAG}+${GIT_BRANCH}.${GIT_HASH}")
    endif()
    
    execute_process(COMMAND "${GIT_EXECUTABLE}" diff-index --quiet HEAD --
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		RESULT_VARIABLE _git_dirty
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_git_dirty EQUAL 1)
        set(GIT_SEM_VERSION "${GIT_SEM_VERSION}.m")
        message("Git working tree is dirty!")
    endif()
    
    #if(CMAKE_SCRIPT_MODE_FILE)
    #    math(EXPR NUM1 "${CMAKE_ARGC} - 2" OUTPUT_FORMAT DECIMAL)
    #    math(EXPR NUM2 "${CMAKE_ARGC} - 1" OUTPUT_FORMAT DECIMAL)
    #    
    #    configure_file (${CMAKE_ARGV${NUM1}} ${CMAKE_ARGV${NUM2}})
    #endif()

    string(REGEX REPLACE "v(.*)" "\\1" GIT_SEM_VERSION "${GIT_SEM_VERSION}")
    set(GIT_SUCCESS TRUE)
else()
    set(GIT_MAJOR 0)
    set(GIT_MINOR 0)
    set(GIT_PATCH 0)
    set(GIT_SKIP 0)
    set(GIT_TAG "v0.0.0")
    set(GIT_SEM_VERSION "0.0.0+error.0000000")
    set(GIT_SUCCESS FALSE)
endif()
