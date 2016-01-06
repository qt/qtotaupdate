TEMPLATE = aux

build_online_docs: {
    QMAKE_DOCS_TARGETDIR = qtautomotiveota
    QMAKE_DOCS = $$PWD/config/qtautomotiveota-online.qdocconf
} else {
    QMAKE_DOCS = $$PWD/config/qtautomotiveota.qdocconf
}

QMAKE_DOCS_OUTPUTDIR = $$OUT_PWD/qtautomotiveota
