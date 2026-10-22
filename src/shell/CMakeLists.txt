set(shell_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/bridge.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/client.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ime.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/input.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pixels.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/scheme.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/shell.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/surface.cpp
)
# CEF's headers require C++20; the rest of the engine stays on its own standard.
set_source_files_properties(${shell_SRCS} PROPERTIES COMPILE_OPTIONS "-std=c++20")
set(shell_SRCS ${shell_SRCS} PARENT_SCOPE)

# Chromium's runtime files next to the executable. libcef.so ships with debug
# info (~1.5 GB); the copy is stripped.
function(add_cef_runtime dir)
	set(cef_runtime_outputs "${dir}/libcef.so")
	add_custom_command(
		OUTPUT "${dir}/libcef.so"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}"
		COMMAND ${CMAKE_STRIP} --strip-unneeded -o "${dir}/libcef.so" "${CEF_BINARY_DIR}/libcef.so"
		DEPENDS "${CEF_BINARY_DIR}/libcef.so"
		COMMENT "Stripping libcef.so into ${dir}")

	foreach(file IN LISTS CEF_BINARY_FILES)
		if(file STREQUAL "libcef.so" OR file STREQUAL "chrome-sandbox")
			continue()
		endif()
		list(APPEND cef_runtime_outputs "${dir}/${file}")
		add_custom_command(
			OUTPUT "${dir}/${file}"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/${file}" "${dir}/${file}"
			DEPENDS "${CEF_BINARY_DIR}/${file}")
	endforeach()

	foreach(file IN LISTS CEF_RESOURCE_FILES)
		list(APPEND cef_runtime_outputs "${dir}/${file}")
		if(IS_DIRECTORY "${CEF_RESOURCE_DIR}/${file}")
			add_custom_command(
				OUTPUT "${dir}/${file}"
				COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_RESOURCE_DIR}/${file}" "${dir}/${file}"
				DEPENDS "${CEF_RESOURCE_DIR}/${file}")
		else()
			add_custom_command(
				OUTPUT "${dir}/${file}"
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_RESOURCE_DIR}/${file}" "${dir}/${file}"
				DEPENDS "${CEF_RESOURCE_DIR}/${file}")
		endif()
	endforeach()

	add_custom_target(cef_runtime DEPENDS ${cef_runtime_outputs})
endfunction()
