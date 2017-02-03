TARGET = QtOtaUpdate
QT = core

MODULE = qtotaupdate
load(qt_module)

CONFIG -= create_cmake

INCLUDEPATH += \
    $$[QT_SYSROOT]/usr/include/ostree-1/ \
    $$[QT_SYSROOT]/usr/include/glib-2.0/ \
    $$[QT_SYSROOT]/usr/lib/glib-2.0/include

LIBS += -lostree-1 -lgio-2.0 -lglib-2.0 -lgobject-2.0

HEADERS += \
    qotaclient.h \
    qotaclientasync_p.h \
    qotaclient_p.h \
    qotarepositoryconfig.h \
    qotarepositoryconfig_p.h

SOURCES += \
    qotaclient.cpp \
    qotarepositoryconfig.cpp

NO_PCH_SOURCES += \
    qotaclientasync.cpp
