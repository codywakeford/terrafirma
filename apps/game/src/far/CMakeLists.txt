# far: the far view. `terrain` (the land as blocks) and `horizon` (its
# packet) are built everywhere: the server sends, the unit tests build
# tiles. `mesh` and `view` are the client's.
set(far_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/terrain.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/horizon.cpp
	PARENT_SCOPE)

set(far_client_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/mesh.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/view.cpp
	PARENT_SCOPE)
