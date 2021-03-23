add_sources(
	CMakeLists.txt
	pm_debug.cpp
	pm_debug.h
	pm_defs.h
	pm_info.h
	pm_materials.h
	pm_math.cpp
	pm_math.h
	pm_movevars.h
	pm_shared.cpp
	pm_shared.h
)

get_sources( HL_PM_SHARED_SRCS )
clear_sources()

set( HL_PM_SHARED_SRCS "${HL_PM_SHARED_SRCS}" PARENT_SCOPE )
