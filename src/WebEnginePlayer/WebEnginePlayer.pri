
INCLUDEPATH += $$PWD

#skip compilation of js and css
QTQUICK_COMPILER_SKIPPED_RESOURCES += $$PWD/js.qrc
QTQUICK_COMPILER_SKIPPED_RESOURCES += $$PWD/css.qrc

HEADERS += \
    $$PWD/blocked/blocked.h \
    $$PWD/fullscreennotification.h \
    $$PWD/fullscreenwindow.h \
    $$PWD/toolbar.h \
    $$PWD/webengineplayer.h

SOURCES += \
    $$PWD/blocked/blocked.cpp \
    $$PWD/fullscreennotification.cpp \
    $$PWD/fullscreenwindow.cpp \
    $$PWD/toolbar.cpp \
    $$PWD/webengineplayer.cpp

RESOURCES += \
    $$PWD/css.qrc \
    $$PWD/js.qrc

FORMS += \
    $$PWD/blocked/blocked.ui \
    $$PWD/toolbar.ui
