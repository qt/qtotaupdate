TARGET = QtOTAUpdate
QT = core

load(qt_module)

INCLUDEPATH += \
    $$[QT_SYSROOT]/usr/include/ostree-1/ \
    $$[QT_SYSROOT]/usr/include/glib-2.0/ \
    $$[QT_SYSROOT]/usr/lib/glib-2.0/include

LIBS += -lostree-1 -lgio-2.0 -lglib-2.0 -lgobject-2.0

HEADERS += \
    qotaclient.h \
    qotaclientasync_p.h \
    qotaclient_p.h

SOURCES += \
    qotaclient.cpp \
    qotaclientasync.cpp
