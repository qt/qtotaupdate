TEMPLATE = aux

build_online_docs: {
    QMAKE_DOCS_TARGETDIR = qtota
    QMAKE_DOCS = $$PWD/config/qtota-online.qdocconf
} else {
    QMAKE_DOCS = $$PWD/config/qtota.qdocconf
}

QMAKE_DOCS_OUTPUTDIR = $$OUT_PWD/qtota
