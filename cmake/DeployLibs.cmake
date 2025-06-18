cmake_minimum_required( VERSION 3.25.0 )

# In script mode, CMAKE_ARGVx contains ALL arguments, including `-P` and script path
# This is stupid, so we neew to find where `--` is
set( actual_args_start_idx 0 )
math( EXPR last_arg_idx "${CMAKE_ARGC} - 1" )

foreach( i RANGE 0 ${last_arg_idx})
	set( arg "${CMAKE_ARGV${i}}" )

	if( "${arg}" STREQUAL "--" )
		math( EXPR actual_args_start_idx "${i} + 1")
		break()
	endif()
endforeach()

set( path_list_file ${CMAKE_ARGV${actual_args_start_idx}} )

if( NOT path_list_file )
	message( FATAL_ERROR "Path list file not specified." )
endif()

if( NOT EXISTS "${path_list_file}" )
	message( NOTICE "No deployment path specified. Create file ${path_list_file} with folder paths on separate lines for auto deployment." )
	return()
endif()

# Read the path list file
file( STRINGS ${path_list_file} deploy_paths )

# Iterate over all paths in the file
foreach( deploy_path IN LISTS deploy_paths)
	# Convert to full path
	file( REAL_PATH ${deploy_path} deploy_path_full )

	# Iterate over all files to copy
	math( EXPR path_arg_start "${actual_args_start_idx} + 1" )
	foreach( i RANGE ${path_arg_start} ${last_arg_idx})
		file( REAL_PATH "${CMAKE_ARGV${i}}" file_to_copy )
		message(STATUS "${file_to_copy} -> ${deploy_path}")
		file(
			COPY ${file_to_copy}
			DESTINATION ${deploy_path_full}
		)
	endforeach()
endforeach()
