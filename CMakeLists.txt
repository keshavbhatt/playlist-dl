cmake_minimum_required(VERSION 3.21)

project(playlist-dl
    VERSION 3.0.0
    DESCRIPTION "Search, browse, play and download YouTube playlists"
    HOMEPAGE_URL "https://github.com/keshavbhatt/playlist-dl"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(PLDL_BUILD_TESTS "Build unit tests" ON)
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
# Qt version. Linux builds and runs on 6.11, the version shipped by the
# kde-qt6-core24-sdk snap and the Flathub KDE runtime (ADR-001).
# ---------------------------------------------------------------------------
set(PLDL_QT_MIN_VERSION "6.11" CACHE STRING "Minimum required Qt6 version")
find_package(Qt6 ${PLDL_QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    WebChannel
    Network
    Sql
    Concurrent
    Svg
)
qt_standard_project_setup()
include(SnapSdkWorkaround)

if(MSVC)
    add_compile_options(/utf-8 /bigobj)
endif()

# Qt hygiene: forces explicit string conversions and keyword-free signal syntax.
add_compile_definitions(
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_KEYWORDS
    QT_NO_FOREACH
    QT_DISABLE_DEPRECATED_UP_TO=0x061100
    QT_USE_QSTRINGBUILDER
)

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
# Desktop integration (Linux): .desktop, AppStream metainfo, hicolor icons.
# ---------------------------------------------------------------------------
if(LINUX)
    include(GNUInstallDirs)
    install(FILES dist/linux/com.ktechpit.playlist-dl.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES dist/linux/com.ktechpit.playlist-dl.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(DIRECTORY src/resources/icons/hicolor
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
endif()

if(PLDL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
