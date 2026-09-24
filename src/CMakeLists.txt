# Layered libraries. Dependency direction is enforced by target_link_libraries:
#
#   ui -> web -> services -> core
#   |     |
#   +---> platform -> core
#
# Nothing links to `ui` except the executable (via `app`).

add_subdirectory(core)
add_subdirectory(modules/AccountAndLicense)
add_subdirectory(services)
add_subdirectory(platform)
add_subdirectory(web)
add_subdirectory(ui)
add_subdirectory(app)

qt_add_executable(playlist-dl
    main.cpp
)
pldl_set_warnings(playlist-dl)
target_link_libraries(playlist-dl PRIVATE pldl_app)

if(LINUX)
    # QtWebEngine drags in libproxy, whose backends are not always resolvable at
    # link time in the KDE Qt6 content snaps; those symbols come from shared
    # libraries and resolve at runtime.
    target_link_options(playlist-dl PRIVATE -Wl,--allow-shlib-undefined)
endif()

set_target_properties(playlist-dl PROPERTIES
    OUTPUT_NAME playlist-dl
    # Export symbols (-rdynamic) so the crash handler's backtrace shows function
    # names instead of bare addresses.
    ENABLE_EXPORTS ON
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    # Pure QtWidgets app: no QML anywhere, skip the qmlimportscanner finalizer.
    QT_QML_MODULE_NO_IMPORT_SCAN TRUE
)

install(TARGETS playlist-dl
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
