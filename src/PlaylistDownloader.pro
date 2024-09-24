#-------------------------------------------------
#
# Project created by QtCreator 2021-04-18T19:42:20
#
#-------------------------------------------------

QT       += core gui network webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = playlist-dl
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# No debug output in release mode
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

# Set program version
VERSION = 2.2
DEFINES += VERSIONSTR=\\\"$${VERSION}\\\"


# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

include(SlidingStackedWidget/SlidingStackedWidget.pri)
include(WebEnginePlayer/WebEnginePlayer.pri)
include(PlaylistDownloadWidget/PlaylistDownloadWidget.pri)
include(RateApp/RateApp.pri)


SOURCES += \
        account.cpp \
        circularprogessbar.cpp \
        claimoffer.cpp \
        qadvancedslider.cpp \
        about.cpp \
        elidedlabel.cpp \
        engine.cpp \
        main.cpp \
        mainwindow.cpp \
        onlinesearchsuggestion.cpp \
        playlistdownloadoptions.cpp \
        playlistitem.cpp \
        playlistsearch.cpp \
        playlistview.cpp \
        remotepixmaplabel2.cpp \
        request.cpp \
        rungaurd.cpp \
        scrolltext.cpp \
        settingwidget.cpp \
        spinner.cpp \
        utils.cpp \
        videoitem.cpp \
        waitingspinnerwidget.cpp

HEADERS += \
        account.h \
        circularprogessbar.h \
        claimoffer.h \
        qadvancedslider.h \
        about.h \
        elidedlabel.h \
        engine.h \
        mainwindow.h \
        onlinesearchsuggestion.h \
        playlistdownloadoptions.h \
        playlistitem.h \
        playlistsearch.h \
        playlistview.h \
        remotepixmaplabel2.h \
        request.h \
        rungaurd.h \
        scrolltext.h \
        settingwidget.h \
        spinner.h \
        utils.h \
        videoitem.h \
        waitingspinnerwidget.h

FORMS += \
        about.ui \
        account.ui \
        claimoffer.ui \
        mainwindow.ui \
        playlistdownloadoptions.ui \
        playlistitem.ui \
        playlistsearch.ui \
        playlistview.ui \
        settingwidget.ui \
        spinner.ui \
        videoitem.ui

# Default rules for deployment.
isEmpty(PREFIX){
 PREFIX = /usr
}

BINDIR  = $$PREFIX/bin
DATADIR = $$PREFIX/share

target.path = $$BINDIR

icon.files = icons/playlist-dl.png
icon.path = $$DATADIR/icons/hicolor/512x512/apps/

desktop.files = playlist-dl.desktop
desktop.path = $$DATADIR/applications/

INSTALLS += target icon desktop

RESOURCES += \
    icons.qrc
