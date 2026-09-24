# Derives version metadata for the build.
#
# PLDL_VERSION        -> "5.0.0" (from project(VERSION))
# PLDL_GIT_REVISION   -> short hash or "unknown"
#
# These are handed to the code via a generated header (see src/app/CMakeLists.txt),
# never via string literals in sources.

set(PLDL_VERSION "${PROJECT_VERSION}")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE PLDL_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT PLDL_GIT_REVISION)
    set(PLDL_GIT_REVISION "unknown")
endif()

message(STATUS "playlist-dl ${PLDL_VERSION} (${PLDL_GIT_REVISION})")
