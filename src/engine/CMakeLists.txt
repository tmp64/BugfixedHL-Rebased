add_sources(
	CMakeLists.txt
	anorms.h
	APIProxy.h
	cdll_int.h
	custom.h
	customentity.h
	edict.h
	eiface.h
	progdefs.h
	progs.h
	shake.h
	studio.h
)

get_sources( HL_ENGINE_SRCS )
clear_sources()

set( HL_ENGINE_SRCS "${HL_ENGINE_SRCS}" PARENT_SCOPE )
