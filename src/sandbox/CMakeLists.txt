# sandbox: a tiny voxel piece of the universe for the wiki's demos, run with
# the game's own matter physics. Depends on matter only.
set(sandbox_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/flow.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/physics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sandbox.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/world.cpp
	PARENT_SCOPE)
