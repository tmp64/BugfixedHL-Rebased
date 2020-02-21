add_sources(
	CMakeLists.txt
	pm_debug.c
	pm_debug.h
	pm_defs.h
	pm_info.h
	pm_materials.h
	pm_math.c
	pm_movevars.h
	pm_shared.c
	pm_shared.h
)

get_sources( HL_PM_SHARED_SRCS )
clear_sources()

set( HL_PM_SHARED_SRCS "${HL_PM_SHARED_SRCS}" PARENT_SCOPE )
