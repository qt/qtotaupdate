CXX_MODULE = qml
TARGET = qtotaupdateplugin
TARGETPATH = QtOTAUpdate/
IMPORT_VERSION = 1.0

QT += qml qtotaupdate

SOURCES += pluginmain.cpp

OTHER_FILES += qmldir

load(qml_plugin)
