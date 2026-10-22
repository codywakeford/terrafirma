# Prebuilt Chromium Embedded Framework for the web shell (src/shell).
# Downloaded once into the build tree and verified against a pinned hash.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "ENABLE_SHELL is only wired for Linux so far")
endif()

set(CEF_VERSION "156.0.7+g47dbc1d+chromium-156.0.8078.17")
set(CEF_SHA1 "c8c8055f9ad040a42691be6f04fb6107e2309c8d")
set(CEF_DIST "cef_binary_${CEF_VERSION}_linux64_minimal")
set(CEF_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")
set(CEF_ROOT "${CEF_DEPS_DIR}/${CEF_DIST}")

if(NOT IS_DIRECTORY "${CEF_ROOT}/cmake")
	string(REPLACE "+" "%2B" cef_url_dist "${CEF_DIST}")
	message(STATUS "Downloading CEF ${CEF_VERSION}")
	file(DOWNLOAD "https://cef-builds.spotifycdn.com/${cef_url_dist}.tar.bz2"
		"${CEF_DEPS_DIR}/cef.tar.bz2"
		EXPECTED_HASH SHA1=${CEF_SHA1}
		SHOW_PROGRESS)
	file(ARCHIVE_EXTRACT INPUT "${CEF_DEPS_DIR}/cef.tar.bz2" DESTINATION "${CEF_DEPS_DIR}")
	file(REMOVE "${CEF_DEPS_DIR}/cef.tar.bz2")
endif()

# CEF's bootstrap forces a build type when none is set; ours is always set by now.
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper EXCLUDE_FROM_ALL)
# The wrapper is linked into an RTTI + exceptions binary, and CEF's -Werror
# trips on newer compilers than it was released against.
target_compile_options(libcef_dll_wrapper PRIVATE -frtti -Wno-error)
