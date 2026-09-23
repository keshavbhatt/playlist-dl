cmake_minimum_required(VERSION 3.21)

project(playlist-dl
    VERSION 3.0.0
    DESCRIPTION "Search, browse, play and download YouTube playlists"
    HOMEPAGE_URL "https://github.com/keshavbhatt/p-pldl"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(PLDL_WERROR "Treat warnings as errors" OFF)
option(PLDL_ENABLE_CLANG_TIDY "Run clang-tidy during the build" OFF)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Warnings)
include(Version)

# ---------------------------------------------------------------------------
# Qt version. Linux builds and runs on the Qt shipped by the
# kde-qt6-core24-sdk snap and the kf6-core24 content snap (6.11 at the time
# of writing); anything from 6.8 on is accepted for local builds.
# ---------------------------------------------------------------------------
set(PLDL_QT_MIN_VERSION "6.8" CACHE STRING "Minimum required Qt6 version")
find_package(Qt6 ${PLDL_QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebEngineWidgets
    WebChannel
)
qt_standard_project_setup()
include(SnapSdkWorkaround)

# No debug output in release builds (mirrors the old qmake CONFIG(release)).
add_compile_definitions($<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>)

if(PLDL_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
add_subdirectory(src)

# ---------------------------------------------------------------------------
# Desktop integration (Linux): .desktop and hicolor icons.
# ---------------------------------------------------------------------------
if(LINUX)
    include(GNUInstallDirs)
    install(FILES dist/linux/playlist-dl.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    foreach(size 16 32 64 128 256 512)
        install(FILES src/icons/app/icon-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME playlist-dl.png)
    endforeach()
endif()
