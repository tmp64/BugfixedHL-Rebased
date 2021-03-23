add_sources(
	CMakeLists.txt
	bitvec.h
	CGameVersion.cpp
	CGameVersion.h
	ClientSupportsFlags.h
	enum_utils.h
	IBugfixedServer.h
	IGameVersion.h
	mathlib.cpp
	voice_common.h
)

get_sources( GAME_SHARED_SRCS )
clear_sources()

set( GAME_SHARED_SRCS "${GAME_SHARED_SRCS}" PARENT_SCOPE )
