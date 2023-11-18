add_sources(
	CMakeLists.txt
	cl_dll/IGameClientExports.h
	steam/steamtypes.h
	archtypes.h
	keydefs.h
	particleman.h
	pman_particlemem.h
	pman_triangleffect.h
)

get_sources( HL_PUBLIC_SRCS )
clear_sources()

set( HL_PUBLIC_SRCS "${HL_PUBLIC_SRCS}" PARENT_SCOPE )
