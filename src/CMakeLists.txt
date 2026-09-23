# The application: one executable built from the widget sources below plus
# the self-contained widget modules that used to be qmake .pri includes
# (SlidingStackedWidget, WebEnginePlayer, PlaylistDownloadWidget, RateApp).
# Version metadata is generated from CMake, never hand-written.

configure_file(version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h" @ONLY)

qt_add_executable(playlist-dl
    main.cpp
    about.cpp
    about.h
    about.ui
    account.cpp
    account.h
    account.ui
    circularprogessbar.cpp
    circularprogessbar.h
    claimoffer.cpp
    claimoffer.h
    claimoffer.ui
    elidedlabel.cpp
    elidedlabel.h
    engine.cpp
    engine.h
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    onlinesearchsuggestion.cpp
    onlinesearchsuggestion.h
    playlistdownloadoptions.cpp
    playlistdownloadoptions.h
    playlistdownloadoptions.ui
    playlistitem.cpp
    playlistitem.h
    playlistitem.ui
    playlistsearch.cpp
    playlistsearch.h
    playlistsearch.ui
    playlistview.cpp
    playlistview.h
    playlistview.ui
    qadvancedslider.cpp
    qadvancedslider.h
    remotepixmaplabel2.cpp
    remotepixmaplabel2.h
    request.cpp
    request.h
    rungaurd.cpp
    rungaurd.h
    scrolltext.cpp
    scrolltext.h
    settingwidget.cpp
    settingwidget.h
    settingwidget.ui
    spinner.cpp
    spinner.h
    spinner.ui
    utils.cpp
    utils.h
    videoitem.cpp
    videoitem.h
    videoitem.ui
    waitingspinnerwidget.cpp
    waitingspinnerwidget.h

    # SlidingStackedWidget
    SlidingStackedWidget/slidingstackedwidget.cpp
    SlidingStackedWidget/slidingstackedwidget.h

    # WebEnginePlayer
    WebEnginePlayer/blocked/blocked.cpp
    WebEnginePlayer/blocked/blocked.h
    WebEnginePlayer/blocked/blocked.ui
    WebEnginePlayer/fullscreennotification.cpp
    WebEnginePlayer/fullscreennotification.h
    WebEnginePlayer/fullscreenwindow.cpp
    WebEnginePlayer/fullscreenwindow.h
    WebEnginePlayer/toolbar.cpp
    WebEnginePlayer/toolbar.h
    WebEnginePlayer/toolbar.ui
    WebEnginePlayer/webengineplayer.cpp
    WebEnginePlayer/webengineplayer.h

    # PlaylistDownloadWidget
    PlaylistDownloadWidget/downloaditem.cpp
    PlaylistDownloadWidget/downloaditem.h
    PlaylistDownloadWidget/downloaditem.ui
    PlaylistDownloadWidget/downloadmanager.cpp
    PlaylistDownloadWidget/downloadmanager.h
    PlaylistDownloadWidget/downloadprocess.cpp
    PlaylistDownloadWidget/downloadprocess.h
    PlaylistDownloadWidget/downloadwidget.cpp
    PlaylistDownloadWidget/downloadwidget.h
    PlaylistDownloadWidget/downloadwidget.ui
    PlaylistDownloadWidget/helper.cpp
    PlaylistDownloadWidget/helper.h
    PlaylistDownloadWidget/playlistentryitem.cpp
    PlaylistDownloadWidget/playlistentryitem.h
    PlaylistDownloadWidget/playlistentryitem.ui
    PlaylistDownloadWidget/playlistinfo.cpp
    PlaylistDownloadWidget/playlistinfo.h
    PlaylistDownloadWidget/playlistinfo.ui
    PlaylistDownloadWidget/playlistitemoverlay.ui

    # RateApp
    RateApp/rateapp.cpp
    RateApp/rateapp.h
    RateApp/rateapp.ui
)
pldl_set_warnings(playlist-dl)

# Resources. The .qrc files are kept as the single source of truth for the
# icon set (several hundred entries); rcc runs through AUTORCC.
set_target_properties(playlist-dl PROPERTIES AUTORCC ON)
target_sources(playlist-dl PRIVATE
    icons.qrc
    WebEnginePlayer/css.qrc
    WebEnginePlayer/js.qrc
)

# The qmake .pri modules each added their own directory to INCLUDEPATH, and
# the sources include each other by bare file name.
target_include_directories(playlist-dl PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/SlidingStackedWidget"
    "${CMAKE_CURRENT_SOURCE_DIR}/WebEnginePlayer"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlaylistDownloadWidget"
    "${CMAKE_CURRENT_SOURCE_DIR}/RateApp"
)

target_link_libraries(playlist-dl PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)

if(LINUX)
    # QtWebEngine drags in libproxy, whose backends are not always resolvable at
    # link time in the KDE Qt6 content snaps; those symbols come from shared
    # libraries and resolve at runtime.
    target_link_options(playlist-dl PRIVATE -Wl,--allow-shlib-undefined)
endif()

set_target_properties(playlist-dl PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER org.keshavnrj.playlist-dl
    MACOSX_BUNDLE_BUNDLE_NAME "Playlist DL"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    # Pure QtWidgets app: no QML anywhere, skip the qmlimportscanner finalizer.
    QT_QML_MODULE_NO_IMPORT_SCAN TRUE
)

install(TARGETS playlist-dl
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
