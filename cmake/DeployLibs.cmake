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

# Copy args to temp vars
math( EXPR arg_binary_dir_idx "${actual_args_start_idx} + 0" )
set( binary_dir ${CMAKE_ARGV${arg_binary_dir_idx}} )

math( EXPR arg_component_idx "${actual_args_start_idx} + 1" )
set( component_name ${CMAKE_ARGV${arg_component_idx}} )

math( EXPR arg_config_idx "${actual_args_start_idx} + 2" )
set( config_name ${CMAKE_ARGV${arg_config_idx}} )

math( EXPR arg_path_list_idx "${actual_args_start_idx} + 3" )
set( path_list_file ${CMAKE_ARGV${arg_path_list_idx}} )

# Check that args are set
if( NOT binary_dir )
	message( FATAL_ERROR "Binary dir path not specified." )
endif()

if( NOT component_name )
	message( FATAL_ERROR "Component name not specified." )
endif()

if( NOT path_list_file )
	message( FATAL_ERROR "Path list file not specified." )
endif()

# Check that paths exist
if( NOT EXISTS "${binary_dir}" )
	message( FATAL_ERROR "Binary dir ${FATAL_ERROR} doesn't exist." )
endif()

if( NOT EXISTS "${path_list_file}" )
	message( NOTICE "No deployment path specified. Create file ${path_list_file} with folder paths on separate lines for auto deployment." )
	return()
endif()

# Read the path list file
file( STRINGS ${path_list_file} deploy_paths )

# Iterate over all paths in the file
foreach( deploy_path IN LISTS deploy_paths)
	cmake_path( IS_ABSOLUTE deploy_path is_path_absolute )

	if( NOT is_path_absolute )
		message( SEND_ERROR "Path must be absolute: ${deploy_path_full}" )
		continue()
	endif()

	if( NOT EXISTS "${deploy_path}" )
		message( SEND_ERROR "Path doesn't exist: ${deploy_path}" )
		continue()
	endif()

	# Run cmake --install for the path
	execute_process(
		COMMAND
			${CMAKE_COMMAND}
			--install ${binary_dir}
			--config "${config_name}"
			--component ${component_name}
			--prefix ${deploy_path}
	)
endforeach()
