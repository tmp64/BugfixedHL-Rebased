# This toolchain adds -m32 to compiler flags
# It is required to make CMake link with proper 32-bit libraries

include( ${CMAKE_CURRENT_LIST_DIR}/include-linux-m32.cmake )

set( CMAKE_C_COMPILER gcc -m32 )
set( CMAKE_CXX_COMPILER g++ -m32) 
