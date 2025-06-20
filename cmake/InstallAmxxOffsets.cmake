# Glob all offset files
file(
	GLOB offset_files
	LIST_DIRECTORIES false
	"${CMAKE_BINARY_DIR}/amxx-offsets/*.txt"
	"${CMAKE_BINARY_DIR}/bhl-amxx-offsets-*.json"
)

# Install them
file(
	INSTALL ${offset_files}
	DESTINATION "${CMAKE_INSTALL_PREFIX}/addons/amxmodx/data/gamedata/common.games/custom"
)
